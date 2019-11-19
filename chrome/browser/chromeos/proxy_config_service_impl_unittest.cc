// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/proxy_config_service_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

// TODO(stevenjb): Refactor and move this to src/chromeos/network/proxy or
// rename. This is really more of an integration test than a unit test at this
// point and currently relies on some chrome specific components.

using content::BrowserThread;

namespace chromeos {

namespace {

enum class Mode {
  kDirect,
  kAutoDetect,
  kPac,
  kSingleProxy,
  kPerSchemeProxy,
};

struct Input {
  Mode mode;
  std::string pac_url;
  std::string server;
  std::string bypass_rules;
};

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) base::StringPrintf("at line %d <%s>", __LINE__, desc)

// Inspired from net/proxy_resolution/proxy_config_service_linux_unittest.cc.
const struct TestParams {
  // Short description to identify the test
  std::string description;

  Input input;

  // Expected outputs from fields of net::ProxyConfig (via IO).
  bool auto_detect;
  GURL pac_url;
  net::ProxyRulesExpectation proxy_rules;
} tests[] = {
    {
        // 0
        TEST_DESC("No proxying"),

        {
            // Input.
            Mode::kDirect,  // mode
        },

        // Expected result.
        false,                                // auto_detect
        GURL(),                               // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 1
        TEST_DESC("Auto detect"),

        {
            // Input.
            Mode::kAutoDetect,  // mode
        },

        // Expected result.
        true,                                 // auto_detect
        GURL(),                               // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 2
        TEST_DESC("Valid PAC URL"),

        {
            // Input.
            Mode::kPac,              // mode
            "http://wpad/wpad.dat",  // pac_url
        },

        // Expected result.
        false,                                // auto_detect
        GURL("http://wpad/wpad.dat"),         // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 3
        TEST_DESC("Invalid PAC URL"),

        {
            // Input.
            Mode::kPac,  // mode
            "wpad.dat",  // pac_url
        },

        // Expected result.
        false,                                // auto_detect
        GURL(),                               // pac_url
        net::ProxyRulesExpectation::Empty(),  // proxy_rules
    },

    {
        // 4
        TEST_DESC("Single-host in proxy list"),

        {
            // Input.
            Mode::kSingleProxy,  // mode
            "",                  // pac_url
            "www.google.com",    // server
        },

        // Expected result.
        false,                               // auto_detect
        GURL(),                              // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:80",             // single proxy
            "<local>"),                      // bypass rules
    },

    {
        // 5
        TEST_DESC("Single-host, different port"),

        {
            // Input.
            Mode::kSingleProxy,   // mode
            "",                   // pac_url
            "www.google.com:99",  // server
        },

        // Expected result.
        false,                               // auto_detect
        GURL(),                              // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:99",             // single
            "<local>"),                      // bypass rules
    },

    {
        // 6
        TEST_DESC("Tolerate a scheme"),

        {
            // Input.
            Mode::kSingleProxy,          // mode
            "",                          // pac_url
            "http://www.google.com:99",  // server
        },

        // Expected result.
        false,                               // auto_detect
        GURL(),                              // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:99",             // single proxy
            "<local>"),                      // bypass rules
    },

    {
        // 7
        TEST_DESC("Per-scheme proxy rules"),

        {
            // Input.
            Mode::kPerSchemeProxy,  // mode
            "",                     // pac_url
            "http=www.google.com:80;https=https://www.foo.com:110;"
            "ftp=ftp.foo.com:121;socks=socks5://socks.com:888",  // server
        },

        // Expected result.
        false,                                           // auto_detect
        GURL(),                                          // pac_url
        net::ProxyRulesExpectation::PerSchemeWithSocks(  // proxy_rules
            "www.google.com:80",                         // http
            "https://www.foo.com:110",                   // https
            "ftp.foo.com:121",                           // ftp
            "socks5://socks.com:888",                    // fallback proxy
            "<local>"),                                  // bypass rules
    },

    {
        // 8
        TEST_DESC("Bypass rules"),

        {
            // Input.
            Mode::kSingleProxy,  // mode
            "",                  // pac_url
            "www.google.com",    // server
            "*.google.com, *foo.com:99, 1.2.3.4:22, 127.0.0.1/8",
            // bypass_rules
        },

        // Expected result.
        false,                               // auto_detect
        GURL(),                              // pac_url
        net::ProxyRulesExpectation::Single(  // proxy_rules
            "www.google.com:80",             // single proxy
                                             // bypass_rules
            "<local>,*.google.com,*foo.com:99,1.2.3.4:22,127.0.0.1/8"),
    },
};  // tests

const char kEthernetPolicy[] =
    "    { \"GUID\": \"{485d6076-dd44-6b6d-69787465725f5040}\","
    "      \"Type\": \"Ethernet\","
    "      \"Name\": \"MyEthernet\","
    "      \"Ethernet\": {"
    "        \"Authentication\": \"None\" },"
    "      \"ProxySettings\": {"
    "        \"PAC\": \"http://domain.com/x\","
    "        \"Type\": \"PAC\" }"
    "    }";

const char kUserProfilePath[] = "user_profile";

}  // namespace

class ProxyConfigServiceImplTest : public testing::Test {
 protected:
  ProxyConfigServiceImplTest() = default;

  void SetUp() override {
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();

    PrefProxyConfigTrackerImpl::RegisterPrefs(pref_service_.registry());
    ::onc::RegisterPrefs(pref_service_.registry());
    PrefProxyConfigTrackerImpl::RegisterProfilePrefs(profile_prefs_.registry());
    ::onc::RegisterProfilePrefs(profile_prefs_.registry());
  }

  void SetUpProxyConfigService(PrefService* profile_prefs) {
    config_service_impl_.reset(new ProxyConfigServiceImpl(
        profile_prefs, &pref_service_,
        base::CreateSingleThreadTaskRunner({BrowserThread::IO})));
    proxy_config_service_ =
        config_service_impl_->CreateTrackingProxyConfigService(
            std::unique_ptr<net::ProxyConfigService>());

    // CreateTrackingProxyConfigService triggers update of initial prefs proxy
    // config by tracker to chrome proxy config service, so flush all pending
    // tasks so that tests start fresh.
    base::RunLoop().RunUntilIdle();
  }

  void SetUpPrivateWiFi() {
    ShillProfileClient::TestInterface* profile_test =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

    // Process any pending notifications before clearing services.
    base::RunLoop().RunUntilIdle();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUserProfilePath, "user_hash");

    service_test->AddService("/service/stub_wifi2",
                             "stub_wifi2" /* guid */,
                             "wifi2_PSK",
                             shill::kTypeWifi, shill::kStateOnline,
                             true /* visible */);
    profile_test->AddService(kUserProfilePath, "/service/stub_wifi2");

    base::RunLoop().RunUntilIdle();
  }

  void SetUpSharedEthernet() {
    ShillProfileClient::TestInterface* profile_test =
        DBusThreadManager::Get()->GetShillProfileClient()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();

    // Process any pending notifications before clearing services.
    base::RunLoop().RunUntilIdle();
    service_test->ClearServices();

    // Sends a notification about the added profile.
    profile_test->AddProfile(kUserProfilePath, "user_hash");

    service_test->AddService("/service/ethernet", "stub_ethernet" /* guid */,
                             "eth0", shill::kTypeEthernet, shill::kStateOnline,
                             true /* visible */);
    profile_test->AddService(NetworkProfileHandler::GetSharedProfilePath(),
                             "/service/ethernet");

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    config_service_impl_->DetachFromPrefService();
    base::RunLoop().RunUntilIdle();
    config_service_impl_.reset();
    proxy_config_service_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
  }

  base::Value InitConfigWithTestInput(const Input& input) {
    switch (input.mode) {
      case Mode::kDirect:
        return ProxyConfigDictionary::CreateDirect();
      case Mode::kAutoDetect:
        return ProxyConfigDictionary::CreateAutoDetect();
      case Mode::kPac:
        return ProxyConfigDictionary::CreatePacScript(input.pac_url, false);
      case Mode::kSingleProxy:
      case Mode::kPerSchemeProxy:
        return ProxyConfigDictionary::CreateFixedServers(input.server,
                                                         input.bypass_rules);
    }
    NOTREACHED();
    return base::Value();
  }

  void SetUserConfigInShill(const base::Value* pref_proxy_config_dict) {
    std::string proxy_config;
    if (pref_proxy_config_dict)
      base::JSONWriter::Write(*pref_proxy_config_dict, &proxy_config);

    NetworkStateHandler* network_state_handler =
        NetworkHandler::Get()->network_state_handler();
    const NetworkState* network = network_state_handler->DefaultNetwork();
    ASSERT_TRUE(network);
    DBusThreadManager::Get()
        ->GetShillServiceClient()
        ->GetTestInterface()
        ->SetServiceProperty(network->path(), shill::kProxyConfigProperty,
                             base::Value(proxy_config));
  }

  // Synchronously gets the latest proxy config.
  void SyncGetLatestProxyConfig(net::ProxyConfigWithAnnotation* config) {
    *config = net::ProxyConfigWithAnnotation();
    // Let message loop process all messages. This will run
    // ChromeProxyConfigService::UpdateProxyConfig, which is posted on IO from
    // PrefProxyConfigTrackerImpl::OnProxyConfigChanged.
    base::RunLoop().RunUntilIdle();
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_->GetLatestProxyConfig(config);

    EXPECT_EQ(net::ProxyConfigService::CONFIG_VALID, availability);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<ProxyConfigServiceImpl> config_service_impl_;
  TestingPrefServiceSimple pref_service_;
  sync_preferences::TestingPrefServiceSyncable profile_prefs_;

 private:
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

TEST_F(ProxyConfigServiceImplTest, NetworkProxy) {
  SetUpPrivateWiFi();
  // Create a ProxyConfigServiceImpl like for the system request context.
  SetUpProxyConfigService(nullptr /* no profile prefs */);
  for (size_t i = 0; i < base::size(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "] %s", i,
                                    tests[i].description.c_str()));

    base::Value test_config = InitConfigWithTestInput(tests[i].input);
    SetUserConfigInShill(&test_config);

    net::ProxyConfigWithAnnotation config;
    SyncGetLatestProxyConfig(&config);

    EXPECT_EQ(tests[i].auto_detect, config.value().auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.value().pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.value().proxy_rules()));
  }
}

TEST_F(ProxyConfigServiceImplTest, DynamicPrefsOverride) {
  SetUpPrivateWiFi();
  // Create a ProxyConfigServiceImpl like for the system request context.
  SetUpProxyConfigService(nullptr /* no profile prefs */);
  // Groupings of 3 test inputs to use for managed, recommended and network
  // proxies respectively.  Only valid and non-direct test inputs are used.
  const size_t proxies[][3] = {
    { 1, 2, 4, },
    { 1, 4, 2, },
    { 4, 2, 1, },
    { 2, 1, 4, },
    { 2, 4, 5, },
    { 2, 5, 4, },
    { 5, 4, 2, },
    { 4, 2, 5, },
    { 4, 5, 6, },
    { 4, 6, 5, },
    { 6, 5, 4, },
    { 5, 4, 6, },
    { 5, 6, 7, },
    { 5, 7, 6, },
    { 7, 6, 5, },
    { 6, 5, 7, },
    { 6, 7, 8, },
    { 6, 8, 7, },
    { 8, 7, 6, },
    { 7, 6, 8, },
  };
  for (size_t i = 0; i < base::size(proxies); ++i) {
    const TestParams& managed_params = tests[proxies[i][0]];
    const TestParams& recommended_params = tests[proxies[i][1]];
    const TestParams& network_params = tests[proxies[i][2]];

    SCOPED_TRACE(base::StringPrintf(
        "Test[%" PRIuS "] managed=[%s], recommended=[%s], network=[%s]", i,
        managed_params.description.c_str(),
        recommended_params.description.c_str(),
        network_params.description.c_str()));

    base::Value managed_config = InitConfigWithTestInput(managed_params.input);
    base::Value recommended_config =
        InitConfigWithTestInput(recommended_params.input);
    base::Value network_config = InitConfigWithTestInput(network_params.input);

    // Managed proxy pref should take effect over recommended proxy and
    // non-existent network proxy.
    SetUserConfigInShill(nullptr);
    pref_service_.SetManagedPref(::proxy_config::prefs::kProxy,
                                 managed_config.CreateDeepCopy());
    pref_service_.SetRecommendedPref(::proxy_config::prefs::kProxy,
                                     recommended_config.CreateDeepCopy());
    net::ProxyConfigWithAnnotation actual_config;
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Recommended proxy pref should take effect when managed proxy pref is
    // removed.
    pref_service_.RemoveManagedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(recommended_params.auto_detect,
              actual_config.value().auto_detect());
    EXPECT_EQ(recommended_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(recommended_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Network proxy should take take effect over recommended proxy pref.
    SetUserConfigInShill(&network_config);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Managed proxy pref should take effect over network proxy.
    pref_service_.SetManagedPref(::proxy_config::prefs::kProxy,
                                 managed_config.CreateDeepCopy());
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(managed_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(managed_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(managed_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Network proxy should take effect over recommended proxy pref when managed
    // proxy pref is removed.
    pref_service_.RemoveManagedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));

    // Removing recommended proxy pref should have no effect on network proxy.
    pref_service_.RemoveRecommendedPref(::proxy_config::prefs::kProxy);
    SyncGetLatestProxyConfig(&actual_config);
    EXPECT_EQ(network_params.auto_detect, actual_config.value().auto_detect());
    EXPECT_EQ(network_params.pac_url, actual_config.value().pac_url());
    EXPECT_TRUE(network_params.proxy_rules.Matches(
        actual_config.value().proxy_rules()));
  }
}

// Tests whether the proxy settings from user policy are used for ethernet even
// if 'UseSharedProxies' is set to false.
// See https://crbug.com/454966 .
TEST_F(ProxyConfigServiceImplTest, SharedEthernetAndUserPolicy) {
  SetUpSharedEthernet();
  SetUpProxyConfigService(&profile_prefs_);

  std::unique_ptr<base::Value> ethernet_policy =
      chromeos::onc::ReadDictionaryFromJson(kEthernetPolicy);

  std::unique_ptr<base::ListValue> network_configs(new base::ListValue);
  network_configs->Append(std::move(ethernet_policy));

  profile_prefs_.SetUserPref(::proxy_config::prefs::kUseSharedProxies,
                             std::make_unique<base::Value>(false));
  profile_prefs_.SetManagedPref(::onc::prefs::kOpenNetworkConfiguration,
                                std::move(network_configs));

  net::ProxyConfigWithAnnotation actual_config;
  SyncGetLatestProxyConfig(&actual_config);
  net::ProxyConfigWithAnnotation expected_config(
      net::ProxyConfig::CreateFromCustomPacURL(GURL("http://domain.com/x")),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(expected_config.value().Equals(actual_config.value()));
}

}  // namespace chromeos
