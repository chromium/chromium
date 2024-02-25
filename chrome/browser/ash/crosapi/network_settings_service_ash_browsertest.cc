// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/network_settings_translation.h"
#include "chrome/browser/ash/crosapi/prefs_ash.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
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
constexpr char kPrefExtensionCanDisabled[] = "can_be_disabled_key";

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kWifiServicePath[] = "stub_wifi";
constexpr char kWifiSsid[] = "wifi0";
constexpr char kWifiGuid[] = "{wifi0_guid}";

constexpr char kONCPolicyWifi0Proxy[] =
    R"({
     "NetworkConfigurations": [ {
        "GUID": "{wifi0_guid}",
        "Name": "wifi0",
        "ProxySettings": {
           "Manual": {
              "HTTPProxy": {
                 "Host": "proxyhost",
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

// Observes network changes coming from the network settings service.
class NetworkSettingsObserver : public crosapi::mojom::NetworkSettingsObserver {
 public:
  NetworkSettingsObserver() = default;
  NetworkSettingsObserver(const NetworkSettingsObserver&) = delete;
  NetworkSettingsObserver& operator=(const NetworkSettingsObserver&) = delete;
  ~NetworkSettingsObserver() override = default;

  // crosapi::mojom::NetworkSettingsObserver:
  void OnProxyChanged(crosapi::mojom::ProxyConfigPtr proxy_config) override {
    future_.SetValue(std::move(proxy_config));
  }

  void OnAlwaysOnVpnPreConnectUrlAllowlistEnforcedChanged(
      bool enforced) override {
    alwayson_vpn_pre_connect_url_allowlist_enforced_changed_future_.SetValue(
        enforced);
  }

  crosapi::mojom::ProxyConfigPtr WaitForProxyConfig() { return future_.Take(); }
  bool WaitForAlwaysOnVpnPreConnectUrlAllowlistEnforced() {
    return alwayson_vpn_pre_connect_url_allowlist_enforced_changed_future_
        .Take();
  }

  mojo::Receiver<crosapi::mojom::NetworkSettingsObserver> receiver_{this};

  bool AreAllProxyUpdatesRead() { return !future_.IsReady(); }

 private:
  base::test::TestFuture<crosapi::mojom::ProxyConfigPtr> future_;
  base::test::TestFuture<bool>
      alwayson_vpn_pre_connect_url_allowlist_enforced_changed_future_;
};

}  // namespace

namespace crosapi {

class NetworkSettingsServiceAshTest : public InProcessBrowserTest {
 public:
  NetworkSettingsServiceAshTest() = default;
  NetworkSettingsServiceAshTest(const NetworkSettingsServiceAshTest&) = delete;
  NetworkSettingsServiceAshTest& operator=(
      const NetworkSettingsServiceAshTest&) = delete;
  ~NetworkSettingsServiceAshTest() override = default;

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
    SetupNetworkEnvironment();
    ash_proxy_monitor_ = std::make_unique<ash::AshProxyMonitor>(
        g_browser_process->local_state(), g_browser_process->profile_manager());
    network_service_ash_ =
        std::make_unique<NetworkSettingsServiceAsh>(ash_proxy_monitor_.get());
    mojo::Remote<mojom::NetworkSettingsService> network_service_ash_remote;
    network_service_ash_->BindReceiver(
        network_service_ash_remote.BindNewPipeAndPassReceiver());
    observer_ = std::make_unique<NetworkSettingsObserver>();
    network_service_ash_remote->AddNetworkSettingsObserver(
        observer_->receiver_.BindNewPipeAndPassRemote());
    ash_proxy_monitor_->SetProfileForTesting(browser()->profile());

    network_service_ash_remote.FlushForTesting();
    auto result = observer_->WaitForProxyConfig();

    ASSERT_FALSE(result.is_null());
    EXPECT_TRUE(result->proxy_settings->is_direct());
    EXPECT_TRUE(result->extension.is_null());
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    network_service_ash_.reset();
    ash_proxy_monitor_.reset();
  }

  void SetupNetworkEnvironment() {
    ash::ShillProfileClient::TestInterface* profile_test =
        ash::ShillProfileClient::Get()->GetTestInterface();
    ash::ShillServiceClient::TestInterface* service_test =
        ash::ShillServiceClient::Get()->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "test-user");

    service_test->ClearServices();
    ConnectWifiNetworkService(kWifiServicePath, kWifiGuid, kWifiSsid);
  }

  void SetOncPolicy(const std::string& policy_json, policy::PolicyScope scope) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_LEVEL_MANDATORY, scope,
               policy::POLICY_SOURCE_CLOUD, base::Value(policy_json), nullptr);
    provider_.UpdateChromePolicy(policy);
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
  }

  policy::MockConfigurationPolicyProvider provider_;
  std::unique_ptr<ash::AshProxyMonitor> ash_proxy_monitor_;
  std::unique_ptr<NetworkSettingsServiceAsh> network_service_ash_;
  std::unique_ptr<NetworkSettingsObserver> observer_;
};

// Verifies that the `NetworkSettingsServiceAsh` listens to default network
// changes and propagates the network configurations to observers via the mojo
// API.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshTest, ProxyConfigUpdate) {
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);

  auto result = observer_->WaitForProxyConfig();
  crosapi::mojom::ProxySettingsManualPtr manual =
      std::move(result->proxy_settings->get_manual());
  ASSERT_EQ(manual->http_proxies.size(), 1u);
  EXPECT_EQ(manual->http_proxies[0]->host, "proxyhost");
  EXPECT_EQ(manual->http_proxies[0]->port, 3128);
  EXPECT_TRUE(result->extension.is_null());
}

IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshTest,
                       SetAlwaysOnVpnPreConnectUrlAllowlistEnforced) {
  network_service_ash_->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      /*enforced=*/true);
  EXPECT_TRUE(observer_->WaitForAlwaysOnVpnPreConnectUrlAllowlistEnforced());

  network_service_ash_->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      /*enforced=*/false);
  EXPECT_FALSE(observer_->WaitForAlwaysOnVpnPreConnectUrlAllowlistEnforced());
}

// Test suite for testing the AshNetworkSettingsService with proxies set via
// extensions in the Lacros primary profile.
class NetworkSettingsServiceAshExtensionTest
    : public NetworkSettingsServiceAshTest {
 public:
  NetworkSettingsServiceAshExtensionTest() = default;
  NetworkSettingsServiceAshExtensionTest(
      const NetworkSettingsServiceAshExtensionTest&) = delete;
  NetworkSettingsServiceAshExtensionTest& operator=(
      const NetworkSettingsServiceAshExtensionTest&) = delete;
  ~NetworkSettingsServiceAshExtensionTest() override = default;

  void SetUpOnMainThread() override {
    NetworkSettingsServiceAshTest::SetUpOnMainThread();
    prefs_ash_ = std::make_unique<PrefsAsh>(
        g_browser_process->profile_manager(), g_browser_process->local_state());
    prefs_ash_->OnProfileAdded(browser()->profile());
  }

 protected:
  // This method simulates mojo call which happen when an extension sets the
  // proxy in the Lacros primary profile.
  void SetExtensionProxyInLacros(base::Value::Dict proxy_dict,
                                 bool can_be_disabled) {
    SetProxyMetadata(can_be_disabled);
    SetProxyPref(std::move(proxy_dict));
  }

  void ClearExtensionProxyInLacros() {
    base::test::TestFuture<void> future;
    prefs_ash_->ClearExtensionControlledPref(mojom::PrefPath::kProxy,
                                             future.GetCallback());
    EXPECT_TRUE(future.Wait());

    network_service_ash_->ClearExtensionControllingProxyMetadata();
  }

  void SetProxyPref(base::Value::Dict proxy_dict) {
    base::test::TestFuture<void> future;
    prefs_ash_->SetPref(mojom::PrefPath::kProxy,
                        base::Value(std::move(proxy_dict)),
                        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  // Sends the proxy metadata through the mojo service NetworkSettingsService
  // and waits for Ash to store the metadata in the profile prefs.
  void SetProxyMetadata(bool can_be_disabled) {
    auto extension = crosapi::mojom::ExtensionControllingProxy::New();
    extension->name = kExtensionName;
    extension->id = kExtensionId;
    extension->can_be_disabled = can_be_disabled;
    network_service_ash_->SetExtensionControllingProxyMetadata(
        std::move(extension));

    base::Value::Dict expected_pref =
        base::Value::Dict()
            .Set(kPrefExtensionNameKey, kExtensionName)
            .Set(kPrefExtensionIdKey, kExtensionId)
            .Set(kPrefExtensionCanDisabled, can_be_disabled);
    WaitForPrefValue(browser()->profile()->GetPrefs(),
                     ash::prefs::kLacrosProxyControllingExtension,
                     base::Value(std::move(expected_pref)));
  }

  std::unique_ptr<PrefsAsh> prefs_ash_;
};

IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       SetAndClearExtensionProxy) {
  // Emulate receiving an initial proxy config from lacros-chrome.
  base::Value::Dict proxy_config =
      ProxyConfigDictionary::CreatePacScript(kPacUrl, /*pac_mandatory=*/true);
  SetExtensionProxyInLacros(proxy_config.Clone(),
                            /*can_be_disabled=*/true);
  auto result = observer_->WaitForProxyConfig();
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->proxy_settings->is_pac());
  EXPECT_EQ(result->proxy_settings->get_pac()->pac_url, kPacUrl);

  ASSERT_FALSE(result->extension.is_null());
  EXPECT_EQ(result->extension->name, kExtensionName);
  EXPECT_EQ(result->extension->id, kExtensionId);

  ClearExtensionProxyInLacros();

  result = observer_->WaitForProxyConfig();
  ASSERT_FALSE(result.is_null());
  EXPECT_TRUE(result->proxy_settings->is_direct());
  EXPECT_TRUE(result->extension.is_null());
}

// Verifies that proxies set via policy have precedence over proxies set via
// Lacros extensions. The test sets a PAC proxy via extension, a direct proxy
// via policy and then verifies that the direct proxy is applied.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       UserPolicyHasPrecedence) {
  base::Value::Dict pac_proxy =
      ProxyConfigDictionary::CreatePacScript(kPacUrl,
                                             /*pac_mandatory=*/true);
  SetExtensionProxyInLacros(pac_proxy.Clone(),
                            /*can_be_disabled=*/true);

  auto result = observer_->WaitForProxyConfig();
  ASSERT_FALSE(result->extension.is_null());
  EXPECT_EQ(result->extension->name, kExtensionName);
  EXPECT_EQ(result->extension->id, kExtensionId);

  // Set proxy by policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kAutoDetectProxyModeName), nullptr);
  provider_.UpdateChromePolicy(policy);

  const base::Value::Dict& proxy_pref =
      browser()->profile()->GetPrefs()->GetDict(proxy_config::prefs::kProxy);
  EXPECT_EQ(proxy_pref, ProxyConfigDictionary::CreateAutoDetect());

  EXPECT_FALSE(ash_proxy_monitor_->IsLacrosExtensionControllingProxy());

  result = observer_->WaitForProxyConfig();
  EXPECT_TRUE(result->extension.is_null());
}

// Same as the `UserPolicyHasPrecedence` test, but with reverse order of proxies
// applied. This test ensures that priority order is assigned according to proxy
// source and not the latest applied config.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       ExtensionHasLowerPrecedenceThanUserPolicy) {
  // Set proxy by policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kDirectProxyModeName), nullptr);
  provider_.UpdateChromePolicy(policy);

  ProxyConfigDictionary proxy_config_dict(
      ProxyConfigDictionary::CreatePacScript(kPacUrl,
                                             /*pac_mandatory=*/true));

  // Do not use `SetExtensionProxyInLacros` here because
  // `SetExtensionProxyInLacros` will wait until the proxy is changed in Ash and
  // an update is sent to a fake observer. Since this test verifies that the
  // proxy set by extension has a lower priority, no update should be sent
  // here (`SetExtensionProxyInLacros` would wait forever).
  SetProxyPref(ProxyConfigDictionary::CreatePacScript(kPacUrl,
                                                      /*pac_mandatory=*/true));

  const base::Value::Dict& proxy_pref =
      browser()->profile()->GetPrefs()->GetDict(proxy_config::prefs::kProxy);
  EXPECT_EQ(proxy_pref, ProxyConfigDictionary::CreateDirect());
}

// Proxies set by extensions in the primary profile should have priority in Ash
// over proxies set via ONC policy. This test sets a manual proxy via ONC and a
// PAC proxy via Lacros extension and then verifies that the Lacros proxy has
// priority. It also verifies that after clearing the proxy extension, the
// AshNetworkSettingsService uses the ONC proxy.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       OncPolicyHasLowerPriority) {
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);

  auto result = observer_->WaitForProxyConfig();
  EXPECT_TRUE(result->proxy_settings->is_manual());

  base::Value::Dict pac_proxy =
      ProxyConfigDictionary::CreatePacScript(kPacUrl,
                                             /*pac_mandatory=*/true);
  SetExtensionProxyInLacros(pac_proxy.Clone(),
                            /*can_be_disabled=*/true);

  result = observer_->WaitForProxyConfig();
  // The first update may have been triggered by the pref change via the Prefs
  // service. Wait for update which contains the extension metadata.
  if (result->extension.is_null()) {
    result = observer_->WaitForProxyConfig();
  }

  ASSERT_TRUE(result);
  EXPECT_TRUE(result->proxy_settings->is_pac());
  ASSERT_FALSE(result->extension.is_null());
  EXPECT_EQ(result->extension->name, kExtensionName);
  EXPECT_EQ(result->extension->id, kExtensionId);

  ClearExtensionProxyInLacros();

  // Wait for the update which clear the proxy extension metadata and sets the
  // proxy value to manual, as specified by the ONC policy.
  if (!result->proxy_settings->is_manual()) {
    result = observer_->WaitForProxyConfig();
  }

  EXPECT_TRUE(result->extension.is_null());
}

// Same as the `OncPolicyHasLowerPriority` test, but with reverse order of
// proxies applied. This test ensures that priority order is assigned according
// to proxy source and not the latest applied config.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       ExtensionHasHigherPriorityThanOncPolicy) {
  base::Value::Dict pac_proxy =
      ProxyConfigDictionary::CreatePacScript(kPacUrl,
                                             /*pac_mandatory=*/true);
  SetExtensionProxyInLacros(pac_proxy.Clone(),
                            /*can_be_disabled=*/true);

  // Set a manual proxy.
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);

  auto result = observer_->WaitForProxyConfig();
  // The first update may have been triggered by the pref change via the Prefs
  // service. Wait for update which sets the extension metadata.
  if (result->extension.is_null()) {
    result = observer_->WaitForProxyConfig();
  }
  EXPECT_TRUE(observer_->AreAllProxyUpdatesRead());

  // Expect that the PAC proxy set by the extension is still active.
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->proxy_settings->is_pac());
  ASSERT_FALSE(result->extension.is_null());
  EXPECT_EQ(result->extension->name, kExtensionName);
  EXPECT_EQ(result->extension->id, kExtensionId);
}

}  // namespace crosapi
