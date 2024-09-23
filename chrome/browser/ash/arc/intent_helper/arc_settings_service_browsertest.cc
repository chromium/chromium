// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/arc_settings_service.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_backup_settings_instance.h"
#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/handlers/configuration_policy_handler_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/browser_test.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Return;

namespace arc {

namespace {
constexpr char kONCPolicy[] =
    "{ \"NetworkConfigurations\": ["
    "    { \"GUID\": \"stub_ethernet_guid\","
    "      \"Type\": \"Ethernet\","
    "      \"Name\": \"My Ethernet\","
    "      \"Ethernet\": {"
    "        \"Authentication\": \"None\" },"
    "      \"ProxySettings\": {"
    "        \"PAC\": \"http://domain.com/x\","
    "        \"Type\": \"PAC\" }"
    "    }"
    "  ],"
    "  \"Type\": \"UnencryptedConfiguration\""
    "}";

constexpr char kDeviceONCPolicy[] =
    "{"
    "   \"GlobalNetworkConfiguration\": {"
    "      \"AllowOnlyPolicyNetworksToAutoconnect\": false,"
    "      \"AllowOnlyPolicyNetworksToConnect\": false"
    "   },"
    "   \"NetworkConfigurations\": [ {"
    "      \"GUID\": \"{wifi1_guid}\","
    "      \"Name\": \"wifi1\","
    "      \"ProxySettings\": {"
    "         \"Manual\": {"
    "            \"FTPProxy\": {"
    "               \"Host\": \"proxy\","
    "               \"Port\": 5000"
    "            },"
    "            \"HTTPProxy\": {"
    "               \"Host\": \"proxy\","
    "               \"Port\": 5000"
    "            },"
    "            \"SOCKS\": {"
    "               \"Host\": \"proxy\","
    "               \"Port\": 5000"
    "            },"
    "            \"SecureHTTPProxy\": {"
    "               \"Host\": \"proxy\","
    "               \"Port\": 5000"
    "            }"
    "         },"
    "         \"Type\": \"Manual\""
    "      },"
    "      \"Type\": \"WiFi\","
    "      \"WiFi\": {"
    "         \"AutoConnect\": false,"
    "         \"HiddenSSID\": false,"
    "         \"SSID\": \"wifi1\","
    "         \"Security\": \"None\""
    "      }"
    "   } ]"
    "}";

constexpr char kUserONCPolicy[] =
    "{"
    "   \"NetworkConfigurations\": [ {"
    "      \"GUID\": \"{direct_guid}\","
    "      \"Name\": \"EAP-TTLS\","
    "      \"ProxySettings\": {"
    "         \"Type\": \"Direct\""
    "      },"
    "      \"Type\": \"WiFi\","
    "      \"WiFi\": {"
    "         \"AutoConnect\": false,"
    "         \"EAP\": {"
    "            \"Identity\": \"CrOS\","
    "            \"Inner\": \"Automatic\","
    "            \"Outer\": \"EAP-TTLS\","
    "            \"Password\": \"********\","
    "            \"Recommended\": ["
    "              \"AnonymousIdentity\","
    "              \"Identity\","
    "              \"Password\""
    "            ],"
    "            \"SaveCredentials\": true,"
    "            \"UseSystemCAs\": false"
    "         },"
    "         \"HiddenSSID\": false,"
    "        \"SSID\": \"direct_ssid\","
    "        \"Security\": \"WPA-EAP\""
    "     }"
    "  }, {"
    "      \"GUID\": \"{wifi0_guid}\","
    "      \"Name\": \"wifi0\","
    "      \"ProxySettings\": {"
    "         \"Manual\": {"
    "            \"FTPProxy\": {"
    "               \"Host\": \"proxy-n300\","
    "               \"Port\": 3000"
    "            },"
    "            \"HTTPProxy\": {"
    "               \"Host\": \"proxy-n300\","
    "               \"Port\": 3000"
    "            },"
    "            \"SOCKS\": {"
    "               \"Host\": \"proxy-n300\","
    "               \"Port\": 3000"
    "            },"
    "            \"SecureHTTPProxy\": {"
    "               \"Host\": \"proxy-n300\","
    "               \"Port\": 3000"
    "            }"
    "         },"
    "         \"Type\": \"Manual\""
    "      },"
    "      \"Type\": \"WiFi\","
    "      \"WiFi\": {"
    "         \"AutoConnect\": false,"
    "         \"HiddenSSID\": false,"
    "         \"SSID\": \"wifi0\","
    "         \"Security\": \"None\""
    "      }"
    "   } ]"
    "}";

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kDefaultServicePath[] = "stub_ethernet";

constexpr char kWifi0ServicePath[] = "stub_wifi0";
constexpr char kWifi0Ssid[] = "wifi0";
constexpr char kWifi0Guid[] = "{wifi0_guid}";

constexpr char kWifi1ServicePath[] = "stub_wifi1";
constexpr char kWifi1Ssid[] = "wifi1";
constexpr char kWifi1Guid[] = "{wifi1_guid}";

constexpr char kONCPacUrl[] = "http://domain.com/x";

constexpr char kSetProxyBroadcastAction[] =
    "org.chromium.arc.intent_helper.SET_PROXY";

// Returns the number of |broadcasts| having the proxy action, and checks that
// all their extras match with |extras|.
int CountProxyBroadcasts(
    const std::vector<FakeIntentHelperInstance::Broadcast>& broadcasts,
    const std::vector<base::Value::Dict*>& extras) {
  unsigned long count = 0;
  for (const FakeIntentHelperInstance::Broadcast& broadcast : broadcasts) {
    if (broadcast.action == kSetProxyBroadcastAction) {
      DCHECK(count < extras.size())
          << "The expected proxy broadcast count is smaller than "
             "the actual count.";
      EXPECT_EQ(*base::JSONReader::Read(broadcast.extras), *extras[count]);
      count++;
    }
  }
  return count;
}

void RunUntilIdle() {
  DCHECK(base::CurrentThread::Get());
  base::RunLoop loop;
  loop.RunUntilIdle();
}

}  // namespace

class ArcSettingsServiceTest : public InProcessBrowserTest {
 public:
  ArcSettingsServiceTest() {
    feature_list_.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
  }
  ArcSettingsServiceTest(const ArcSettingsServiceTest&) = delete;
  ArcSettingsServiceTest& operator=(const ArcSettingsServiceTest&) = delete;

  // InProcessBrowserTest:
  ~ArcSettingsServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    SetupNetworkEnvironment();
    RunUntilIdle();

    fake_intent_helper_instance_ = std::make_unique<FakeIntentHelperInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(fake_intent_helper_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->intent_helper());

    fake_backup_settings_instance_ =
        std::make_unique<FakeBackupSettingsInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->backup_settings()
        ->SetInstance(fake_backup_settings_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->backup_settings());
  }

  void TearDownOnMainThread() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->backup_settings()
        ->CloseInstance(fake_backup_settings_instance_.get());
    fake_backup_settings_instance_.reset();

    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->CloseInstance(fake_intent_helper_instance_.get());
    fake_intent_helper_instance_.reset();
  }

  void UpdatePolicy(const policy::PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    RunUntilIdle();
  }

 protected:
  void DisconnectNetworkService(const std::string& service_path) {
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();
    base::Value value(shill::kStateIdle);
    service_test->SetServiceProperty(service_path, shill::kStateProperty,
                                     value);
    RunUntilIdle();
  }

  void ConnectWifiNetworkService(const std::string& service_path,
                                 const std::string& guid,
                                 const std::string& ssid) {
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();

    service_test->AddService(service_path, guid, ssid, shill::kTypeWifi,
                             shill::kStateOnline, true /* add_to_visible */);

    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     base::Value(kUserProfilePath));
    RunUntilIdle();
  }

  void SetProxyConfigForNetworkService(const std::string& service_path,
                                       base::Value::Dict proxy_config) {
    ProxyConfigDictionary proxy_config_dict(std::move(proxy_config));
    const ash::NetworkState* network =
        ash::NetworkHandler::Get()->network_state_handler()->GetNetworkState(
            service_path);
    ASSERT_TRUE(network);
    ash::proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
  }

  std::unique_ptr<FakeIntentHelperInstance> fake_intent_helper_instance_;
  std::unique_ptr<FakeBackupSettingsInstance> fake_backup_settings_instance_;

 private:
  void SetupNetworkEnvironment() {
    ash::ShillProfileClient::TestInterface* profile_test =
        ash::ShillProfileClient::Get()->GetTestInterface();
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "user");

    service_test->ClearServices();

    service_test->AddService(kDefaultServicePath, "stub_ethernet_guid", "eth0",
                             shill::kTypeEthernet, shill::kStateOnline,
                             true /* add_to_visible */);
    service_test->SetServiceProperty(kDefaultServicePath,
                                     shill::kProfileProperty,
                                     base::Value(kUserProfilePath));
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, BackupRestorePolicyTest) {
  // The policy is initially set to user control.
  policy::PolicyMap policy;
  policy.Set(policy::key::kArcBackupRestoreServiceEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(static_cast<int>(
                 policy::ArcServicePolicyValue::kUnderUserControl)),
             nullptr);
  UpdatePolicy(policy);

  PrefService* const prefs = browser()->profile()->GetPrefs();

  // Set the user pref as initially enabled.
  prefs->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));

  fake_backup_settings_instance_->ClearCallHistory();

  // The policy is set to disabled.
  policy.Set(
      policy::key::kArcBackupRestoreServiceEnabled,
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(policy::ArcServicePolicyValue::kDisabled)),
      nullptr);
  UpdatePolicy(policy);

  // The pref is disabled and managed, but the corresponding sync method does
  // not reflect the pref as it is not dynamically applied.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
  EXPECT_EQ(0, fake_backup_settings_instance_->set_backup_enabled_count());
  EXPECT_FALSE(fake_backup_settings_instance_->enabled());
  EXPECT_FALSE(fake_backup_settings_instance_->managed());

  fake_backup_settings_instance_->ClearCallHistory();

  // The policy is set to user control.
  policy.Set(policy::key::kArcBackupRestoreServiceEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(static_cast<int>(
                 policy::ArcServicePolicyValue::kUnderUserControl)),
             nullptr);
  UpdatePolicy(policy);

  // The pref is unmanaged, but the corresponding sync method does not reflect
  // the pref as it is not dynamically applied.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
  EXPECT_EQ(0, fake_backup_settings_instance_->set_backup_enabled_count());
  EXPECT_FALSE(fake_backup_settings_instance_->enabled());
  EXPECT_FALSE(fake_backup_settings_instance_->managed());

  fake_backup_settings_instance_->ClearCallHistory();

  // The policy is set to enabled.
  policy.Set(
      policy::key::kArcBackupRestoreServiceEnabled,
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(policy::ArcServicePolicyValue::kEnabled)),
      nullptr);
  UpdatePolicy(policy);

  // The pref is enabled and managed, but the corresponding sync method does
  // not reflect the pref as it is not dynamically applied.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled));
  EXPECT_EQ(0, fake_backup_settings_instance_->set_backup_enabled_count());
  EXPECT_FALSE(fake_backup_settings_instance_->enabled());
  EXPECT_FALSE(fake_backup_settings_instance_->managed());
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, LocationServicePolicyTest) {
  // The policy is initially set to user control.
  policy::PolicyMap policy;
  policy.Set(policy::key::kArcGoogleLocationServicesEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(static_cast<int>(
                 policy::ArcServicePolicyValue::kUnderUserControl)),
             nullptr);
  UpdatePolicy(policy);

  PrefService* const prefs = browser()->profile()->GetPrefs();

  // Set the user pref as initially enabled.
  prefs->SetBoolean(prefs::kArcLocationServiceEnabled, true);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));

  fake_intent_helper_instance_->clear_broadcasts();

  // The policy is set to disabled.
  policy.Set(
      policy::key::kArcGoogleLocationServicesEnabled,
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(policy::ArcServicePolicyValue::kDisabled)),
      nullptr);
  UpdatePolicy(policy);

  // The pref is disabled and managed, but no broadcast is sent as the setting
  // is not dynamically applied.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kArcLocationServiceEnabled));
  EXPECT_EQ(0UL, fake_intent_helper_instance_->broadcasts().size());

  fake_intent_helper_instance_->clear_broadcasts();

  // The policy is set to user control.
  policy.Set(policy::key::kArcGoogleLocationServicesEnabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(static_cast<int>(
                 policy::ArcServicePolicyValue::kUnderUserControl)),
             nullptr);
  UpdatePolicy(policy);

  // The pref is unmanaged, but no broadcast is sent as the setting is not
  // dynamically applied.
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kArcLocationServiceEnabled));
  EXPECT_EQ(0UL, fake_intent_helper_instance_->broadcasts().size());

  // The policy is set to enabled.
  policy.Set(
      policy::key::kArcGoogleLocationServicesEnabled,
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
      policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(policy::ArcServicePolicyValue::kEnabled)),
      nullptr);
  UpdatePolicy(policy);

  // The pref is enabled and managed, but no broadcast is sent as the setting
  // is not dynamically applied.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kArcLocationServiceEnabled));
  EXPECT_EQ(0UL, fake_intent_helper_instance_->broadcasts().size());
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ProxyModePolicyTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kAutoDetectProxyModeName), nullptr);
  UpdatePolicy(policy);

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set("mode",
                            base::Value(ProxyPrefs::kAutoDetectProxyModeName));
  expected_proxy_config.Set("pacUrl", base::Value("http://wpad/wpad.dat"));
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ONCProxyPolicyTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kONCPolicy), nullptr);
  UpdatePolicy(policy);

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set("mode",
                            base::Value(ProxyPrefs::kPacScriptProxyModeName));
  expected_proxy_config.Set("pacUrl", base::Value(kONCPacUrl));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

// Test to verify that, when enabled, the local proxy address is synced instead
// of the real proxy set via policy.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest,
                       SystemProxyAddressForwardedTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kONCPolicy), nullptr);
  UpdatePolicy(policy);

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set("mode",
                            base::Value(ProxyPrefs::kPacScriptProxyModeName));
  expected_proxy_config.Set("pacUrl", base::Value(kONCPacUrl));

  // Set the user preference to indicate that ARC should connect to
  // System-proxy.
  browser()->profile()->GetPrefs()->Set(
      ::prefs::kSystemProxyUserTrafficHostAndPort,
      base::Value("local_proxy:3128"));
  RunUntilIdle();

  base::Value::Dict expected_proxy_config_system_proxy;
  expected_proxy_config_system_proxy.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config_system_proxy.Set("host", base::Value("local_proxy"));
  expected_proxy_config_system_proxy.Set("port", 3128);

  // Unset the System-proxy preference to verify that ARC syncs proxy configs
  // correctly when System-proxy is disabled.
  browser()->profile()->GetPrefs()->Set(
      ::prefs::kSystemProxyUserTrafficHostAndPort, base::Value(""));
  RunUntilIdle();

  EXPECT_EQ(CountProxyBroadcasts(
                fake_intent_helper_instance_->broadcasts(),
                {&expected_proxy_config, &expected_proxy_config_system_proxy,
                 &expected_proxy_config}),
            3);
}

// Test to verify that the address of the local proxy is not forwarded if
// there's no proxy set in the browser.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest,
                       SystemProxyAddressNotForwardedForDirectMode) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  // Apply ONC policy with direct proxy.
  policy.Set(policy::key::kDeviceOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_CLOUD, base::Value(kDeviceONCPolicy),
             nullptr);
  UpdatePolicy(policy);

  // Set the user preference to indicate that ARC should connect to
  // System-proxy.
  browser()->profile()->GetPrefs()->Set(
      ::prefs::kSystemProxyUserTrafficHostAndPort,
      base::Value("local_proxy:3128"));
  RunUntilIdle();

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set("mode",
                            base::Value(ProxyPrefs::kDirectProxyModeName));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

// Proxy policy has a higher priority than proxy default settings.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, TwoSourcesTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  // Proxy policy.
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kFixedServersProxyModeName), nullptr);
  policy.Set(policy::key::kProxyServer, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value("proxy:8888"), nullptr);
  UpdatePolicy(policy);

  base::Value::Dict proxy_config;
  proxy_config.Set("mode", base::Value(ProxyPrefs::kAutoDetectProxyModeName));
  ProxyConfigDictionary proxy_config_dict(std::move(proxy_config));
  const ash::NetworkState* network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  ASSERT_TRUE(network);
  ash::proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
  RunUntilIdle();

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config.Set("host", base::Value("proxy"));
  expected_proxy_config.Set("port", base::Value(8888));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ProxyPrefTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  base::Value::Dict proxy_config;
  proxy_config.Set("mode", base::Value(ProxyPrefs::kPacScriptProxyModeName));
  proxy_config.Set("pac_url", base::Value("http://proxy"));
  browser()->profile()->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                                            std::move(proxy_config));
  RunUntilIdle();

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set("mode",
                            base::Value(ProxyPrefs::kPacScriptProxyModeName));
  expected_proxy_config.Set("pacUrl", base::Value("http://proxy"));
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, DefaultNetworkProxyConfigTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  base::Value::Dict proxy_config;
  proxy_config.Set("mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  proxy_config.Set("server", base::Value("proxy:8080"));
  SetProxyConfigForNetworkService(kDefaultServicePath, std::move(proxy_config));
  RunUntilIdle();

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config.Set("host", base::Value("proxy"));
  expected_proxy_config.Set("port", base::Value(8080));
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

// Chrome and ARC use different delimiters for the string representation of the
// proxy bypass list. This test verifies that the string bypass list sent by
// Chrome to ARC is formatted in a way that Android code understands.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest,
                       ProxyBypassListStringRepresentationTest) {
  fake_intent_helper_instance_->clear_broadcasts();

  net::ProxyBypassRules chrome_proxy_bypass_rules;
  chrome_proxy_bypass_rules.AddRuleFromString("test1.org");
  chrome_proxy_bypass_rules.AddRuleFromString("test2.org");

  const char kArcProxyBypassList[] = "test1.org,test2.org";

  base::Value::Dict proxy_config;
  proxy_config.Set("mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  proxy_config.Set("server", base::Value("proxy:8080"));
  proxy_config.Set("bypass_list",
                   base::Value(chrome_proxy_bypass_rules.ToString()));
  SetProxyConfigForNetworkService(kDefaultServicePath, std::move(proxy_config));
  RunUntilIdle();

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config.Set("host", base::Value("proxy"));
  expected_proxy_config.Set("port", base::Value(8080));
  expected_proxy_config.Set("bypassList", base::Value(kArcProxyBypassList));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, DefaultNetworkDisconnectedTest) {
  ConnectWifiNetworkService(kWifi0ServicePath, kWifi0Guid, kWifi0Ssid);
  fake_intent_helper_instance_->clear_broadcasts();
  // Set proxy confog for default network.
  base::Value::Dict default_proxy_config;
  default_proxy_config.Set("mode",
                           base::Value(ProxyPrefs::kFixedServersProxyModeName));
  default_proxy_config.Set("server", base::Value("default.proxy.test:8080"));
  SetProxyConfigForNetworkService(kDefaultServicePath,
                                  std::move(default_proxy_config));
  RunUntilIdle();

  // Set proxy confog for WI-FI network.
  base::Value::Dict wifi_proxy_config;
  wifi_proxy_config.Set("mode",
                        base::Value(ProxyPrefs::kFixedServersProxyModeName));
  wifi_proxy_config.Set("server", base::Value("wifi.proxy.test:8080"));
  SetProxyConfigForNetworkService(kWifi0ServicePath,
                                  std::move(wifi_proxy_config));
  RunUntilIdle();

  // Observe default network proxy config broadcast.
  base::Value::Dict expected_default_proxy_config;
  expected_default_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_default_proxy_config.Set("host", base::Value("default.proxy.test"));
  expected_default_proxy_config.Set("port", base::Value(8080));
  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_default_proxy_config}),
            1);

  // Disconnect default network.
  fake_intent_helper_instance_->clear_broadcasts();
  DisconnectNetworkService(kDefaultServicePath);

  // Observe WI-FI network proxy config broadcast.
  base::Value::Dict expected_wifi_proxy_config;
  expected_wifi_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_wifi_proxy_config.Set("host", base::Value("wifi.proxy.test"));
  expected_wifi_proxy_config.Set("port", base::Value(8080));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_wifi_proxy_config}),
            1);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, NoNetworkConnectedTest) {
  // Disconnect all networks.
  fake_intent_helper_instance_->clear_broadcasts();
  DisconnectNetworkService(kDefaultServicePath);

  EXPECT_EQ(
      CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(), {}), 0);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, TwoONCProxyPolicyTest) {
  // Connect to wifi1 with appliead device ONC policy.
  ConnectWifiNetworkService(kWifi1ServicePath, kWifi1Guid, kWifi1Ssid);

  // Disconnect default network.
  DisconnectNetworkService(kDefaultServicePath);

  fake_intent_helper_instance_->clear_broadcasts();

  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kUserONCPolicy), nullptr);
  policy.Set(policy::key::kDeviceOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_CLOUD, base::Value(kDeviceONCPolicy),
             nullptr);
  UpdatePolicy(policy);

  base::Value::Dict expected_proxy_config;
  expected_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config.Set("host", base::Value("proxy"));
  expected_proxy_config.Set("port", base::Value(5000));

  base::Value::Dict expected_proxy_config_direct;
  expected_proxy_config_direct.Set(
      "mode", base::Value(ProxyPrefs::kDirectProxyModeName));

  EXPECT_EQ(CountProxyBroadcasts(
                fake_intent_helper_instance_->broadcasts(),
                {&expected_proxy_config, &expected_proxy_config_direct}),
            2);

  DisconnectNetworkService(kWifi1ServicePath);
  fake_intent_helper_instance_->clear_broadcasts();

  // Connect to wifi0 with appliead user ONC policy.
  ConnectWifiNetworkService(kWifi0ServicePath, kWifi0Guid, kWifi0Ssid);

  expected_proxy_config.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config.Set("host", base::Value("proxy-n300"));
  expected_proxy_config.Set("port", base::Value(3000));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config}),
            1);
}

// Test that on consumer devices, the proxy is correctly synced when the user
// changes network configurations.
IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, ProxySyncUnmanagedDevice) {
  fake_intent_helper_instance_->clear_broadcasts();

  std::vector<base::Value::Dict*> expected_proxy_configs;
  base::Value::Dict expected_proxy_config1;
  expected_proxy_config1.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config1.Set("host", base::Value("proxy"));
  expected_proxy_config1.Set("port", base::Value(1111));

  base::Value::Dict expected_proxy_config2;
  expected_proxy_config2.Set(
      "mode", base::Value(ProxyPrefs::kFixedServersProxyModeName));
  expected_proxy_config2.Set("host", base::Value("proxy"));
  expected_proxy_config2.Set("port", base::Value(2222));

  // The number of times to sync is randomly chosen. The only constraint is that
  // it has to be larger than two, as the proxy settings will be synced once at
  // ARC boot time and once when the default network is first changed.
  int proxy_sync_count = 10;

  for (int i = 0; i < proxy_sync_count; i += 2) {
    base::Value::Dict proxy_config1;
    proxy_config1.Set("mode",
                      base::Value(ProxyPrefs::kFixedServersProxyModeName));
    proxy_config1.Set("server", base::Value("proxy:1111"));
    SetProxyConfigForNetworkService(kDefaultServicePath,
                                    std::move(proxy_config1));
    expected_proxy_configs.push_back(&expected_proxy_config1);
    RunUntilIdle();

    base::Value::Dict proxy_config2;
    proxy_config2.Set("mode",
                      base::Value(ProxyPrefs::kFixedServersProxyModeName));
    proxy_config2.Set("server", base::Value("proxy:2222"));
    SetProxyConfigForNetworkService(kDefaultServicePath,
                                    std::move(proxy_config2));
    expected_proxy_configs.push_back(&expected_proxy_config2);
    RunUntilIdle();
  }

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 expected_proxy_configs),
            proxy_sync_count);
}

IN_PROC_BROWSER_TEST_F(ArcSettingsServiceTest, WebProxyAutoDiscovery) {
  fake_intent_helper_instance_->clear_broadcasts();

  // Set the proxy config to use auto-discovery. There's no PAC URL set via DHCP
  // so the URL "http://wpad/wpad.dat" set via DNS will be propagated to ARC.
  base::Value::Dict proxy_config_wpad;
  proxy_config_wpad.Set("mode",
                        base::Value(ProxyPrefs::kAutoDetectProxyModeName));
  browser()->profile()->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                                            std::move(proxy_config_wpad));

  RunUntilIdle();
  const char kWebProxyAutodetectionUrl[] = "www.proxyurl.com:443";

  ash::ShillIPConfigClient::TestInterface* ip_config_client =
      ash::ShillIPConfigClient::Get()->GetTestInterface();

  // Set the WPAD DHCP URL. This should now have precedence over the PAC URL set
  // via DNS.
  base::Value::Dict wpad_config;
  wpad_config.Set(shill::kWebProxyAutoDiscoveryUrlProperty,
                  base::Value(kWebProxyAutodetectionUrl));
  const std::string kIPConfigPath = "test_ip_config";
  ip_config_client->AddIPConfig(kIPConfigPath, std::move(wpad_config));

  ash::ShillServiceClient::TestInterface* service_test =
      ash::ShillServiceClient::Get()->GetTestInterface();

  service_test->SetServiceProperty(kDefaultServicePath,
                                   shill::kIPConfigProperty,
                                   base::Value(kIPConfigPath));
  RunUntilIdle();

  // Remove the proxy.
  base::Value::Dict proxy_config_direct;
  proxy_config_direct.Set("mode",
                          base::Value(ProxyPrefs::kDirectProxyModeName));
  browser()->profile()->GetPrefs()->SetDict(proxy_config::prefs::kProxy,
                                            std::move(proxy_config_direct));

  RunUntilIdle();
  base::Value::Dict expected_proxy_config_wpad_dns;
  expected_proxy_config_wpad_dns.Set(
      "mode", base::Value(ProxyPrefs::kAutoDetectProxyModeName));
  expected_proxy_config_wpad_dns.Set("pacUrl",
                                     base::Value("http://wpad/wpad.dat"));

  base::Value::Dict expected_proxy_config_wpad_dhcp;
  expected_proxy_config_wpad_dhcp.Set(
      "mode", base::Value(ProxyPrefs::kAutoDetectProxyModeName));
  expected_proxy_config_wpad_dhcp.Set("pacUrl",
                                      base::Value(kWebProxyAutodetectionUrl));

  base::Value::Dict expected_proxy_config_direct;
  expected_proxy_config_direct.Set(
      "mode", base::Value(ProxyPrefs::kDirectProxyModeName));

  EXPECT_EQ(CountProxyBroadcasts(fake_intent_helper_instance_->broadcasts(),
                                 {&expected_proxy_config_wpad_dns,
                                  &expected_proxy_config_wpad_dhcp,
                                  &expected_proxy_config_direct}),
            3);
}

}  // namespace arc
