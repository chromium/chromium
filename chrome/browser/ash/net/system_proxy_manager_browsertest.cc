// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/system_proxy_manager.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/notifications/request_system_proxy_credentials_view.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
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
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/proxy_server.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

constexpr char kRealm[] = "My proxy";
constexpr char kScheme[] = "dIgEsT";
constexpr char kProxyAuthUrl[] = "http://example.com:3128";
constexpr char kSystemProxyNotificationId[] = "system-proxy.auth_required";
constexpr char kUsername[] = "testuser";
constexpr char16_t kUsername16[] = u"testuser";
constexpr char kPassword[] = "testpwd";
constexpr char16_t kPassword16[] = u"testpwd";

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
    return SystemProxyManager::Get();
  }

  RequestSystemProxyCredentialsView* dialog() {
    return GetSystemProxyManager()->GetActiveAuthDialogForTest();
  }

  SystemProxyClient::TestInterface* client_test_interface() {
    return SystemProxyClient::Get()->GetTestInterface();
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
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt);
  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(kUsername16);
  dialog()->password_textfield_for_testing()->SetText(kPassword16);

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
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt);

  // Dialog is created.
  ASSERT_TRUE(dialog());

  // Expect warning is not shown.
  ASSERT_FALSE(dialog()->error_label_for_testing()->GetVisible());
  dialog()->username_textfield_for_testing()->SetText(kUsername16);
  dialog()->password_textfield_for_testing()->SetText(kPassword16);

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
      /*action_index=*/std::nullopt, /*reply=*/std::nullopt);
  ASSERT_TRUE(dialog());

  // Expect warning is shown.
  EXPECT_TRUE(dialog()->error_label_for_testing()->GetVisible());
}

class SystemProxyManagerPolicyCredentialsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  SystemProxyManagerPolicyCredentialsBrowserTest() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
    device_state_.set_skip_initial_policy_setup(true);
  }
  SystemProxyManagerPolicyCredentialsBrowserTest(
      const SystemProxyManagerPolicyCredentialsBrowserTest&) = delete;
  SystemProxyManagerPolicyCredentialsBrowserTest& operator=(
      const SystemProxyManagerPolicyCredentialsBrowserTest&) = delete;
  ~SystemProxyManagerPolicyCredentialsBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    constexpr std::string_view kAffiliationID = "id";
    // Initialize device policy.
    auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
        FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &policy_helper_, std::array{kAffiliationID}));

    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
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
        {kSystemProxySettings});
    RunUntilIdle();
  }

  void SetOncPolicy(const std::string& policy_json, policy::PolicyScope scope) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kOpenNetworkConfiguration,
               policy::POLICY_LEVEL_MANDATORY, scope,
               policy::POLICY_SOURCE_CLOUD, base::Value(policy_json), nullptr);
    provider_.UpdateChromePolicy(policy);
    RunUntilIdle();
  }

  void DisconnectNetworkService(const std::string& service_path) {
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();
    base::Value value(shill::kStateIdle);
    service_test->SetServiceProperty(service_path, shill::kStateProperty,
                                     value);
    RunUntilIdle();
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
    RunUntilIdle();
  }

  void SetProxyConfigForNetworkService(const std::string& service_path,
                                       base::Value::Dict proxy_config) {
    ProxyConfigDictionary proxy_config_dict(std::move(proxy_config));
    DCHECK(NetworkHandler::IsInitialized());
    const NetworkState* network =
        NetworkHandler::Get()->network_state_handler()->GetNetworkState(
            service_path);
    ASSERT_TRUE(network);
    proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
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

  SystemProxyClient::TestInterface* client_test_interface() {
    return SystemProxyClient::Get()->GetTestInterface();
  }

 private:
  void SetupNetworkEnvironment() {
    ShillProfileClient::TestInterface* profile_test =
        ShillProfileClient::Get()->GetTestInterface();
    ShillServiceClient::TestInterface* service_test =
        ShillServiceClient::Get()->GetTestInterface();

    profile_test->AddProfile(kUserProfilePath, "user");

    service_test->ClearServices();
    ConnectWifiNetworkService(kDefaultServicePath, kDefaultServiceSsid,
                              kDefaultServiceGuid);
  }
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  ScopedStubInstallAttributes test_install_attributes_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
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
  auto proxy_config = base::Value::Dict()
                          .Set("mode", ProxyPrefs::kPacScriptProxyModeName)
                          .Set("pac_url", "http://proxy");
  browser()->profile()->GetPrefs()->SetDict(::proxy_config::prefs::kProxy,
                                            std::move(proxy_config));
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
  auto proxy_config = base::Value::Dict()
                          .Set("mode", ProxyPrefs::kFixedServersProxyModeName)
                          .Set("server", "proxy:8080");
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

namespace {
constexpr char kProxyUsername[] = "foo";
constexpr char16_t kProxyUsername16[] = u"foo";
constexpr char kProxyPassword[] = "bar";
constexpr char16_t kProxyPassword16[] = u"bar";
constexpr char kBadUsername[] = "bad-username";
constexpr char kBadPassword[] = "bad-pwd";
constexpr char kOriginHostname[] = "a.test";
}  // namespace

class SystemProxyCredentialsReuseBrowserTest
    : public SystemProxyManagerPolicyCredentialsBrowserTest {
 public:
  SystemProxyCredentialsReuseBrowserTest()
      : proxy_server_(std::make_unique<net::SpawnedTestServer>(
            net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
            base::FilePath())) {}
  SystemProxyCredentialsReuseBrowserTest(
      const SystemProxyCredentialsReuseBrowserTest&) = delete;
  SystemProxyCredentialsReuseBrowserTest& operator=(
      const SystemProxyCredentialsReuseBrowserTest&) = delete;
  ~SystemProxyCredentialsReuseBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kOriginHostname, "127.0.0.1");
    proxy_server_->set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(proxy_server_->Start());

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    net::test_server::RegisterDefaultHandlers(https_server_.get());
    ASSERT_TRUE(https_server_->Start());
  }

 protected:
  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetManagedProxy() {
    // Configure a proxy via user policy.
    auto proxy_config =
        base::Value::Dict()
            .Set("mode", ProxyPrefs::kFixedServersProxyModeName)
            .Set("server", proxy_server_->host_port_pair().ToString());
    browser()->profile()->GetPrefs()->SetDict(::proxy_config::prefs::kProxy,
                                              std::move(proxy_config));
    RunUntilIdle();
  }

  GURL GetServerUrl(const std::string& page) {
    return https_server_->GetURL(kOriginHostname, page);
  }

  // Navigates to the test page "/simple.html" and authenticates in the proxy
  // login dialog with `username` and `password`.
  void LoginWithDialog(const std::u16string& username,
                       const std::u16string& password) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetServerUrl("/simple.html")));
    ASSERT_TRUE(base::test::RunUntil([]() {
      return LoginHandler::GetAllLoginHandlersForTest().size() == 1;
    }));
    LoginHandler* login_handler =
        LoginHandler::GetAllLoginHandlersForTest().front();
    login_handler->SetAuth(username, password);
  }

  void CheckEntryInHttpAuthCache(const std::string& auth_scheme,
                                 const std::string& expected_username,
                                 const std::string& expected_password) {
    network::mojom::NetworkContext* network_context =
        browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext();
    std::string username;
    std::string password;
    base::RunLoop loop;
    network_context->LookupProxyAuthCredentials(
        net::ProxyServer(net::ProxyServer::SCHEME_HTTP,
                         proxy_server_->host_port_pair()),
        auth_scheme, "MyRealm1",
        base::BindOnce(
            [](std::string* username, std::string* password,
               base::OnceClosure closure,
               const std::optional<net::AuthCredentials>& credentials) {
              if (credentials) {
                *username = base::UTF16ToUTF8(credentials->username());
                *password = base::UTF16ToUTF8(credentials->password());
              }
              std::move(closure).Run();
            },
            &username, &password, loop.QuitClosure()));
    loop.Run();
    EXPECT_EQ(username, expected_username);
    EXPECT_EQ(password, expected_password);
  }

  SystemProxyManager* GetSystemProxyManager() {
    return SystemProxyManager::Get();
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  // A proxy server which requires authentication using the 'Basic'
  // authentication method.
  std::unique_ptr<net::SpawnedTestServer> proxy_server_;
};

// Verifies that the policy provided credentials are not used for regular users.
IN_PROC_BROWSER_TEST_F(SystemProxyCredentialsReuseBrowserTest, RegularUser) {
  base::AddTagToTestResult("feature_id",
                             "screenplay-04b4c463-f720-4cd4-9e50-7ee009d9a241");
  SetManagedProxy();
  SetPolicyCredentials(kProxyUsername, kProxyPassword);
  LoginWithDialog(kProxyUsername16, kProxyPassword16);
  CheckEntryInHttpAuthCache("Basic", kProxyUsername, kProxyPassword);
}

// Verifies that the policy provided credentials are used for MGS.
IN_PROC_BROWSER_TEST_F(SystemProxyCredentialsReuseBrowserTest,
                       PolicyCredentialsUsed) {
  SetManagedProxy();
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  SetPolicyCredentials(kProxyUsername, kProxyPassword);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetServerUrl("/simple.html")));
  CheckEntryInHttpAuthCache("Basic", kProxyUsername, kProxyPassword);
}

// Verifies that if the policy provided proxy credentials are not correct in a
// MGS, then the user is prompted for credentials.
IN_PROC_BROWSER_TEST_F(SystemProxyCredentialsReuseBrowserTest,
                       BadPolicyCredentials) {
  SetManagedProxy();
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  SetPolicyCredentials(kBadUsername, kBadPassword);
  LoginWithDialog(kProxyUsername16, kProxyPassword16);
  CheckEntryInHttpAuthCache("Basic", kProxyUsername, kProxyPassword);
}

// Verifies that the policy provided proxy credentials are only used for
// authentication schemes allowed by the SystemProxySettings policy.
IN_PROC_BROWSER_TEST_F(SystemProxyCredentialsReuseBrowserTest,
                       RestrictedPolicyCredentials) {
  SetManagedProxy();
  LoginState::Get()->SetLoggedInState(
      LoginState::LOGGED_IN_ACTIVE, LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
  SetPolicyCredentials(kProxyUsername, kProxyPassword, R"("ntlm","digest")");
  LoginWithDialog(kProxyUsername16, kProxyPassword16);
  CheckEntryInHttpAuthCache("Basic", kProxyUsername, kProxyPassword);
}

}  // namespace ash
