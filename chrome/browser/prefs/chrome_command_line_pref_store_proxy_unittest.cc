// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <stddef.h>

#include <array>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "content/public/common/content_switches.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "url/gurl.h"

namespace {

// Test parameter object for testing command line proxy configuration.
struct CommandLineTestParams {
  // Short description to identify the test.
  const char* description;

  // The command line to build a ProxyConfig from.
  struct SwitchValue {
    const char* name;
    const char* value;
  };
  std::array<CommandLineTestParams::SwitchValue, 2> switches;

  // Expected outputs (fields of the ProxyConfig).
  bool is_null;
  bool auto_detect;
  std::string pac_url;
  net::ProxyRulesExpectation proxy_rules;
};

void PrintTo(const CommandLineTestParams& params, std::ostream* os) {
  *os << params.description;
}

static const auto kCommandLineTestParams =
    std::to_array<CommandLineTestParams>({
        CommandLineTestParams{
            "Empty command line",
            // Input
            {},
            // Expected result
            true,   // is_null
            false,  // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::Empty(),
        },
        CommandLineTestParams{
            "No proxy",
            // Input
            {{{switches::kNoProxyServer, nullptr}}},
            // Expected result
            false,  // is_null
            false,  // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::Empty(),
        },
        CommandLineTestParams{
            "No proxy with extra parameters.",
            // Input
            {{{switches::kNoProxyServer, nullptr},
              {switches::kProxyServer, "http://proxy:8888"}}},
            // Expected result
            false,  // is_null
            false,  // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::Empty(),
        },
        CommandLineTestParams{
            "Single proxy.",
            // Input
            {{{switches::kProxyServer, "http://proxy:8888"}}},
            // Expected result
            false,                                            // is_null
            false,                                            // auto_detect
            "",                                               // pac_url
            net::ProxyRulesExpectation::Single("proxy:8888",  // single proxy
                                               ""),           // bypass rules
        },
        CommandLineTestParams{
            "Per scheme proxy.",
            // Input
            {{{switches::kProxyServer,
               "http=httpproxy:8888;ftp=ftpproxy:8889"}}},
            // Expected result
            false,  // is_null
            false,  // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::PerScheme("httpproxy:8888",  // http
                                                  "",                // https
                                                  "ftpproxy:8889",   // ftp
                                                  ""),  // bypass rules
        },
        CommandLineTestParams{
            "Per scheme proxy with bypass URLs.",
            // Input
            {{{switches::kProxyServer, "http=httpproxy:8888;ftp=ftpproxy:8889"},
              {switches::kProxyBypassList,
               ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8"}}},
            // Expected result
            false,  // is_null
            false,  // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::PerScheme(
                "httpproxy:8888",  // http
                "",                // https
                "ftpproxy:8889",   // ftp
                "*.google.com,foo.com:99,1.2.3.4:22,127.0.0.1/8"),
        },
        CommandLineTestParams{
            "Pac URL",
            // Input
            {{{switches::kProxyPacUrl, "http://wpad/wpad.dat"}}},
            // Expected result
            false,                   // is_null
            false,                   // auto_detect
            "http://wpad/wpad.dat",  // pac_url
            net::ProxyRulesExpectation::Empty(),
        },
        CommandLineTestParams{
            "Autodetect",
            // Input
            {{{switches::kProxyAutoDetect, nullptr}}},
            // Expected result
            false,  // is_null
            true,   // auto_detect
            "",     // pac_url
            net::ProxyRulesExpectation::Empty(),
        },
    });

}  // namespace

class ChromeCommandLinePrefStoreProxyTest
    : public testing::TestWithParam<CommandLineTestParams> {
 protected:
  ChromeCommandLinePrefStoreProxyTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  net::ProxyConfigWithAnnotation* proxy_config() { return &proxy_config_; }

  void SetUp() override {
    for (const auto& switch_value : GetParam().switches) {
      if (switch_value.name && switch_value.value) {
        command_line_.AppendSwitchASCII(switch_value.name, switch_value.value);
      } else if (switch_value.name) {
        command_line_.AppendSwitch(switch_value.name);
      }
    }
    scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
    PrefProxyConfigTrackerImpl::RegisterPrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_command_line_prefs(
        new ChromeCommandLinePrefStore(&command_line_));
    pref_service_ = factory.Create(registry.get());
    PrefProxyConfigTrackerImpl::ReadPrefConfig(pref_service_.get(),
                                               &proxy_config_);
  }

 private:
  base::CommandLine command_line_;
  std::unique_ptr<PrefService> pref_service_;
  net::ProxyConfigWithAnnotation proxy_config_;
};

TEST_P(ChromeCommandLinePrefStoreProxyTest, CommandLine) {
  EXPECT_EQ(GetParam().auto_detect, proxy_config()->value().auto_detect());
  EXPECT_EQ(GURL(GetParam().pac_url), proxy_config()->value().pac_url());
  EXPECT_TRUE(
      GetParam().proxy_rules.Matches(proxy_config()->value().proxy_rules()));
}

INSTANTIATE_TEST_SUITE_P(ChromeCommandLinePrefStoreProxyTestInstance,
                         ChromeCommandLinePrefStoreProxyTest,
                         testing::ValuesIn(kCommandLineTestParams));
