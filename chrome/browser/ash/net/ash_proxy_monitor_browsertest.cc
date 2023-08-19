// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/ash_proxy_monitor.h"

#include <tuple>

#include "ash/constants/ash_pref_names.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_test_utils.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/browser_test.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace {

constexpr char kPacUrl[] = "http://pac.pac/";

constexpr char kExtensionName[] = "Lacros Test Extension Name";
constexpr char kExtensionId[] = "Lacros Test Extension ID";
constexpr char kPrefExtensionNameKey[] = "extension_name_key";
constexpr char kPrefExtensionIdKey[] = "extension_id_key";
constexpr char kPrefExtensionCanDisabledKey[] = "can_be_disabled_key";

constexpr char kDefaultServicePath[] = "default_wifi";
constexpr char kDefaultServiceSsid[] = "default_wifi_guid";
constexpr char kDefaultServiceGuid[] = "eth0";

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kWifi0ServicePath[] = "stub_wifi";
constexpr char kWifi0Ssid[] = "wifi0";
constexpr char kWifi0Guid[] = "{wifi0_guid}";

constexpr char kWifi1ServicePath[] = "stub_wifi1";
constexpr char kWifi1Ssid[] = "wifi1";
constexpr char kWifi1Guid[] = "{wifi1_guid}";

constexpr char kONCPolicyWifi0Proxy[] =
    R"({
     "NetworkConfigurations": [ {
        "GUID": "{wifi0_guid}",
        "Name": "wifi0",
        "ProxySettings": {
           "Manual": {
              "HTTPProxy": {
                 "Host": "http://proxy.com",
                 "Port": 3128
              }
           },
           "Type": "Manual"
        },
        "Type": "WiFi",
        "WiFi": {
           "AutoConnect": true,
           "HiddenSSID": false,
           "SSID": "wifi0",
           "Security": "None"
        }
     } ]
    })";

constexpr char kONCPolicyWifi1Pac[] =
    R"({
      "NetworkConfigurations": [ {
         "GUID": "{wifi1_guid}",
         "Name": "wifi1",
         "ProxySettings": {
            "Type": "PAC",
            "PAC": "http://pac.foo.com/script.pac"
         },
         "Type": "WiFi",
         "WiFi": {
            "AutoConnect": true,
            "HiddenSSID": false,
            "SSID": "wifi0",
            "Security": "None"
         }
      } ]
    })";
constexpr char kExpectedOncPacUrl[] = "http://pac.foo.com/script.pac";

base::Value::Dict GetPacProxyConfig(const std::string& pac_url) {
  return ProxyConfigDictionary::CreatePacScript(pac_url,
                                                /*pac_mandatory=*/false);
}

base::Value::Dict GetManualProxyConfig(const std::string& proxy_servers) {
  return ProxyConfigDictionary::CreateFixedServers(
      proxy_servers, /*bypass_list=*/std::string());
}

class TestPrefObserver : public crosapi::mojom::PrefObserver {
 public:
  TestPrefObserver() = default;
  TestPrefObserver(const TestPrefObserver&) = delete;
  TestPrefObserver& operator=(const TestPrefObserver&) = delete;
  ~TestPrefObserver() override {}

  // crosapi::mojom::PrefObserver:
  void OnPrefChanged(base::Value value) override {
    future_.AddValue(std::move(value));
  }
  base::Value Wait() { return future_.Take(); }
  base::test::RepeatingTestFuture<base::Value> future_;
  mojo::Receiver<crosapi::mojom::PrefObserver> receiver_{this};
};

}  // namespace

namespace ash {

class TestAshProxyMonitorObserver : public AshProxyMonitor::Observer {
 public:
  TestAshProxyMonitorObserver() {
    g_browser_process->platform_part()->ash_proxy_monitor()->AddObserver(this);
  }
  TestAshProxyMonitorObserver(const TestAshProxyMonitorObserver&) = delete;
  TestAshProxyMonitorObserver& operator=(const TestAshProxyMonitorObserver&) =
      delete;
  ~TestAshProxyMonitorObserver() override {
    g_browser_process->platform_part()->ash_proxy_monitor()->RemoveObserver(
        this);
  }

  std::tuple<base::Value::Dict, GURL> WaitForUpdate() { return future_.Take(); }

  bool AreAllProxyUpdatesRead() { return future_.IsEmpty(); }

 private:
  void OnProxyChanged() override {
    future_.AddValue(g_browser_process->platform_part()
                         ->ash_proxy_monitor()
                         ->GetLatestProxyConfig()
                         ->GetDictionary()
                         .Clone(),
                     g_browser_process->platform_part()
                         ->ash_proxy_monitor()
                         ->GetLatestWpadUrl());
  }

  base::test::RepeatingTestFuture<base::Value::Dict, GURL> future_;
};

class AshProxyMonitorTest : public InProcessBrowserTest {
 public:
  AshProxyMonitorTest() = default;
  AshProxyMonitorTest(const AshProxyMonitorTest&) = delete;
  AshProxyMonitorTest& operator=(const AshProxyMonitorTest&) = delete;
  ~AshProxyMonitorTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetupFakePrefService();
    SetupNetworkEnvironment();
    ash_proxy_monitor_observer_ =
        std::make_unique<TestAshProxyMonitorObserver>();
    g_browser_process->platform_part()
        ->ash_proxy_monitor()
        ->SetProfileForTesting(browser()->profile());
  }

  void TearDownOnMainThread() override {
    ash_proxy_monitor_observer_.reset();
    prefs_observer_.reset();
  }

  void SetupNetworkEnvironment() {
    ash::ShillProfileClient::TestInterface* profile_test =
        ash::ShillProfileClient::Get()->GetTestInterface();
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();
    profile_test->AddProfile(kUserProfilePath, "user");
    service_test->ClearServices();
    ConnectWifiNetworkService(kDefaultServicePath, kDefaultServiceSsid,
                              kDefaultServiceGuid);
  }

  void SetupFakePrefService() {
    prefs_ash_ = std::make_unique<crosapi::PrefsAsh>(
        g_browser_process->profile_manager(), g_browser_process->local_state());
    mojo::Remote<crosapi::mojom::Prefs> prefs_ash_remote;
    prefs_ash_->BindReceiver(prefs_ash_remote.BindNewPipeAndPassReceiver());
    prefs_observer_ = std::make_unique<TestPrefObserver>();
    prefs_ash_remote->AddObserver(
        crosapi::mojom::PrefPath::kProxy,
        prefs_observer_->receiver_.BindNewPipeAndPassRemote());
    prefs_ash_->OnProfileAdded(browser()->profile());
  }

  void ConnectWifiNetworkService(const std::string& service_path,
                                 const std::string& guid,
                                 const std::string& ssid) {
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();

    service_test->AddService(service_path, guid, ssid, shill::kTypeWifi,
                             shill::kStateOnline, true /* add_to_visible */);

    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     base::Value(kUserProfilePath));
  }

  void DisconnectNetworkService(const std::string& service_path) {
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    base::Value value(shill::kStateIdle);
    service_test->SetServiceProperty(service_path, shill::kStateProperty,
                                     value);
  }

  void ClearProxyPrefFromLacrosExtension() {
    base::test::TestFuture<void> future;
    prefs_ash_->ClearExtensionControlledPref(crosapi::mojom::PrefPath::kProxy,
                                             future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  void SetProxyPrefFromLacrosExtension(base::Value::Dict proxy_dict) {
    base::test::TestFuture<void> future;
    prefs_ash_->SetPref(crosapi::mojom::PrefPath::kProxy,
                        base::Value(std::move(proxy_dict)),
                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  void SetDhcpWpadUrl(const std::string& dhcp_url,
                      const std::string& service_path) {
    auto wpad_config = base::Value::Dict().Set(
        shill::kWebProxyAutoDiscoveryUrlProperty, base::Value(dhcp_url));
    const std::string kIPConfigPath = "test_ip_config";
    ash::ShillIPConfigClient::Get()->GetTestInterface()->AddIPConfig(
        kIPConfigPath, std::move(wpad_config));

    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();

    service_test->SetServiceProperty(service_path, shill::kIPConfigProperty,
                                     base::Value(kIPConfigPath));
  }
  std::unique_ptr<crosapi::PrefsAsh> prefs_ash_;
  std::unique_ptr<TestPrefObserver> prefs_observer_;

  policy::MockConfigurationPolicyProvider provider_;
  std::unique_ptr<TestAshProxyMonitorObserver> ash_proxy_monitor_observer_;
};

// Verifies that the `AshProxyMonitor` listens to default network
// changes and propagates the proxy configuration to observers.
IN_PROC_BROWSER_TEST_F(AshProxyMonitorTest, DefaultNetworkChanges) {
  // Verify that observers get updated when connecting to a new network with a
  // proxy configured.
  DisconnectNetworkService(kDefaultServicePath);
  ConnectWifiNetworkService(kWifi0ServicePath, kWifi0Guid, kWifi0Ssid);
  policy::PolicyMap policy;
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kONCPolicyWifi0Proxy),
             nullptr);
  provider_.UpdateChromePolicy(policy);

  constexpr char kEmptyWpadUrl[] = "";
  std::tuple<base::Value::Dict, GURL> result =
      ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetManualProxyConfig("http=proxy.com:3128"));
  EXPECT_EQ(std::get<1>(result), kEmptyWpadUrl);

  // Verify that observers get updated when the default network receives a WPAD
  // URL via DHCP.
  constexpr char kWebProxyAutodetectionUrl[] = "www.proxyurl.com:443";
  SetDhcpWpadUrl(kWebProxyAutodetectionUrl, kWifi0ServicePath);

  result = ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetManualProxyConfig("http=proxy.com:3128"));
  EXPECT_EQ(std::get<1>(result), kWebProxyAutodetectionUrl);

  // Verify that observers get updated when switching networks with a different
  // proxy config and no WPAD URL.
  DisconnectNetworkService(kWifi0ServicePath);
  ConnectWifiNetworkService(kWifi1ServicePath, kWifi1Guid, kWifi1Ssid);
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kONCPolicyWifi1Pac),
             nullptr);
  provider_.UpdateChromePolicy(policy);

  result = ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetPacProxyConfig(kExpectedOncPacUrl));
  EXPECT_EQ(std::get<1>(result), kEmptyWpadUrl);

  EXPECT_TRUE(ash_proxy_monitor_observer_->AreAllProxyUpdatesRead());
}

// Verifies that the `AshProxyMonitor` listens to kProxy pref changes and
// propagates the proxy configuration to observers.
IN_PROC_BROWSER_TEST_F(AshProxyMonitorTest, ProxyPrefChanges) {
  // Set the pref via policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kPacScriptProxyModeName), nullptr);
  policy.Set(policy::key::kProxyPacUrl, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(kPacUrl), nullptr);
  provider_.UpdateChromePolicy(policy);

  std::tuple<base::Value::Dict, GURL> result =
      ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetPacProxyConfig(kPacUrl));
  EXPECT_FALSE(g_browser_process->platform_part()
                   ->ash_proxy_monitor()
                   ->IsLacrosExtensionControllingProxy());

  // Clear the policy pref.
  policy.Erase(policy::key::kProxyMode);
  policy.Erase(policy::key::kProxyPacUrl);
  provider_.UpdateChromePolicy(policy);
  result = ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), ProxyConfigDictionary::CreateDirect());

  EXPECT_TRUE(ash_proxy_monitor_observer_->AreAllProxyUpdatesRead());
}

IN_PROC_BROWSER_TEST_F(AshProxyMonitorTest, OrderOfPrecedence) {
  DisconnectNetworkService(kDefaultServicePath);
  ConnectWifiNetworkService(kWifi0ServicePath, kWifi0Guid, kWifi0Ssid);

  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kPacScriptProxyModeName), nullptr);
  policy.Set(policy::key::kProxyPacUrl, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(kPacUrl), nullptr);
  policy.Set(policy::key::kOpenNetworkConfiguration,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD, base::Value(kONCPolicyWifi0Proxy),
             nullptr);
  provider_.UpdateChromePolicy(policy);
  std::tuple<base::Value::Dict, GURL> result =
      ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetPacProxyConfig(kPacUrl));

  // Clear the global proxy.
  policy.Erase(policy::key::kProxyMode);
  policy.Erase(policy::key::kProxyPacUrl);
  provider_.UpdateChromePolicy(policy);

  // The proxy configured on the local network is active.
  result = ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetManualProxyConfig("http=proxy.com:3128"));

  EXPECT_TRUE(ash_proxy_monitor_observer_->AreAllProxyUpdatesRead());
}

IN_PROC_BROWSER_TEST_F(AshProxyMonitorTest, LacrosExtensionProxyPrefChanges) {
  SetProxyPrefFromLacrosExtension(GetPacProxyConfig(kPacUrl));
  std::tuple<base::Value::Dict, GURL> result =
      ash_proxy_monitor_observer_->WaitForUpdate();
  EXPECT_EQ(std::get<0>(result), GetPacProxyConfig(kPacUrl));

  auto* ash_proxy_monitor =
      g_browser_process->platform_part()->ash_proxy_monitor();

  EXPECT_TRUE(ash_proxy_monitor->IsLacrosExtensionControllingProxy());

  // Update the extension metadata.
  ash_proxy_monitor->SetLacrosExtensionControllingProxyInfo(
      kExtensionName, kExtensionId, false);

  auto* pref_service = browser()->profile()->GetPrefs();
  EXPECT_EQ(pref_service->GetDict(ash::prefs::kLacrosProxyControllingExtension),
            base::Value::Dict()
                .Set(kPrefExtensionNameKey, kExtensionName)
                .Set(kPrefExtensionIdKey, kExtensionId)
                .Set(kPrefExtensionCanDisabledKey, false));
  auto extension = ash_proxy_monitor->GetLacrosExtensionControllingTheProxy();
  EXPECT_EQ(extension->name, kExtensionName);
  EXPECT_EQ(extension->id, kExtensionId);
  EXPECT_EQ(extension->can_be_disabled, false);
  ClearProxyPrefFromLacrosExtension();
  ash_proxy_monitor->ClearLacrosExtensionControllingProxyInfo();

  while (std::get<0>(result) != ProxyConfigDictionary::CreateDirect()) {
    result = ash_proxy_monitor_observer_->WaitForUpdate();
  }

  EXPECT_FALSE(ash_proxy_monitor->GetLacrosExtensionControllingTheProxy());
}

}  // namespace ash
