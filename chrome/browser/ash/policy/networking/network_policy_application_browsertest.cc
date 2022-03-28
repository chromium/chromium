// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/dbus/shill/shill_device_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_policy_observer.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace policy {
namespace {

using ::base::test::DictionaryHasValue;

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kSharedProfilePath[] = "/profile/default";
constexpr char kServiceWifi1[] = "/service/1";
constexpr char kServiceWifi2[] = "/service/2";

// A utility to wait until a FakeShillServiceClient's service has been
// connected.
// Usage:
// (1) Construct a ServiceConnectedWaiter, specifying the shill service path
//     that is expected to connect.
// (2) Call ServiceConnectedWaiter::Wait
// Wait will return when the service passed to (1) connects. If the service has
// connected between (1) and (2), Wait returns immediately. Note that this class
// does not evaluate whether the service was connected before (1).
class ServiceConnectedWaiter {
 public:
  ServiceConnectedWaiter(
      ash::ShillServiceClient::TestInterface* shill_service_client_test,
      const std::string& service_path)
      : shill_service_client_test_(shill_service_client_test),
        service_path_(service_path) {
    shill_service_client_test_->SetConnectBehavior(service_path_,
                                                   run_loop_.QuitClosure());
  }

  ServiceConnectedWaiter(const ServiceConnectedWaiter&) = delete;
  ServiceConnectedWaiter& operator=(const ServiceConnectedWaiter&) = delete;

  // Waits until the |service_path| passed to the constructor has connected.
  // If it has connected since the constructor has run, will return immediately.
  void Wait() {
    run_loop_.Run();
    shill_service_client_test_->SetConnectBehavior(service_path_,
                                                   base::RepeatingClosure());
  }

 private:
  ash::ShillServiceClient::TestInterface* shill_service_client_test_;
  std::string service_path_;
  base::RunLoop run_loop_;
};

// Registers itself as ash::NetworkPolicyObserver and records events for
// ash::NetworkPolicyObserver::PoliciesApplied and
// ash::NetworkPolicyObserver::PolicyAppliedtoNetwork.
class ScopedNetworkPolicyApplicationObserver
    : public ash::NetworkPolicyObserver {
 public:
  ScopedNetworkPolicyApplicationObserver() {
    ash::ManagedNetworkConfigurationHandler*
        managed_network_configuration_handler =
            ash::NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->AddObserver(this);
  }

  ~ScopedNetworkPolicyApplicationObserver() override {
    ash::ManagedNetworkConfigurationHandler*
        managed_network_configuration_handler =
            ash::NetworkHandler::Get()->managed_network_configuration_handler();
    managed_network_configuration_handler->RemoveObserver(this);
  }

  void PoliciesApplied(const std::string& userhash) override {
    policies_applied_events_.push_back(userhash);
    policies_applied_wait_loop_[userhash].Quit();
  }

  void PolicyAppliedToNetwork(const std::string& service_path) override {
    policy_applied_to_network_events_.push_back(service_path);
  }

  const std::vector<std::string>& policies_applied_events() {
    return policies_applied_events_;
  }
  const std::vector<std::string>& policy_applied_to_network_events() {
    return policy_applied_to_network_events_;
  }

  // Clears all recorded events.
  void ResetEvents() {
    policies_applied_events_.clear();
    policy_applied_to_network_events_.clear();
  }

  void WaitPoliciesApplied(const std::string& userhash) {
    policies_applied_wait_loop_[userhash].Run();
  }

 private:
  std::vector<std::string> policies_applied_events_;
  std::vector<std::string> policy_applied_to_network_events_;

  std::map<std::string, base::RunLoop> policies_applied_wait_loop_;
};

}  // namespace

// This class is used for implementing integration tests for network policy
// application across sign-in screen and/or user session.
class NetworkPolicyApplicationTest : public ash::LoginManagerTest {
 public:
  NetworkPolicyApplicationTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
    test_account_id_ = login_mixin_.users()[0].account_id;
  }

  NetworkPolicyApplicationTest(const NetworkPolicyApplicationTest&) = delete;
  NetworkPolicyApplicationTest& operator=(const NetworkPolicyApplicationTest&) =
      delete;

 protected:
  // InProcessBrowserTest:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    BrowserPolicyConnector::SetPolicyProviderForTesting(&policy_provider_);

    LoginManagerTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);

    // Allow policy fetches to fail - these tests use
    // MockConfigurationPolicyProvider.
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();

    shill_manager_client_test_ =
        ash::ShillManagerClient::Get()->GetTestInterface();
    shill_service_client_test_ =
        ash::ShillServiceClient::Get()->GetTestInterface();
    shill_profile_client_test_ =
        ash::ShillProfileClient::Get()->GetTestInterface();
    shill_device_client_test_ =
        ash::ShillDeviceClient::Get()->GetTestInterface();
    shill_service_client_test_->ClearServices();
    shill_device_client_test_->ClearDevices();
    shill_profile_client_test_->ClearProfiles();

    shill_manager_client_test_->AddTechnology(shill::kTypeWifi,
                                              true /* enabled */);
    shill_device_client_test_->AddDevice("/device/wifi1", shill::kTypeWifi,
                                         "stub_wifi_device1");
    shill_profile_client_test_->AddProfile(kSharedProfilePath, "");
    shill_service_client_test_->ClearServices();
  }

  void SetDeviceOpenNetworkConfiguration(
      const std::string& device_onc_policy_blob) {
    current_policy_.Set(key::kDeviceOpenNetworkConfiguration,
                        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                        POLICY_SOURCE_CLOUD,
                        base::Value(device_onc_policy_blob), nullptr);
    policy_provider_.UpdateChromePolicy(current_policy_);
  }

  void SetUserOpenNetworkConfiguration(
      const std::string& user_onc_policy_blob) {
    current_policy_.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
                        POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                        base::Value(user_onc_policy_blob), nullptr);
    policy_provider_.UpdateChromePolicy(current_policy_);
  }

  void OnConnectToServiceFailed(base::OnceClosure run_loop_quit_closure,
                                const std::string& error_name,
                                const std::string& error_message) {
    ADD_FAILURE() << "Connect failed with " << error_name << " - "
                  << error_message;
    std::move(run_loop_quit_closure).Run();
  }

  void ConnectToService(const std::string& service_path) {
    base::RunLoop run_loop;
    ash::ShillServiceClient::Get()->Connect(
        dbus::ObjectPath(service_path), run_loop.QuitClosure(),
        base::BindOnce(&NetworkPolicyApplicationTest::OnConnectToServiceFailed,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Unowned pointers -- just pointers to the singleton instances.
  ash::ShillManagerClient::TestInterface* shill_manager_client_test_ = nullptr;
  ash::ShillServiceClient::TestInterface* shill_service_client_test_ = nullptr;
  ash::ShillProfileClient::TestInterface* shill_profile_client_test_ = nullptr;
  ash::ShillDeviceClient::TestInterface* shill_device_client_test_ = nullptr;

  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  AccountId test_account_id_;

 private:
  testing::NiceMock<MockConfigurationPolicyProvider> policy_provider_;
  PolicyMap current_policy_;
};

// This test applies a global network policy with
// AllowOnlyPolicyNetworksToAutoconnect set to true. It then performs a user
// log-in and simulates that user policy application is slow. This is a
// regression test for https://crbug.com/936677.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       OnlyPolicyAutoconnectWithSlowUserPolicyApplication) {
  ScopedNetworkPolicyApplicationObserver network_policy_application_observer;

  // Set up two services.
  shill_service_client_test_->AddService(
      kServiceWifi1, "wifi_orig_guid_1", "WifiOne", shill::kTypeWifi,
      shill::kStateOnline, true /* add_to_visible */);
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSSIDProperty, base::Value("WifiOne"));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityPsk));

  shill_service_client_test_->AddService(
      kServiceWifi2, "wifi_orig_guid_2", "WifiTwo", shill::kTypeWifi,
      shill::kStateOnline, true /* add_to_visible */);
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi2, shill::kSSIDProperty, base::Value("WifiTwo"));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi2, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityPsk));

  // Apply device ONC policy and wait until it takes effect (one of the networks
  // auto connects).
  const char kDeviceONC[] = R"(
    {
      "GlobalNetworkConfiguration": {
        "AllowOnlyPolicyNetworksToAutoconnect": true,
        "AllowOnlyPolicyNetworksToConnect": false
      },
      "NetworkConfigurations": [
        {
          "GUID": "{device-policy-for-Wifi1}",
          "Name": "DeviceLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": true,
             "HiddenSSID": false,
             "Passphrase": "DeviceLevelWifiPwd",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  ServiceConnectedWaiter wifi_one_connected_waiter(shill_service_client_test_,
                                                   kServiceWifi1);
  shill_manager_client_test_->SetBestServiceToConnect(kServiceWifi1);
  SetDeviceOpenNetworkConfiguration(kDeviceONC);
  wifi_one_connected_waiter.Wait();

  EXPECT_THAT(
      network_policy_application_observer.policy_applied_to_network_events(),
      testing::ElementsAre(kServiceWifi1));
  EXPECT_THAT(network_policy_application_observer.policies_applied_events(),
              testing::ElementsAre(std::string() /* shill shared profile */));
  network_policy_application_observer.ResetEvents();

  absl::optional<std::string> wifi_service =
      shill_service_client_test_->FindServiceMatchingGUID(
          "{device-policy-for-Wifi1}");
  ASSERT_TRUE(wifi_service);
  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(true)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kProfileProperty,
                                   base::Value(kSharedProfilePath)));
  }

  // Manually connect to the other network.
  ConnectToService(kServiceWifi2);

  // Sign-in a user and apply user ONC policy. Simulate that shill takes a while
  // to reflect the changes back to chrome by holding back service property
  // updates (regression test for https://crbug.com/936677).
  shill_service_client_test_->SetHoldBackServicePropertyUpdates(true);

  std::string user_hash = ash::ProfileHelper::GetUserIdHashByUserIdForTesting(
      test_account_id_.GetUserEmail());
  LoginUser(test_account_id_);
  shill_profile_client_test_->AddProfile(kUserProfilePath, user_hash);

  const char kUserONC[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{user-policy-for-Wifi1}",
          "Name": "DeviceLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "DeviceLevelWifiPwd",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        },
        {
          "GUID": "{user-policy-for-Wifi2}",
          "Name": "UserLevelWifi",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": true,
             "HiddenSSID": false,
             "Passphrase": "UserLevelWifiPwd",
             "SSID": "WifiTwo",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetUserOpenNetworkConfiguration(kUserONC);
  base::RunLoop().RunUntilIdle();

  // Expect that the policies have not been signalled as applied yet because
  // property updates are being held back by FakeShillServiceClient.
  EXPECT_TRUE(
      network_policy_application_observer.policy_applied_to_network_events()
          .empty());
  EXPECT_TRUE(
      network_policy_application_observer.policies_applied_events().empty());

  shill_service_client_test_->SetHoldBackServicePropertyUpdates(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      network_policy_application_observer.policy_applied_to_network_events(),
      testing::ElementsAre(kServiceWifi1, kServiceWifi2));
  EXPECT_THAT(network_policy_application_observer.policies_applied_events(),
              testing::ElementsAre(user_hash));

  // Expect that the same service path now has the user policy GUID.
  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{user-policy-for-Wifi1}")));
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(false)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kProfileProperty,
                                   base::Value(kUserProfilePath)));
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kStateProperty,
                                   base::Value(shill::kStateIdle)));
  }

  absl::optional<std::string> wifi2_service =
      shill_service_client_test_->FindServiceMatchingGUID(
          "{user-policy-for-Wifi2}");
  ASSERT_TRUE(wifi2_service);
  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(wifi2_service.value());
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kAutoConnectProperty, base::Value(true)));
    // This service is still connected. This is an important check in this
    // regression test:
    // In https://crbug.com/936677, AutoConnectHandler was already running
    // (because OnPoliciesApplied was already triggered) when the NetworkState
    // for a policy-managed network was not marked managed yet (because shill
    // has not reflected the property changes yet). As a consequence,
    // AutoConnectHandler disconnected the current network because of the global
    // AllowOnlyPolicyNetworksToAutoconnect policy. Verify that this has not
    // happened in this test.
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kStateProperty,
                                   base::Value(shill::kStateOnline)));
  }
}

// Checks the edge case where a policy with GUID {same_guid} applies to network
// with SSID "WifiTwo", and subsequently the policy changes, the new
// NetworkConfiguration with GUID {same_guid} now applying to SSID "WifiOne".
// For this to work correctly, PolicyApplicator must first clear the "WifiTwo"
// settings so it is not matched by GUID, and then write the new policy to
// shill.
IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest,
                       PolicyWithSameGUIDAppliesToOtherSSID) {
  // Set up two services.
  shill_service_client_test_->AddService(
      kServiceWifi1, "wifi_orig_guid_1", "WifiOne", shill::kTypeWifi,
      shill::kStateOnline, true /* add_to_visible */);
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSSIDProperty, base::Value("WifiOne"));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityPsk));

  shill_service_client_test_->AddService(
      kServiceWifi2, "wifi_orig_guid_2", "WifiTwo", shill::kTypeWifi,
      shill::kStateOnline, true /* add_to_visible */);
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi2, shill::kSSIDProperty, base::Value("WifiTwo"));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi2, shill::kSecurityClassProperty,
      base::Value(shill::kSecurityPsk));

  const char kDeviceONC1[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{same_guid}",
          "Name": "X",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "Passphrase",
             "SSID": "WifiTwo",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetDeviceOpenNetworkConfiguration(kDeviceONC1);

  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi2);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kGuidProperty, base::Value("{same_guid}")));
  }

  // Same GUID for a different SSID.
  const char kDeviceONC2[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{same_guid}",
          "Name": "X",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "HiddenSSID": false,
             "Passphrase": "SomePassphrase",
             "SSID": "WifiOne",
             "Security": "WPA-PSK"
          }
        }
      ]
    })";
  SetDeviceOpenNetworkConfiguration(kDeviceONC2);
  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi2);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_FALSE(wifi_service_properties->FindKey(shill::kGuidProperty));
  }
  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(
        *wifi_service_properties,
        DictionaryHasValue(shill::kGuidProperty, base::Value("{same_guid}")));
  }
}

IN_PROC_BROWSER_TEST_F(NetworkPolicyApplicationTest, DoesNotWipeCertSettings) {
  const std::string kCertAndKeyId = "test";

  // Set up a policy-managed EAP wifi with a certificate already selected.
  shill_service_client_test_->AddService(
      kServiceWifi1, "DeviceLevelWifiGuidOrig", "DeviceLevelWifiSsid",
      shill::kTypeWifi, shill::kStateOnline, true /* add_to_visible */);
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSSIDProperty, base::Value("DeviceLevelWifiSsid"));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kSecurityClassProperty,
      base::Value(shill::kSecurity8021x));

  const char kDeviceONC1[] = R"(
    {
      "NetworkConfigurations": [
        {
          "GUID": "{DeviceLevelWifiGuid}",
          "Name": "DeviceLevelWifiName",
          "Type": "WiFi",
          "WiFi": {
             "AutoConnect": false,
             "EAP":  {
              "Outer": "EAP-TLS",
              "ClientCertType": "Pattern",
              "Identity": "test_identity",
              "ClientCertPattern": {
                "Issuer": {
                  "Organization": "Example Inc."
                }
              }
             },
             "SSID": "DeviceLevelWifiSsid",
             "Security": "WPA-EAP"
          }
        }
      ]
    })";
  {
    ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
    SetDeviceOpenNetworkConfiguration(kDeviceONC1);
    network_policy_application_observer.WaitPoliciesApplied(
        /*userhash=*/std::string());
  }

  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kEapCertIdProperty, base::Value(kCertAndKeyId));
  shill_service_client_test_->SetServiceProperty(
      kServiceWifi1, shill::kEapKeyIdProperty, base::Value(kCertAndKeyId));

  {
    // Trigger reapplication of policy for the network by changing "Identity".
    const char kDeviceONC2[] = R"(
      {
        "NetworkConfigurations": [
          {
            "GUID": "{DeviceLevelWifiGuid}",
            "Name": "DeviceLevelWifiName",
            "Type": "WiFi",
            "WiFi": {
               "AutoConnect": false,
               "EAP":  {
                "Outer": "EAP-TLS",
                "ClientCertType": "Pattern",
                "Identity": "test_identity_changed",
                "ClientCertPattern": {
                  "Issuer": {
                    "Organization": "Example Inc."
                  }
                }
               },
               "SSID": "DeviceLevelWifiSsid",
               "Security": "WPA-EAP"
            }
          }
        ]
      })";
    ScopedNetworkPolicyApplicationObserver network_policy_application_observer;
    SetDeviceOpenNetworkConfiguration(kDeviceONC2);
    network_policy_application_observer.WaitPoliciesApplied(
        /*userhash=*/std::string());
  }

  {
    const base::Value* wifi_service_properties =
        shill_service_client_test_->GetServiceProperties(kServiceWifi1);
    ASSERT_TRUE(wifi_service_properties);
    EXPECT_THAT(*wifi_service_properties,
                DictionaryHasValue(shill::kGuidProperty,
                                   base::Value("{DeviceLevelWifiGuid}")));
    // Expect that the EAP.CertId and EAP.KeyId properties have been preserved.
    const std::string* eap_cert_id =
        wifi_service_properties->FindStringKey(shill::kEapCertIdProperty);
    ASSERT_TRUE(eap_cert_id);
    EXPECT_EQ(*eap_cert_id, kCertAndKeyId);
    const std::string* eap_key_id =
        wifi_service_properties->FindStringKey(shill::kEapKeyIdProperty);
    ASSERT_TRUE(eap_key_id);
    EXPECT_EQ(*eap_key_id, kCertAndKeyId);
  }
}

}  // namespace policy
