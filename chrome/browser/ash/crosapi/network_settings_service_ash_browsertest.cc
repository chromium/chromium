// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "base/callback.h"
#include "chrome/browser/ash/crosapi/network_settings_translation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
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
#include "net/base/proxy_server.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "url/url_constants.h"

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
class TestObserver : public crosapi::mojom::NetworkSettingsObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  // crosapi::mojom::NetworkSettingsObserver:
  void OnProxyChanged(crosapi::mojom::ProxyConfigPtr proxy_config) override {
    proxy_config_ = std::move(proxy_config);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }
  crosapi::mojom::ProxyConfigPtr proxy_config_;
  mojo::Receiver<crosapi::mojom::NetworkSettingsObserver> receiver_{this};

 private:
  base::OnceClosure quit_closure_;
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
    network_service_ash_ = std::make_unique<NetworkSettingsServiceAsh>(
        g_browser_process->local_state());
    mojo::Remote<mojom::NetworkSettingsService> network_service_ash_remote;
    network_service_ash_->BindReceiver(
        network_service_ash_remote.BindNewPipeAndPassReceiver());
    observer_ = std::make_unique<TestObserver>();
    network_service_ash_remote->AddNetworkSettingsObserver(
        observer_->receiver_.BindNewPipeAndPassRemote());
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    network_service_ash_.reset();
  }

  void SetupNetworkEnvironment() {
    chromeos::ShillProfileClient::TestInterface* profile_test =
        chromeos::ShillProfileClient::Get()->GetTestInterface();
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::ShillServiceClient::Get()->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "user");

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
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::ShillServiceClient::Get()->GetTestInterface();

    service_test->AddService(service_path, guid, ssid, shill::kTypeWifi,
                             shill::kStateOnline, true /* add_to_visible */);

    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     base::Value(kUserProfilePath));
  }

  policy::MockConfigurationPolicyProvider provider_;
  std::unique_ptr<NetworkSettingsServiceAsh> network_service_ash_;
  std::unique_ptr<TestObserver> observer_;
};

// Verifies that the `NetworkSettingsServiceAsh` listens to default network
// changes and propagates the network configurations to observers via the mojo
// API.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshTest, ProxyConfigUpdate) {
  base::RunLoop run_loop;
  observer_->SetQuitClosure(run_loop.QuitClosure());
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);
  // Wait for the `observer` to get the proxy configurations from the ONC
  // policy.
  run_loop.Run();

  ASSERT_TRUE(observer_->proxy_config_);
  ASSERT_TRUE(observer_->proxy_config_->proxy_settings->is_manual());
  crosapi::mojom::ProxySettingsManualPtr manual =
      std::move(observer_->proxy_config_->proxy_settings->get_manual());
  ASSERT_EQ(manual->http_proxies.size(), 1u);
  EXPECT_EQ(manual->http_proxies[0]->host, "proxyhost");
  EXPECT_EQ(manual->http_proxies[0]->port, 3128);
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

 protected:
  // This method simulates sending an extension controlled proxy config from
  // Lacros to Ash.
  void SendExtensionProxyConfig(base::Value proxy_dict, bool can_be_disabled) {
    ProxyConfigDictionary proxy_config_dict(std::move(proxy_dict));
    auto proxy_config =
        crosapi::ProxyConfigToCrosapiProxy(&proxy_config_dict,
                                           /*wpad_url=*/GURL(""));
    proxy_config->extension = crosapi::mojom::ExtensionControllingProxy::New();
    proxy_config->extension->name = kExtensionName;
    proxy_config->extension->id = kExtensionId;
    proxy_config->extension->can_be_disabled = can_be_disabled;
    network_service_ash_->SetExtensionProxy(std::move(proxy_config));

    base::Value expected_pref(base::Value::Type::DICTIONARY);
    expected_pref.SetStringKey(kPrefExtensionNameKey, kExtensionName);
    expected_pref.SetStringKey(kPrefExtensionIdKey, kExtensionId);
    expected_pref.SetBoolKey(kPrefExtensionCanDisabled, can_be_disabled);
    WaitForLacrosProxyControllingExtensionPref(std::move(expected_pref));
  }

  void WaitForLacrosProxyControllingExtensionPref(
      const base::Value& expected_pref_value) {
    WaitForPrefValue(browser()->profile()->GetPrefs(),
                     ash::prefs::kLacrosProxyControllingExtension,
                     std::move(expected_pref_value));
  }
};

IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       SetAndClearExtensionProxy) {
  const PrefService::Preference* proxy_pref =
      browser()->profile()->GetPrefs()->FindPreference(
          proxy_config::prefs::kProxy);
  ASSERT_TRUE(proxy_pref);
  const PrefService::Preference* extension_proxy_pref =
      browser()->profile()->GetPrefs()->FindPreference(
          ash::prefs::kLacrosProxyControllingExtension);
  ASSERT_TRUE(extension_proxy_pref);

  // Emulate receiving an initial proxy config from lacros-chrome.
  base::Value proxy_config = ProxyConfigDictionary::CreatePacScript(
      kPacUrl, /*is_pac_mandatory=*/true);
  SendExtensionProxyConfig(proxy_config.Clone(),
                           /*can_be_disabled=*/true);
  EXPECT_EQ(*(proxy_pref->GetValue()), proxy_config);
  EXPECT_EQ(
      *extension_proxy_pref->GetValue()->FindStringKey(kPrefExtensionNameKey),
      kExtensionName);
  EXPECT_EQ(
      *extension_proxy_pref->GetValue()->FindStringKey(kPrefExtensionIdKey),
      kExtensionId);
  EXPECT_EQ(
      extension_proxy_pref->GetValue()->FindBoolKey(kPrefExtensionCanDisabled),
      true);
  // Send an update.
  SendExtensionProxyConfig(proxy_config.Clone(),
                           /*can_be_disabled=*/false);
  EXPECT_EQ(
      extension_proxy_pref->GetValue()->FindBoolKey(kPrefExtensionCanDisabled),
      false);

  // Send another update - clear.
  network_service_ash_->ClearExtensionProxy();
  WaitForLacrosProxyControllingExtensionPref(
      base::Value(base::Value::Type::DICTIONARY));

  EXPECT_EQ(*(extension_proxy_pref->GetValue()),
            base::Value(base::Value::Type::DICTIONARY));
  // proxy_mode=system is the default value (see
  // PrefProxyConfigTrackerImpl::RegisterProfilePrefs).
  EXPECT_EQ(*(proxy_pref->GetValue()), ProxyConfigDictionary::CreateSystem());
}

// Verifies that proxies set via policy have precedence over proxies set via
// Lacros extensions. The test sets a PAC proxy via extension, a direct proxy
// via policy and then verifies that the direct proxy is applied.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       UserPolicyHasPrecedence) {
  base::Value pac_proxy = ProxyConfigDictionary::CreatePacScript(
      kPacUrl, /*is_pac_mandatory=*/true);
  SendExtensionProxyConfig(pac_proxy.Clone(),
                           /*can_be_disabled=*/true);
  // Set proxy by policy.
  policy::PolicyMap policy;
  policy.Set(policy::key::kProxyMode, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(ProxyPrefs::kAutoDetectProxyModeName), nullptr);
  provider_.UpdateChromePolicy(policy);

  const base::Value* proxy_pref =
      browser()->profile()->GetPrefs()->GetDictionary(
          proxy_config::prefs::kProxy);
  ASSERT_TRUE(proxy_pref);
  EXPECT_EQ(*proxy_pref, ProxyConfigDictionary::CreateAutoDetect());

  // The kLacrosProxyControllingExtension pref which is used to display the
  // extension controlling the proxy should also be reset.
  const base::Value* extension_proxy_pref =
      browser()->profile()->GetPrefs()->GetDictionary(
          ash::prefs::kLacrosProxyControllingExtension);
  ASSERT_TRUE(extension_proxy_pref);
  EXPECT_EQ(*extension_proxy_pref, base::Value(base::Value::Type::DICTIONARY));
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
                                             /*is_pac_mandatory=*/true));
  auto proxy_config = crosapi::ProxyConfigToCrosapiProxy(&proxy_config_dict,
                                                         /*wpad_url=*/GURL(""));
  proxy_config->extension = crosapi::mojom::ExtensionControllingProxy::New();
  proxy_config->extension->name = kExtensionName;
  proxy_config->extension->id = kExtensionId;
  proxy_config->extension->can_be_disabled = true;
  network_service_ash_->SetExtensionProxy(std::move(proxy_config));
  base::RunLoop().RunUntilIdle();
  const base::Value* proxy_pref =
      browser()->profile()->GetPrefs()->GetDictionary(
          proxy_config::prefs::kProxy);
  ASSERT_TRUE(proxy_pref);
  EXPECT_EQ(*proxy_pref, ProxyConfigDictionary::CreateDirect());
}

// Proxies set by extensions in the primary profile should have priority in Ash
// over proxies set via ONC policy. This test sets a manual proxy via ONC and a
// PAC proxy via Lacros extension and then verifies that the Lacros proxy has
// priority. It also verifies that after clearing the proxy extension, the
// AshNetworkSettingsService uses the ONC proxy.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       OncPolicyHasLowerPriority) {
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);

  EXPECT_TRUE(observer_->proxy_config_->proxy_settings->is_manual());
  {
    base::RunLoop run_loop;
    observer_->SetQuitClosure(run_loop.QuitClosure());
    base::Value pac_proxy = ProxyConfigDictionary::CreatePacScript(
        kPacUrl, /*is_pac_mandatory=*/true);
    SendExtensionProxyConfig(pac_proxy.Clone(),
                             /*can_be_disabled=*/true);
    // Wait for the `observer` to get the proxy
    // configurations from the extension.
    run_loop.Run();
  }
  ASSERT_TRUE(observer_->proxy_config_);
  EXPECT_TRUE(observer_->proxy_config_->proxy_settings->is_pac());

  {
    base::RunLoop run_loop;
    observer_->SetQuitClosure(run_loop.QuitClosure());
    network_service_ash_->ClearExtensionProxy();
    run_loop.Run();
  }
  ASSERT_TRUE(observer_->proxy_config_);
  EXPECT_TRUE(observer_->proxy_config_->proxy_settings->is_manual());
}

// Same as the `OncPolicyHasLowerPriority` test, but with reverse order of
// proxies applied. This test ensures that priority order is assigned according
// to proxy source and not the latest applied config.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshExtensionTest,
                       ExtensionHasHigherPriorityThanOncPolicy) {
  {
    base::RunLoop run_loop;
    observer_->SetQuitClosure(run_loop.QuitClosure());
    base::Value pac_proxy = ProxyConfigDictionary::CreatePacScript(
        kPacUrl, /*is_pac_mandatory=*/true);
    SendExtensionProxyConfig(pac_proxy.Clone(),
                             /*can_be_disabled=*/true);
    // Wait for the `observer` to get the proxy
    // configurations from the extension.
    run_loop.Run();
  }

  // Set a manual proxy.
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);

  // Expect that the PAC proxy set by the extension is still active.
  ASSERT_TRUE(observer_->proxy_config_);
  EXPECT_TRUE(observer_->proxy_config_->proxy_settings->is_pac());
}

}  // namespace crosapi
