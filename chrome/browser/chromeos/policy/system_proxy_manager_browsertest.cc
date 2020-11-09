// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/system_proxy_manager.h"
#include "chrome/browser/chromeos/ui/request_system_proxy_credentials_view.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

using testing::_;
using testing::Return;

namespace policy {

namespace {
constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "dIgEsT";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kSystemProxyNotificationId[] = "system-proxy.auth_required";
constexpr char kUsername[] = "testuser";
constexpr char kPassword[] = "testpwd";

constexpr char kUserProfilePath[] = "user_profile";
constexpr char kDefaultServicePath[] = "default_wifi";
constexpr char kDefaultServiceSsid[] = "default_wifi_guid";
constexpr char kDefaultServiceGuid[] = "eth0";

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
              },
              "SecureHTTPProxy": {
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

constexpr char kONCPolicyWifi0AltProxy[] =
    R"({
      "NetworkConfigurations": [ {
         "GUID": "{wifi0_guid}",
         "Name": "wifi0",
         "ProxySettings": {
            "Manual": {
               "HTTPProxy": {
                  "Host": "proxyhostalt",
                  "Port": 3129
               },
               "SecureHTTPProxy": {
                  "Host": "proxyhostalt",
                  "Port": 3129
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

constexpr char kSystemProxyPolicyJson[] =
    R"({
       "system_proxy_enabled": true,
       "system_services_username": "%s",
       "system_services_password": "%s",
       %s
    })";

constexpr char kAuthSchemesPolicyEntry[] =
    R"("policy_credentials_auth_schemes": [%s],)";

void RunUntilIdle() {
  DCHECK(base::CurrentThread::Get());
  base::RunLoop loop;
  loop.RunUntilIdle();
}
}  // namespace

class SystemProxyManagerBrowserTest : public InProcessBrowserTest {
 public:
  SystemProxyManagerBrowserTest() = default;
  SystemProxyManagerBrowserTest(const SystemProxyManagerBrowserTest&) = delete;
  SystemProxyManagerBrowserTest& operator=(
      const SystemProxyManagerBrowserTest&) = delete;
  ~SystemProxyManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GetSystemProxyManager()->StartObservingPrimaryProfilePrefs(
        browser()->profile());
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(/*profile=*/nullptr);
    GetSystemProxyManager()->SetSystemProxyEnabledForTest(true);
  }

  void TearDownOnMainThread() override {
    GetSystemProxyManager()->SetSystemProxyEnabledForTest(false);
  }

 protected:
  SystemProxyManager* GetSystemProxyManager() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetSystemProxyManager();
  }

  chromeos::RequestSystemProxyCredentialsView* dialog() {
    return GetSystemProxyManager()->GetActiveAuthDialogForTest();
  }

  chromeos::SystemProxyClient::TestInterface* client_test_interface() {
    return chromeos::SystemProxyClient::Get()->GetTestInterface();
  }

  void SendAuthenticationRequest(bool bad_cached_credentials) {
    system_proxy::ProtectionSpace protection_space;
    protection_space.set_origin(kProxyAuthUrl);
    protection_space.set_scheme(kScheme);
    protection_space.set_realm(kRealm);

    system_proxy::AuthenticationRequiredDetails details;
    details.set_bad_cached_credentials(bad_cached_credentials);
    *details.mutable_proxy_protection_space() = protection_space;

    client_test_interface()->SendAuthenticationRequiredSignal(details);
  }

  void WaitForNotification() {
    base::RunLoop run_loop;
    display_service_tester_->SetNotificationAddedClosure(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

// Tests the flow for setting user credentials for System-proxy:
// - Receiving an authentication request prompts a notification;
// - Clicking on the notification opens a dialog;
// - Credentials introduced in the dialog are sent via D-Bus to System-proxy.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest, AuthenticationDialog) {
  base::RunLoop run_loop;
  GetSystemProxyManager()->SetSendAuthDetailsClosureForTest(
      run_loop.QuitClosure());

  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ false);
  WaitForNotification();

  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);
  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kUsername));
  dialog()->password_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kPassword));

  // Simulate clicking on "OK" button.
  dialog()->Accept();

  // Wait for the callback set via |SetSendAuthDetailsClosureForTest| to be
  // called. The callback will be called when SystemProxyManager calls the D-Bus
  // method |SetAuthenticationDetails|
  run_loop.Run();

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(request.credentials().username(), kUsername);
  EXPECT_EQ(request.credentials().password(), kPassword);

  // Verify that the UI elements are reset.
  GetSystemProxyManager()->CloseAuthDialogForTest();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  EXPECT_FALSE(dialog());
}

// Tests that canceling the authentication dialog sends empty credentials to
// System-proxy.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest,
                       CancelAuthenticationDialog) {
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ false);
  WaitForNotification();
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);

  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kUsername));
  dialog()->password_textfield_for_testing()->SetText(
      base::ASCIIToUTF16(kPassword));

  base::RunLoop run_loop;
  GetSystemProxyManager()->SetSendAuthDetailsClosureForTest(
      run_loop.QuitClosure());
  // Simulate clicking on "Cancel" button.
  dialog()->Cancel();
  run_loop.Run();

  system_proxy::SetAuthenticationDetailsRequest request =
      client_test_interface()->GetLastAuthenticationDetailsRequest();

  ASSERT_TRUE(request.has_credentials());
  EXPECT_EQ(request.credentials().username(), "");
  EXPECT_EQ(request.credentials().password(), "");

  // Verify that the UI elements are reset.
  GetSystemProxyManager()->CloseAuthDialogForTest();
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  EXPECT_FALSE(dialog());
}

// Tests that the warning informing the user that the previous credentials are
// incorrect is shown in the UI.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerBrowserTest,
                       BadCachedCredentialsWarning) {
  EXPECT_FALSE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));
  SendAuthenticationRequest(/* bad_cached_credentials = */ true);
  WaitForNotification();
  EXPECT_TRUE(
      display_service_tester_->GetNotification(kSystemProxyNotificationId));

  display_service_tester_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, kSystemProxyNotificationId,
      /*action_index=*/base::nullopt, /*reply=*/base::nullopt);
  ASSERT_TRUE(dialog());

  // Expect warning is shown.
  EXPECT_TRUE(dialog()->error_label_for_testing()->GetVisible());
}

class SystemProxyManagerPolicyCredentialsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SystemProxyManagerPolicyCredentialsBrowserTest() {
    device_state_.SetState(
        chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
    device_state_.set_skip_initial_policy_setup(true);
  }
  SystemProxyManagerPolicyCredentialsBrowserTest(
      const SystemProxyManagerPolicyCredentialsBrowserTest&) = delete;
  SystemProxyManagerPolicyCredentialsBrowserTest& operator=(
      const SystemProxyManagerPolicyCredentialsBrowserTest&) = delete;
  ~SystemProxyManagerPolicyCredentialsBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    chromeos::SessionManagerClient::InitializeFakeInMemory();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    const std::string kAffiliationID = "id";
    // Initialize device policy.
    std::set<std::string> device_affiliation_ids;
    device_affiliation_ids.insert(kAffiliationID);
    auto affiliation_helper = AffiliationTestHelper::CreateForCloud(
        chromeos::FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
        &policy_helper_, device_affiliation_ids)));

    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override { SetupNetworkEnvironment(); }

 protected:
  void SetPolicyCredentials(const std::string& username,
                            const std::string& password,
                            const std::string& auth_schemes = "") {
    const std::string schemes_json_entry =
        auth_schemes.empty()
            ? ""
            : base::StringPrintf(kAuthSchemesPolicyEntry, auth_schemes.c_str());
    const std::string policy_value =
        base::StringPrintf(kSystemProxyPolicyJson, username.c_str(),
                           password.c_str(), schemes_json_entry.c_str());
    enterprise_management::ChromeDeviceSettingsProto& proto(
        policy_helper_.device_policy()->payload());
    proto.mutable_system_proxy_settings()->set_system_proxy_settings(
        policy_value);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {chromeos::kSystemProxySettings});
    RunUntilIdle();
  }

  void SetOncPolicy(const std::string& policy_json, PolicyScope scope) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_LEVEL_MANDATORY, scope,
               policy::POLICY_SOURCE_CLOUD, base::Value(policy_json), nullptr);
    provider_.UpdateChromePolicy(policy);
    RunUntilIdle();
  }

  void DisconnectNetworkService(const std::string& service_path) {
    chromeos::ShillServiceClient::TestInterface* service_test =
        chromeos::DBusThreadManager::Get()
            ->GetShillServiceClient()
            ->GetTestInterface();
    base::Value value(shill::kStateIdle);
    service_test->SetServiceProperty(service_path, shill::kStateProperty,
                                     value);
    RunUntilIdle();
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
    RunUntilIdle();
  }

  void SetProxyConfigForNetworkService(const std::string& service_path,
                                       base::Value proxy_config) {
    ProxyConfigDictionary proxy_config_dict(std::move(proxy_config));
    DCHECK(chromeos::NetworkHandler::IsInitialized());
    const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                                ->network_state_handler()
                                                ->GetNetworkState(service_path);
    ASSERT_TRUE(network);
    chromeos::proxy_config::SetProxyConfigForNetwork(proxy_config_dict,
                                                     *network);
  }

  void ExpectSystemCredentialsSent(
      const std::string& username,
      const std::string& password,
      const std::vector<std::string>& auth_schemes = {}) {
    system_proxy::SetAuthenticationDetailsRequest request =
        client_test_interface()->GetLastAuthenticationDetailsRequest();
    EXPECT_EQ(username, request.credentials().username());
    EXPECT_EQ(password, request.credentials().password());
    ASSERT_EQ(
        auth_schemes.size(),
        static_cast<size_t>(
            request.credentials().policy_credentials_auth_schemes().size()));
    for (size_t i = 0; i < auth_schemes.size(); ++i) {
      EXPECT_EQ(request.credentials().policy_credentials_auth_schemes()[i],
                auth_schemes[i]);
    }
    EXPECT_EQ(system_proxy::TrafficOrigin::SYSTEM, request.traffic_type());
  }

  chromeos::SystemProxyClient::TestInterface* client_test_interface() {
    return chromeos::SystemProxyClient::Get()->GetTestInterface();
  }

 private:
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
    ConnectWifiNetworkService(kDefaultServicePath, kDefaultServiceSsid,
                              kDefaultServiceGuid);
  }
  chromeos::DeviceStateMixin device_state_{
      &mixin_host_, chromeos::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  chromeos::ScopedStubInstallAttributes test_install_attributes_;
  MockConfigurationPolicyProvider provider_;
  DevicePolicyCrosTestHelper policy_helper_;
};

// Tests that the SystemProxyManager syncs credentials correctly for managed
// proxies configured via device ONC policy. It also tests the overall policy
// credentials syncing behaviour, more specifically that SystemProxyManager:
// - sends a D-Bus request when enabled by policy;
// - doesn't send redundant requests to set credentials, i.e. when the default
// network is updated;
// - sends requests to clear credentials if the network is not managed.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerPolicyCredentialsBrowserTest,
                       DeviceONCPolicyUpdates) {
  int set_auth_details_call_count = 0;
  SetPolicyCredentials(/*username=*/"", /*password=*/"");
  // Expect that if System-proxy policy is enabled, one initial call to set
  // empty credentials is sent, regardless of network configuration.
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("", "");
  DisconnectNetworkService(kDefaultServicePath);

  ConnectWifiNetworkService(kWifiServicePath, kWifiGuid, kWifiSsid);

  // Set an ONC policy with proxy configuration and expect that no D-Bus call to
  // update credentials is sent, since the credentials were not changed.
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_MACHINE);
  EXPECT_EQ(set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  // Set policy credentials for the remote proxy and expect that a D-Bus request
  // to set policy credentials was sent.
  SetPolicyCredentials(kUsername, kPassword);
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent(kUsername, kPassword);

  // Connect to a different managed proxy and expect that no additional request
  // is sent to System-proxy since credentials were not changed.
  SetOncPolicy(kONCPolicyWifi0AltProxy, policy::POLICY_SCOPE_MACHINE);
  EXPECT_EQ(set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());

  // Change credentials and expect that a D-Bus request to set the new
  // credentials is sent to System-proxy.
  SetPolicyCredentials("test_user_alt", "test_pwd_alt");
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("test_user_alt", "test_pwd_alt");

  DisconnectNetworkService(kWifiServicePath);

  // Expect credentials are cleared on non-managed networks.
  ConnectWifiNetworkService(kDefaultServicePath, kDefaultServiceSsid,
                            kDefaultServiceGuid);
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("", "");
}

// Tests that the SystemProxyManager syncs credentials correctly for managed
// proxies configured via user ONC policy.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerPolicyCredentialsBrowserTest,
                       UserONCPolicy) {
  int set_auth_details_call_count = 0;
  SetPolicyCredentials(kUsername, kPassword);
  // Expect that credentials were not sent, since there's no managed proxy
  // configured.
  ExpectSystemCredentialsSent("", "");
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  DisconnectNetworkService(kDefaultServicePath);

  // Configure a proxy via user ONC policy and expect that credentials were
  // forwarded to System-proxy.
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);
  ConnectWifiNetworkService(kWifiServicePath, kWifiGuid, kWifiSsid);

  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent(kUsername, kPassword);
}

// Tests that the SystemProxyManager syncs credentials correctly for managed
// proxies configured via the user policy which sets the kProxy pref.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerPolicyCredentialsBrowserTest,
                       UserPolicy) {
  int set_auth_details_call_count = 0;
  SetPolicyCredentials(kUsername, kPassword);
  // Expect that credentials were not sent, since there's no managed proxy
  // configured.
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("", "");

  // Configure a proxy via user policy.
  base::Value proxy_config(base::Value::Type::DICTIONARY);
  proxy_config.SetKey("mode", base::Value(ProxyPrefs::kPacScriptProxyModeName));
  proxy_config.SetKey("pac_url", base::Value("http://proxy"));
  browser()->profile()->GetPrefs()->Set(proxy_config::prefs::kProxy,
                                        proxy_config);
  RunUntilIdle();
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent(kUsername, kPassword);
}

// Tests that the SystemProxyManager doesn't send credentials for user
// configured proxies.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerPolicyCredentialsBrowserTest,
                       UserSetProxy) {
  SetPolicyCredentials(kUsername, kPassword);
  base::Value proxy_config(base::Value::Type::DICTIONARY);
  proxy_config.SetKey("mode",
                      base::Value(ProxyPrefs::kFixedServersProxyModeName));
  proxy_config.SetKey("server", base::Value("proxy:8080"));
  SetProxyConfigForNetworkService(kDefaultServicePath, std::move(proxy_config));
  RunUntilIdle();
  int set_auth_details_call_count = 0;
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("", "");
}

// Tests that the SystemProxyManager forwards the authentication schemes set by
// policy to System-proxy via D-Bus.
IN_PROC_BROWSER_TEST_F(SystemProxyManagerPolicyCredentialsBrowserTest,
                       PolicySetAuthSchemes) {
  int set_auth_details_call_count = 0;
  SetPolicyCredentials(kUsername, kPassword, R"("basic","digest")");
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent("", "", {"basic", "digest"});
  DisconnectNetworkService(kDefaultServicePath);
  // Configure a proxy via user ONC policy and expect that credentials were
  // forwarded to System-proxy.
  SetOncPolicy(kONCPolicyWifi0Proxy, policy::POLICY_SCOPE_USER);
  ConnectWifiNetworkService(kWifiServicePath, kWifiGuid, kWifiSsid);
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent(kUsername, kPassword, {"basic", "digest"});

  SetPolicyCredentials(kUsername, kPassword, R"("ntlm")");
  EXPECT_EQ(++set_auth_details_call_count,
            client_test_interface()->GetSetAuthenticationDetailsCallCount());
  ExpectSystemCredentialsSent(kUsername, kPassword, {"ntlm"});
}

}  // namespace policy
