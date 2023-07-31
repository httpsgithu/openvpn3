#include <iostream>
#include "test_common.h"

/* if initproces.hpp is not included, mingw/windows compilation fails with
 * weird stack struct creation related errors in OpenSSL */
#include <openvpn/init/initprocess.hpp>
#include <openvpn/time/asiotimer.hpp>

#include <client/ovpncli.hpp>
#include <openvpn/client/cliopt.hpp>

using namespace openvpn;

/* The config parser checks for valid certificates, provide valid ones */
std::string dummysecp256cert = "-----BEGIN CERTIFICATE-----\n"
                               "MIIBETCBuAIJAImY2B4ODlQuMAoGCCqGSM49BAMCMBExDzANBgNVBAMMBnNlcnZl\n"
                               "cjAeFw0yMjA4MzAxNTA3NDJaFw0zMjA4MjcxNTA3NDJaMBExDzANBgNVBAMMBnNl\n"
                               "cnZlcjBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABDwU0GWKxTxYXP/L448OlaQr\n"
                               "fhF2p83eg/55LJB7Aiq7xckQImGa3w2heo01hFQXQ/4mK3wsLZr7ZZl7IDC4hhMw\n"
                               "CgYIKoZIzj0EAwIDSAAwRQIhAKDmwivsD4qjRtbaXmUNc3src6oFOCus32ZRZw0p\n"
                               "Oz9zAiBZ47YdsJ985ID5COg1+nCKk+0d7jWjICbPcODHyzH4fg==\n"
                               "-----END CERTIFICATE-----\n";

std::string dummysecp256key = "-----BEGIN PRIVATE KEY-----\n"
                              "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgbzZUYL0jZM05vB2O\n"
                              "kIKcA1OxSKw9ZVQ8UnlUCf6l/8ChRANCAAQ8FNBlisU8WFz/y+OPDpWkK34RdqfN\n"
                              "3oP+eSyQewIqu8XJECJhmt8NoXqNNYRUF0P+Jit8LC2a+2WZeyAwuIYT\n"
                              "-----END PRIVATE KEY-----\n";

std::string certconfig = "<ca>\n"
                         + dummysecp256cert + "</ca>\n"
                         + "<cert>\n"
                         + dummysecp256cert + "</cert>\n"
                         + "<key>\n" + dummysecp256key
                         + "</key>\n";

std::string minimalConfig = certconfig + "\n"
                            + "client\n"
                              "remote wooden.box\n";

void load_client_config(const std::string &config_content)
{
    OptionList options;
    ClientOptions::Config config;
    config.proto_context_options.reset(new ProtoContextOptions());

    ClientAPI::OpenVPNClientHelper client_helper;

    ClientAPI::Config api_config;
    api_config.content = config_content;
    auto eval = client_helper.eval_config(api_config);
    config.dco_compatible = eval.dcoCompatible;

    ParseClientConfig conf = ParseClientConfig::parse(config_content);

    auto parsed_config = ParseClientConfig::parse(config_content, nullptr, options);

    ClientOptions cliopt(options, config);
}

TEST(config, missingRequiredOption)
{
    ParseClientConfig conf = ParseClientConfig::parse("mode server");
    EXPECT_EQ(conf.error(), true);
    EXPECT_EQ(conf.message(), "ERR_PROFILE_OPTION: option_error: remote option not specified");
}

TEST(config, parse_missing_option)
{
    OVPN_EXPECT_THROW(
        load_client_config(std::string("remote wooden.box\n") + "mode server"
                           + "\n<ca>\n" + dummysecp256cert + "</ca>\n"),
        option_error,
        "option 'cert' not found");
}

TEST(config, parse_forbidden_option)
{
    OVPN_EXPECT_THROW(
        load_client_config(minimalConfig + "mode server"),
        option_error,
        "Only 'mode p2p' supported");

    OVPN_EXPECT_THROW(
        load_client_config(minimalConfig + "fragment"),
        option_error,
        "sorry, 'fragment' directive is not supported");
}

TEST(config, parse_unknown_option)
{
    OVPN_EXPECT_THROW(
        load_client_config(minimalConfig + "bikeshed-color green"),
        option_error,
        "sorry, unsupported options present in configuration: UNKNOWN/UNSUPPORTED OPTIONS");
}

TEST(config, duplicate_option)
{
    /* A duplicate option should not cause our parser to fail */
    load_client_config(minimalConfig + "cipher AES-192-CBC\ncipher AES-256-GCM\n");
}

TEST(config, parse_ignore_unkown)
{
    /* Bikeshed colour is ignored should throw no error */
    load_client_config(minimalConfig + "ignore-unknown-option bikeshed-colour bikeshed-color\n"
                                       "ignore-unknown-option danish axe phk\n"
                                       "bikeshed-colour green");

    load_client_config(minimalConfig + "setenv opt bikeshed-paint silver with sparkling");
}

TEST(config, ignore_warning_option)
{
    load_client_config(minimalConfig + "tun-ipv6\n");
    load_client_config(minimalConfig + "opt-verify\n");
}

TEST(config, parse_management)
{
    OVPN_EXPECT_THROW(
        load_client_config(minimalConfig + "management-is-blue"),
        option_error,
        "OpenVPN management interface is not supported by this client");

    OVPN_EXPECT_THROW(
        load_client_config(minimalConfig + "management"),
        option_error,
        "OpenVPN management interface is not supported by this client");
}

TEST(config, duplicate_options_sets)
{
    /* Do the whole dance to get a ClientOption object to access the list */
    OptionList options;
    ClientOptions::Config config;
    config.dco = false;
    config.proto_context_options = new ProtoContextOptions();

    ClientAPI::OpenVPNClientHelper client_helper;

    ParseClientConfig conf = ParseClientConfig::parse(minimalConfig);

    auto parsed_config = ParseClientConfig::parse(minimalConfig, nullptr, options);

    ClientOptions cliopt(options, config);

    std::vector<std::unordered_set<std::string>> allsets = {
        cliopt.settings_feature_not_implemented_fatal,
        cliopt.settings_feature_not_implemented_warn,
        cliopt.settings_ignoreSilently,
        cliopt.settings_ignoreWithWarning,
        cliopt.settings_pushonlyoptions,
        cliopt.settings_removedOptions,
        cliopt.settings_serverOnlyOptions,
        cliopt.settings_standalone_options};

    std::unordered_set<std::string> allOptions;

    for (auto set : allsets)
    {
        for (auto optname : set)
        {
            /* Use an expection instead of an assert to get the name of the option
             * that is a duplicate */
            if (!(allOptions.find(optname) == allOptions.end()))
                throw std::runtime_error("duplicate element: " + optname);
            allOptions.insert(optname);
        }
    }
}

TEST(config, dco_compatibility)
{
    for (auto &optname : ClientAPI::OpenVPNClientHelper::dco_incompatible_opts)
    {
        OVPN_EXPECT_THROW(
            load_client_config(minimalConfig + optname),
            option_error,
            "option_error: dco_compatibility: config/options are not compatible with dco");
    }
}

TEST(config, client_missing_in_config)
{
    std::string configNoClient = certconfig + "\nremote 1.2.3.4\n";
    OVPN_EXPECT_THROW(
        load_client_config(configNoClient),
        option_error,
        "option_error: Neither 'client' nor both 'tls-client' and 'pull' options declared. OpenVPN3 client only supports --client mode.");
}

TEST(config, pull_and_tlsclient_in_config)
{
    std::string configNoClient = certconfig + "\nremote 1.2.3.4\ntls-client\npull\n";
    /* Should not trigger an error, even without --client in place */
    load_client_config(configNoClient);
}

TEST(config, pull_and_client_and_tlsclient_in_config)
{
    std::string configNoClient = certconfig + "\nremote 1.2.3.4\ntls-client\npull\nclient\n";
    /* Should not trigger an error. Redundant options are no problem */
    load_client_config(configNoClient);
}

TEST(config, onlypullortlsclient)
{
    std::array<std::string, 2> options{"tls-client", "pull"};

    for (const auto &opt : options)
    {
        std::string configNoClient = certconfig + "\nremote 1.2.3.4\n" + opt + "\n";
        OVPN_EXPECT_THROW(
            load_client_config(configNoClient),
            option_error,
            "option_error: Neither 'client' nor both 'tls-client' and 'pull' options declared. OpenVPN3 client only supports --client mode.");
    }
}