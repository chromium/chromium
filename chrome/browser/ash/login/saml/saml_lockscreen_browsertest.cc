// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "components/account_id/account_id.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "dbus/object_path.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-1";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kWifiServicePath[] = "/service/wifi1";
constexpr char kEthServicePath[] = "/service/eth1";

constexpr char kSAMLIdPCookieName[] = "saml";
constexpr char kSAMLIdPCookieValue[] = "value";
constexpr std::string_view kAffiliationID = "test id";

constexpr char kSAMLLink[] = "link";
constexpr char kSAMLLinkedPageURLPattern[] =
    "*"
    "/linked";

void ErrorCallbackFunction(base::OnceClosure run_loop_quit_closure,
                           const std::string& error_name,
                           const std::string& error_message) {
  std::move(run_loop_quit_closure).Run();
  FAIL() << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

void SetDisconnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Disconnect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

// TODO: b/314327647 - move tests which don't depend on SAML to another
// file, or rename this file to lock_screen_online_reauth_browsertest.cc
// and move it elsewhere.
class LockscreenWebUiTest : public MixinBasedInProcessBrowserTest {
 public:
  LockscreenWebUiTest() = default;
  LockscreenWebUiTest(const LockscreenWebUiTest&) = delete;
  LockscreenWebUiTest& operator=(const LockscreenWebUiTest&) = delete;

  ~LockscreenWebUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
    // TODO(crbug.com/1177416) - Fix this with a proper SSL solution.
    command_line->AppendSwitch(::switches::kIgnoreCertificateErrors);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();

    host_resolver()->AddRule("*", "127.0.0.1");

    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

    // TODO(b/314327647): remove dependency on SAML setup and add tests for
    // online reauth with pure Gaia.
    fake_gaia_mixin()->fake_gaia()->RegisterSamlUser(
        FakeGaiaMixin::kEnterpriseUser1, fake_saml_idp_.GetSamlPageUrl());

    fake_gaia_mixin()->set_initialize_configuration(false);
    fake_gaia_mixin()->fake_gaia()->SetConfigurationHelper(
        FakeGaiaMixin::kEnterpriseUser1, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);
    fake_gaia_mixin()->SetupFakeGaiaForLogin(FakeGaiaMixin::kEnterpriseUser1,
                                             "", kTestRefreshToken);

    // Set up fake networks.
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/true);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { network_state_test_helper_.reset(); }

  void Login() {
    logged_in_user_mixin_.LogInUser();
    PerformPostLoginSetup();
  }

  void LoginWithoutUpdatingPolicies() {
    logged_in_user_mixin_.LogInUser(
        {ash::LoggedInUserMixin::LoginDetails::kNoPolicyForUser});
    PerformPostLoginSetup();
  }

  void PerformPostLoginSetup() {
    // Because the `logged_in_user_mixin_` uses a stub authenticator, we need to
    // also configure the fake UserDataAuth, otherwise lock-screen flow fails.
    cryptohome_mixin_.MarkUserAsExisting(GetAccountId());

    // Mark user as a SAML user: in production this would be done during online
    // sign in on the login screen, which we skip in these tests.
    user_manager::UserManager::Get()->SetUserUsingSaml(
        GetAccountId(), /*using_saml=*/true,
        /*using_saml_principals_api=*/false);
  }

  AccountId GetAccountId() { return logged_in_user_mixin_.GetAccountId(); }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_; }

  FakeGaiaMixin* fake_gaia_mixin() {
    return logged_in_user_mixin_.GetFakeGaiaMixin();
  }

 protected:
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;

 private:
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kManaged};

  FakeSamlIdpMixin fake_saml_idp_{&mixin_host_, fake_gaia_mixin()};
};

// TODO(b/276829737): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowNetworkDialog DISABLED_ShowNetworkDialog
#else
#define MAYBE_ShowNetworkDialog ShowNetworkDialog
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_ShowNetworkDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  reauth_dialog_helper->ShowNetworkScreenAndWait();

  // Ensures that the web element 'cr-dialog' is really visible.
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Click on the actual button to close the dialog.
  reauth_dialog_helper->ClickCloseNetworkButton();
  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN and linux-chromeos-rel.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TriggerDialogOnNetworkOff DISABLED_TriggerDialogOnNetworkOff
#else
#define MAYBE_TriggerDialogOnNetworkOff TriggerDialogOnNetworkOff
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_TriggerDialogOnNetworkOff) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateIdle, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Click on the actual button to close the dialog.
  reauth_dialog_helper->ClickCloseNetworkButton();
  // Ensures that both dialogs are closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, TriggerAndHideNetworkDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateIdle, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Reconnect network.
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOnline, /*visible=*/true);

  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();

  // Ensures that the network dialog is closed.
  reauth_dialog_helper->ExpectNetworkDialogHidden();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, CaptivePortal) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateIdle, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Change network to be behind a captive portal.
  network_state_test_helper_->service_test()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateRedirectFound));

  reauth_dialog_helper->WaitForCaptivePortalDialogToLoad();
  reauth_dialog_helper->WaitForCaptivePortalDialogToShow();
  reauth_dialog_helper->ExpectCaptivePortalDialogVisible();

  // User actions on captive portal page should lead to network becoming online,
  // so instead of mocking a portal page we simply switch the network state.
  network_state_test_helper_->service_test()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateOnline));

  reauth_dialog_helper->WaitForCaptivePortalDialogToClose();

  // Ensures that captive portal and network dialogs are closed.
  reauth_dialog_helper->ExpectCaptivePortalDialogHidden();
  reauth_dialog_helper->ExpectNetworkDialogHidden();
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if BUILDFLAG(IS_CHROMEOS) || defined(MEMORY_SANITIZER)
#define MAYBE_TriggerAndHideCaptivePortalDialog \
  DISABLED_TriggerAndHideCaptivePortalDialog
#else
#define MAYBE_TriggerAndHideCaptivePortalDialog \
  TriggerAndHideCaptivePortalDialog
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest,
                       MAYBE_TriggerAndHideCaptivePortalDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // This test uses NetworkHandlerTestHelper instead of NetworkStateTestHelper
  // because closing captive portal dialog involves a call to
  // NetworkHandler::Get()
  NetworkHandlerTestHelper network_test_helper;
  network_test_helper.manager_test()->SetupDefaultEnvironment();

  // Disconnect from all networks in order to trigger the network screen.
  network_test_helper.service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_test_helper.service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateIdle, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  auto TriggerAndCloseCaptivePortal = [&network_test_helper,
                                       &reauth_dialog_helper] {
    // Change network to be behind a captive portal.
    network_test_helper.service_test()->SetServiceProperty(
        kWifiServicePath, shill::kStateProperty,
        base::Value(shill::kStateRedirectFound));

    reauth_dialog_helper->WaitForCaptivePortalDialogToLoad();
    reauth_dialog_helper->WaitForCaptivePortalDialogToShow();
    reauth_dialog_helper->ExpectCaptivePortalDialogVisible();

    // Close captive portal dialog and check that we are back to network dialog
    reauth_dialog_helper->CloseCaptivePortalDialogAndWait();
    reauth_dialog_helper->ExpectCaptivePortalDialogHidden();
    reauth_dialog_helper->ExpectNetworkDialogVisible();
  };
  // Check that captive portal dialog can be opened and closed multiple times
  TriggerAndCloseCaptivePortal();
  TriggerAndCloseCaptivePortal();

  // Close all dialogs at the end of the test - otherwise these tests crash
  reauth_dialog_helper->ClickCloseNetworkButton();
  reauth_dialog_helper->WaitForReauthDialogToClose();
}

// Sets up proxy server which requires authentication.
class ProxyAuthLockscreenWebUiTest : public LockscreenWebUiTest {
 public:
  ProxyAuthLockscreenWebUiTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath()) {}

  ProxyAuthLockscreenWebUiTest(const ProxyAuthLockscreenWebUiTest&) = delete;
  ProxyAuthLockscreenWebUiTest& operator=(const ProxyAuthLockscreenWebUiTest&) =
      delete;

  ~ProxyAuthLockscreenWebUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LockscreenWebUiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LockscreenWebUiTest::SetUpOnMainThread();

    // Disconnect unneeded wifi network - these tests use only the network which
    // corresponds to `kEthServicePath`
    SetDisconnected(kWifiServicePath);
    ConfigureNetworkBehindProxy();
  }

  void SetUp() override {
    proxy_server_.set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(proxy_server_.Start());
    LockscreenWebUiTest::SetUp();
  }

 private:
  // Configure settings which are neccesarry for `NetworkStateInformer` to
  // report `NetworkStateInformer::PROXY_AUTH_REQUIRED` in the tests.
  void ConfigureNetworkBehindProxy() {
    ProxyConfigDictionary proxy_config_dict(
        ProxyConfigDictionary::CreateFixedServers(
            proxy_server_.host_port_pair().ToString(), ""));
    const NetworkState* network =
        network_state_test_helper_->network_state_handler()->DefaultNetwork();
    ASSERT_TRUE(network);
    ASSERT_EQ(network->guid(),
              FakeShillManagerClient::kFakeEthernetNetworkGuid);

    proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
    base::RunLoop().RunUntilIdle();
  }

  net::SpawnedTestServer proxy_server_;
};

// TODO(b/343013116): Flaky on linux-chromeos-rel.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SwitchToProxyNetwork DISABLED_SwitchToProxyNetwork
#else
#define MAYBE_SwitchToProxyNetwork SwitchToProxyNetwork
#endif
IN_PROC_BROWSER_TEST_F(ProxyAuthLockscreenWebUiTest,
                       MAYBE_SwitchToProxyNetwork) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Start with disconnected network.
  SetDisconnected(kEthServicePath);

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();
  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // No networks are connected so we should start on the network screen.
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Connect to a network behind proxy.
  SetConnected(kEthServicePath);

  reauth_dialog_helper->ExpectNetworkDialogHidden();

  reauth_dialog_helper->WaitForSigninWebview();

  // Wait for http auth dialog and authenticate.
  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 1; }));
  HttpAuthDialog::GetAllDialogsForTest().front()->SupplyCredentialsForTest(
      u"foo", u"bar");

  reauth_dialog_helper->WaitForPrimaryGaiaButtonToBeEnabled();
  reauth_dialog_helper->ClickPrimaryGaiaButton();

  reauth_dialog_helper->WaitForSamlIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();

  // We should no longer be using the ash http auth dialog.
  EXPECT_FALSE(HttpAuthDialog::IsEnabled());
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
// TODO(crbug.com/40272814): Flaky on linux-chromeos-rel.
#if defined(MEMORY_SANITIZER) || \
    (defined(NDEBUG) && !defined(ADDRESS_SANITIZER))
#define MAYBE_ProxyAuthCanBeCancelled DISABLED_ProxyAuthCanBeCancelled
#else
#define MAYBE_ProxyAuthCanBeCancelled ProxyAuthCanBeCancelled
#endif
IN_PROC_BROWSER_TEST_F(ProxyAuthLockscreenWebUiTest,
                       MAYBE_ProxyAuthCanBeCancelled) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();
  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  reauth_dialog_helper->WaitForSigninWebview();

  // Appearance of http auth dialog means that proxy authentication was
  // requested.
  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 1; }));

  // Cancel proxy authentication
  HttpAuthDialog::GetAllDialogsForTest().front()->CancelForTest();
  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 0; }));

  // Expect to end up on the network screen
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Close all dialogs at the end of the test - otherwise these tests crash
  reauth_dialog_helper->ClickCloseNetworkButton();
}

class AutoStartTest : public LockscreenWebUiTest {
 public:
  AutoStartTest() = default;
  AutoStartTest(const AutoStartTest&) = delete;
  AutoStartTest& operator=(const AutoStartTest&) = delete;
  ~AutoStartTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    LockscreenWebUiTest::SetUpInProcessBrowserTestFixture();

    // Enable auto-start user policy which makes online reauth dialog on the
    // lock screen open automatically when online reauth is required.
    provider.SetDefaultReturns(/*is_initialization_complete_return=*/true,
                               /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider);
    policy::PolicyMap policy;
    policy.Set(policy::key::kLockScreenAutoStartOnlineReauth,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    provider.UpdateChromePolicy(policy);
  }

  void ForceOnlineReauthOnLockScreen() {
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    ASSERT_TRUE(profile);
    LockScreenReauthManager* lock_screen_reauth_manager =
        LockScreenReauthManagerFactory::GetForProfile(profile);
    ASSERT_TRUE(lock_screen_reauth_manager);
    // Force online reauth on the lock screen as if SAML time limit
    // policy demands it. For auto start it is important to just have
    // reauth forced, specific reason doesn't matter.
    lock_screen_reauth_manager->MaybeForceReauthOnLockScreen(
        ReauthReason::kSamlLockScreenReauthPolicy);
  }

  void ExpectSuccessfulAutoStart() {
    std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
        LockScreenReauthDialogTestHelper::InitForShownDialog();
    // `reauth_dialog_helper` not being empty confirms that online reauth
    // dialog is shown.
    EXPECT_TRUE(reauth_dialog_helper);

    // Wait for the webview and SAML IdP page to load.
    reauth_dialog_helper->WaitForSigninWebview();
    reauth_dialog_helper->WaitForSamlIdpPageLoad();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider;
};

// Verify that online reauth dialog is shown automatically after user locks the
// screen if online reauth is required.
IN_PROC_BROWSER_TEST_F(AutoStartTest, DialogShownOnLock) {
  Login();
  ForceOnlineReauthOnLockScreen();
  ScreenLockerTester().Lock();
  ExpectSuccessfulAutoStart();
}

// Verify that online reauth dialog is shown automatically when online reauth
// becomes required while the screen is locked.
IN_PROC_BROWSER_TEST_F(AutoStartTest, DialogShownOnReauthEnforcement) {
  Login();
  ScreenLockerTester().Lock();

  // No dialog is shown since online reauth is not required yet.
  EXPECT_FALSE(LockScreenStartReauthDialog::GetInstance());

  ForceOnlineReauthOnLockScreen();
  ExpectSuccessfulAutoStart();
}

// Verify that the "Enter Google Account Info" is shown during
// AutoStart flow and pressing it initiates the standard reauth flow.
IN_PROC_BROWSER_TEST_F(AutoStartTest, ChangeIdPButtonPresence) {
  Login();
  ForceOnlineReauthOnLockScreen();
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::InitForShownDialog();
  // `reauth_dialog_helper` not being empty confirms that online reauth
  // dialog is shown.
  EXPECT_TRUE(reauth_dialog_helper);

  // Wait for the webview and SAML IdP page to load.
  reauth_dialog_helper->WaitForSigninWebview();
  reauth_dialog_helper->WaitForSamlIdpPageLoad();

  // EGAI button should be visible during the AutoStart flow,
  // but not during normal reauth.
  reauth_dialog_helper->ExpectChangeIdPButtonVisible();
  reauth_dialog_helper->ClickChangeIdPButtonOnSamlScreen();

  // With reauth endpoint we start on a Gaia page where user needs to click
  // "Next" before being redirected to SAML IdP page.
  reauth_dialog_helper->WaitForPrimaryGaiaButtonToBeEnabled();
  reauth_dialog_helper->ClickPrimaryGaiaButton();

  reauth_dialog_helper->WaitForSamlIdpPageLoad();
  reauth_dialog_helper->ExpectChangeIdPButtonHidden();
}

class SamlUnlockTest : public LockscreenWebUiTest {
 public:
  SamlUnlockTest() = default;

  SamlUnlockTest(const SamlUnlockTest&) = delete;
  SamlUnlockTest& operator=(const SamlUnlockTest&) = delete;
  ~SamlUnlockTest() override = default;

  // Go through online authentication (with saml) flow on the lock screen.
  void UnlockWithSAML() {
    std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
        LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

    // Fill-in the SAML IdP form and submit.
    test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
    signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
    signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
    signin_frame_js.TypeIntoPath("actual_password", {"Password"});
    signin_frame_js.TapOn("Submit");

    // Ensures that the re-auth dialog is closed.
    reauth_dialog_helper->WaitForReauthDialogToClose();
    ScreenLockerTester().WaitForUnlock();
  }
};

// Test Lockscreen reauth main flow.
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, Login) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  UnlockWithSAML();
}

// Test that SAML notice message mentions user's idp host.
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, SamlNoticeMessage) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  reauth_dialog_helper->ExpectSamlNoticeMessageVisible();
  // Check that SAML notice message contains idp host .
  std::string js = "$SamlNoticeMessagePath.textContent.indexOf('$Host') > -1";
  base::ReplaceSubstringsAfterOffset(
      &js, 0, "$SamlNoticeMessagePath",
      test::GetOobeElementPath(reauth_dialog_helper->SamlNoticeMessage()));
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Host",
                                     fake_saml_idp()->GetIdpHost());
  reauth_dialog_helper->DialogJS().ExpectTrue(js);
}

// Tests that "Enter Google Account Info" button is hidden when reauth endpoint
// is initiated on the lock screen.
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, SamlEgaiButtonHidden) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // With Gaia reauth endpoint we are guaranteed to land on the correct IdP
  // page so by design we don't display a button to switch to the Gaia page.
  reauth_dialog_helper->ExpectChangeIdPButtonHidden();
}

// Tests the close button in SAML Screen.
// TODO(crbug.com/1401612): re-enable this test. Flakily times out on
// linux-chromeos-rel.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SamlScreenCancel DISABLED_SamlScreenCancel
#else
#define MAYBE_SamlScreenCancel SamlScreenCancel
#endif
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, MAYBE_SamlScreenCancel) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  reauth_dialog_helper->ClickCancelButtonOnSamlScreen();

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ASSERT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());

  // Verify that the dialog can be opened again.
  LockScreenReauthDialogTestHelper::ShowDialogAndWait();
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, ScrapedSingle) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  content::DOMMessageQueue message_queue(
      reauth_dialog_helper->DialogWebContents());

  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecJs(
      reauth_dialog_helper->DialogWebContents(),
      "$('main-element').authenticator.addEventListener('authCompleted',"
      "    function(e) {"
      "      var password = e.detail.password;"
      "      window.domAutomationController.send(password);"
      "    });"));

  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();

  // Fill-in the SAML IdP form and submit.
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login and start the session.
  signin_frame_js.TapOn("Submit");

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"fake_password\"");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests password scraping from a dynamically created password field.
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, ScrapedDynamic) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.Evaluate(
      "(function() {"
      "  var newPassInput = document.createElement('input');"
      "  newPassInput.id = 'DynamicallyCreatedPassword';"
      "  newPassInput.type = 'password';"
      "  newPassInput.name = 'Password';"
      "  document.forms[0].appendChild(newPassInput);"
      "})();");

  // Fill-in the SAML IdP form and submit.
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"DynamicallyCreatedPassword"});

  // Scraping a single password should finish the login and start the session.
  signin_frame_js.TapOn("Submit");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests the multiple password scraped flow.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ScrapedMultiple DISABLED_ScrapedMultiple
#else
#define MAYBE_ScrapedMultiple ScrapedMultiple
#endif
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, MAYBE_ScrapedMultiple) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"Password"});
  signin_frame_js.TypeIntoPath("password1", {"Password1"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectSigninWebviewHidden();
  reauth_dialog_helper->ExpectPasswordConfirmInputHidden();

  // Entering an unknown password should go back to the confirm password screen.
  reauth_dialog_helper->SendConfirmPassword("wrong_password");
  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectPasswordConfirmInputHidden();

  // Either scraped password should be able to sign-in.
  reauth_dialog_helper->SendConfirmPassword("password1");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Test when no password is scraped.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ScrapedNone DISABLED_ScrapedNone
#else
#define MAYBE_ScrapedNone ScrapedNone
#endif
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, MAYBE_ScrapedNone) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectSigninWebviewHidden();
  reauth_dialog_helper->ExpectPasswordConfirmInputVisible();

  // Entering passwords that don't match will make us land again in the same
  // page.
  reauth_dialog_helper->SetManualPasswords("Test1", "Test2");
  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectPasswordConfirmInputVisible();

  // Two matching passwords should let the user to authenticate.
  reauth_dialog_helper->SetManualPasswords("Test1", "Test1");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests another account is authenticated other than the one used in sign
// in.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_VerifyAgainFlow DISABLED_VerifyAgainFlow
#else
#define MAYBE_VerifyAgainFlow VerifyAgainFlow
#endif
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, MAYBE_VerifyAgainFlow) {
  fake_gaia_mixin()->fake_gaia()->SetConfigurationHelper(
      FakeGaiaMixin::kEnterpriseUser2, kTestAuthSIDCookie1,
      kTestAuthLSIDCookie1);

  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Authenticate in the IdP with another account other than the one used in
  // sign in.
  fake_gaia_mixin()->fake_gaia()->SetConfigurationHelper(
      FakeGaiaMixin::kEnterpriseUser2, kTestAuthSIDCookie2,
      kTestAuthLSIDCookie2);
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser2, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectErrorScreenVisible();
  reauth_dialog_helper->ClickCancelButtonOnErrorScreen();

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();

  ASSERT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_LoadAbort DISABLED_LoadAbort
#else
#define MAYBE_LoadAbort LoadAbort
#endif
IN_PROC_BROWSER_TEST_F(SamlUnlockTest, MAYBE_LoadAbort) {
  Login();

  // Make gaia landing page unreachable
  const GaiaUrls& gaia_urls = *GaiaUrls::GetInstance();
  fake_gaia_mixin()->fake_gaia()->SetFixedResponse(
      gaia_urls.embedded_reauth_chromeos_url(), net::HTTP_NOT_FOUND);

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Unreachable gaia page should have resulted in load abort error which
  // should trigger the network dialog
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Close dialog at the end of the test - otherwise test will crash on exit
  reauth_dialog_helper->ClickCloseNetworkButton();
}

IN_PROC_BROWSER_TEST_F(SamlUnlockTest, SAMLBlocklistNavigationDisallowed) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_link.html");

  Login();
  ScreenLockerTester().Lock();

  std::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  ASSERT_TRUE(reauth_dialog_helper);

  // TODO(https://issuetracker.google.com/290830337): Make this test class
  // support propagating device policies to prefs with the logic in
  // `LoginProfilePolicyProvider`, and instead of setting prefs here directly,
  // just set the right device policies using
  // `DeviceStateMixin::RequestDevicePolicyUpdate`.
  // TODO(https://issuetracker.google.com/290821299): Add browser tests for
  // allowlisting.
  Profile::FromBrowserContext(
      BrowserContextHelper::Get()->GetLockScreenBrowserContext())
      ->GetPrefs()
      ->SetList(policy::policy_prefs::kUrlBlocklist,
                base::Value::List().Append(kSAMLLinkedPageURLPattern));

  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, kSAMLLink)->Wait();
  signin_frame_js.TapOn(kSAMLLink);
  WaitForLoadStop(signin_frame_js.web_contents());

  signin_frame_js
      .CreateElementTextContentWaiter(
          l10n_util::GetStringUTF8(
              IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR),
          {"main-frame-error"})
      ->Wait();
}

// Fixture which allows to test transfer of SAML cookies during online
// reauthentication on the lock screen.
class SAMLCookieTransferTest : public SamlUnlockTest {
 public:
  SAMLCookieTransferTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }

  SAMLCookieTransferTest(const SAMLCookieTransferTest&) = delete;
  SAMLCookieTransferTest& operator=(const SAMLCookieTransferTest&) = delete;

  ~SAMLCookieTransferTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();
    LockscreenWebUiTest::SetUpInProcessBrowserTestFixture();

    policy::DevicePolicyCrosTestHelper device_policy_test_helper;
    // Enable DeviceTransferSAMLCookies policy.
    device_policy_test_helper.device_policy()
        ->payload()
        .mutable_saml_settings()
        ->set_transfer_saml_cookies(true);
    // Make user affiliated - this is another condition required to transfer
    // saml cookies.
    auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
        FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper, std::array{kAffiliationID}));
    policy::UserPolicyBuilder user_policy_builder;
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy_builder, GetAccountId(), std::array{kAffiliationID}));
  }

  // Add some random cookie to user partition. This is needed because during
  // online reauthentication on the lock screen we transfer cookies only if it
  // is a subsequent login. To detect "subsequent login", we check if user's
  // cookie jar is not empty. These tests do not simulate online authentication
  // on sign-in screen which is why we need to add a cookie manually with this
  // method.
  void AddCookieToUserPartition() {
    constexpr char kRandomCookieName[] = "random_cookie";
    constexpr char kRandomCookieValue[] = "random_cookie_value";

    Profile* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    ASSERT_TRUE(profile);
    net::CookieOptions options;
    options.set_include_httponly();
    profile->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(
            *net::CanonicalCookie::CreateSanitizedCookie(
                fake_saml_idp()->GetSamlPageUrl(), kRandomCookieName,
                kRandomCookieValue, ".example.com", /*path=*/std::string(),
                /*creation_time=*/base::Time(),
                /*expiration_time=*/base::Time(),
                /*last_access_time=*/base::Time(), /*secure=*/true,
                /*http_only=*/false, net::CookieSameSite::NO_RESTRICTION,
                net::COOKIE_PRIORITY_DEFAULT,
                /*partition_key=*/std::nullopt, /*status=*/nullptr),
            fake_saml_idp()->GetSamlPageUrl(), options, base::DoNothing());
    ExpectCookieInUserProfile(kRandomCookieName, kRandomCookieValue);
  }

  void ExpectCookieInUserProfile(const std::string& cookie_name,
                                 const std::string& cookie_value) {
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    net::CookieList cookie_list_;
    base::RunLoop run_loop;
    profile->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindLambdaForTesting(
            [&](const std::vector<net::CanonicalCookie>& cookies) {
              cookie_list_ = cookies;
              run_loop.Quit();
            }));
    run_loop.Run();
    EXPECT_GT(cookie_list_.size(), 0u);

    const auto saml_cookie_iterator = base::ranges::find(
        cookie_list_, cookie_name,
        [](const net::CanonicalCookie& cookie) { return cookie.Name(); });
    EXPECT_NE(saml_cookie_iterator, cookie_list_.end());
    EXPECT_EQ(cookie_value, saml_cookie_iterator->Value());
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Test transfer of saml cookies during online reauth on the lock screen
IN_PROC_BROWSER_TEST_F(SAMLCookieTransferTest, CookieTransfer) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  LoginWithoutUpdatingPolicies();
  AddCookieToUserPartition();

  ScreenLockerTester().Lock();

  UnlockWithSAML();

  ExpectCookieInUserProfile(kSAMLIdPCookieName, kSAMLIdPCookieValue);
}

// Fixture which sets SAML SSO profile to device policy protobuff
class SamlSsoProfileTest : public SamlUnlockTest {
 public:
  SamlSsoProfileTest() { device_state_.set_skip_initial_policy_setup(true); }

  SamlSsoProfileTest(const SamlSsoProfileTest&) = delete;
  SamlSsoProfileTest& operator=(const SamlSsoProfileTest&) = delete;

  ~SamlSsoProfileTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();
    LockscreenWebUiTest::SetUpInProcessBrowserTestFixture();

    // Set sso profile to device policy protobuff. It will be fetched from there
    // during online reauth.
    policy::DevicePolicyCrosTestHelper device_policy_test_helper;
    device_policy_test_helper.device_policy()->policy_data().set_sso_profile(
        fake_saml_idp()->GetIdpSsoProfile());

    // Set affiliation and user policies - this is needed for login in tests to
    // work correctly
    auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
        FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper, std::array{kAffiliationID}));
    policy::UserPolicyBuilder user_policy_builder;
    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy_builder, GetAccountId(), std::array{kAffiliationID}));
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Test that during online reauth on the lock screen we can perform SAML
// redirection without relying on domain-based redirection. Depending on
// Gaia endpoint, we will rely either on an email, or on an SSO profile.
IN_PROC_BROWSER_TEST_F(SamlSsoProfileTest, ReauthIndependentOfDomain) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  // Set wrong redirect url for domain-based saml redirection.
  const GURL wrong_redirect_url("https://wrong.com");
  fake_gaia_mixin()->fake_gaia()->RegisterSamlDomainRedirectUrl(
      fake_saml_idp()->GetIdpDomain(), wrong_redirect_url);

  LoginWithoutUpdatingPolicies();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  UnlockWithSAML();
}

}  // namespace ash
