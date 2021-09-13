// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_service_ash.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "content/public/test/browser_test.h"
#include "net/base/proxy_server.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "url/url_constants.h"

namespace {

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

  void SetupNetworkEnvironment() {
    chromeos::ShillProfileClient::TestInterface* profile_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillProfileClient()
            ->GetTestInterface();
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();

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
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();

    service_test->AddService(service_path, guid, ssid, shill::kTypeWifi,
                             shill::kStateOnline, true /* add_to_visible */);

    service_test->SetServiceProperty(service_path, shill::kProfileProperty,
                                     base::Value(kUserProfilePath));
  }

  policy::MockConfigurationPolicyProvider provider_;
};

// Verifies that the `NetworkSettingsServiceAsh` listens to default network
// changes and propagates the network configurations to observers via the mojo
// API.
IN_PROC_BROWSER_TEST_F(NetworkSettingsServiceAshTest, ProxyConfigUpdate) {
  SetupNetworkEnvironment();
  NetworkSettingsServiceAsh network_service_ash(
      g_browser_process->local_state());
  mojo::Remote<mojom::NetworkSettingsService> network_service_ash_remote;
  network_service_ash.BindReceiver(
      network_service_ash_remote.BindNewPipeAndPassReceiver());
  TestObserver observer;
  network_service_ash_remote->AddNetworkSettingsObserver(
      observer.receiver_.BindNewPipeAndPassRemote());

  base::RunLoop run_loop;
  observer.SetQuitClosure(run_loop.QuitClosure());
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);
  // Wait for the `observer` to get the proxy configurations from the ONC
  // policy.
  run_loop.Run();

  ASSERT_TRUE(observer.proxy_config_);
  ASSERT_TRUE(observer.proxy_config_->proxy_settings->is_manual());
  crosapi::mojom::ProxySettingsManualPtr manual =
      std::move(observer.proxy_config_->proxy_settings->get_manual());
  ASSERT_EQ(manual->http_proxies.size(), 1);
  EXPECT_EQ(manual->http_proxies[0]->host, "proxyhost");
  EXPECT_EQ(manual->http_proxies[0]->port, 3128);
}

}  // namespace crosapi
