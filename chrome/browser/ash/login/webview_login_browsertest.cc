// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <iterator>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/signin/token_handle_util.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/fake_recovery_service_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/tpm/tpm_token_loader.h"
#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/onc/onc_constants.h"
#include "components/onc/onc_pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/trusted_vault_client.h"
#include "components/sync/trusted_vault/securebox.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "google_apis/gaia/gaia_urls.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

namespace em = ::enterprise_management;

constexpr char kTestGuid[] = "cccccccc-cccc-4ccc-0ccc-ccccccccccc1";
constexpr char kTestCookieName[] = "TestCookie";
constexpr char kTestCookieValue[] = "present";
constexpr char kTestCookieHost[] = "host1.com";
constexpr char kClientCert1Name[] = "client_1";
constexpr char kClientCert2Name[] = "client_2";
constexpr char kTestTokenHandle[] = "test_token_handle";

constexpr test::UIPath kPrimaryButton = {"gaia-signin", "signin-frame-dialog",
                                         "primary-action-button"};
constexpr test::UIPath kSecondaryButton = {"gaia-signin", "signin-frame-dialog",
                                           "secondary-action-button"};
constexpr test::UIPath kBackButton = {"gaia-signin", "signin-frame-dialog",
                                      "signin-back-button"};
constexpr char kSigninWebview[] = "$('gaia-signin').getSigninFrame_()";
constexpr char kSigninWebviewOnLockScreen[] =
    "$('main-element').getSigninFrame_()";

// UMA names for better test reading.
const char kLoginRequests[] = "OOBE.GaiaScreen.LoginRequests";
const char kSuccessLoginRequests[] = "OOBE.GaiaScreen.SuccessLoginRequests";
const char kPasswordIgnoredChars[] = "OOBE.GaiaScreen.PasswordIgnoredChars";

void InjectCookieDoneCallback(base::OnceClosure done_closure,
                              net::CookieAccessResult result) {
  ASSERT_TRUE(result.status.IsInclude());
  std::move(done_closure).Run();
}

// Injects a cookie into `storage_partition`, so we can test for cookie presence
// later to infer if the StoragePartition has been cleared.
void InjectCookie(content::StoragePartition* storage_partition) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          kTestCookieName, kTestCookieValue, kTestCookieHost, "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM,
          /*same_party=*/false);
  base::RunLoop run_loop;
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      net::CookieOptions(),
      base::BindOnce(&InjectCookieDoneCallback, run_loop.QuitClosure()));
  run_loop.Run();
}

void GetAllCookiesCallback(std::string* cookies_out,
                           base::OnceClosure done_closure,
                           const std::vector<net::CanonicalCookie>& cookies) {
  *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
  std::move(done_closure).Run();
}

// Returns all cookies present in `storage_partition` as a HTTP header cookie
// line. Will be an empty string if there are no cookies.
std::string GetAllCookies(content::StoragePartition* storage_partition) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  storage_partition->GetNetworkContext()->GetCookieManager(
      cookie_manager.BindNewPipeAndPassReceiver());

  std::string cookies;
  base::RunLoop run_loop;
  cookie_manager->GetAllCookies(
      base::BindOnce(&GetAllCookiesCallback, &cookies, run_loop.QuitClosure()));
  run_loop.Run();
  return cookies;
}

void PolicyChangedCallback(base::RepeatingClosure callback,
                           const base::Value* old_value,
                           const base::Value* new_value) {
  callback.Run();
}

// Spins the loop until a notification is received from `prefs` that the value
// of `pref_name` has changed. If the notification is received before Wait()
// has been called, Wait() returns immediately and no loop is spun.
class PrefChangeWatcher {
 public:
  PrefChangeWatcher(const std::string& pref_name, PrefService* prefs);

  PrefChangeWatcher(const PrefChangeWatcher&) = delete;
  PrefChangeWatcher& operator=(const PrefChangeWatcher&) = delete;

  void Wait();

 private:
  void OnPrefChange();

  bool pref_changed_ = false;

  base::RunLoop run_loop_;
  PrefChangeRegistrar registrar_;
};

PrefChangeWatcher::PrefChangeWatcher(const std::string& pref_name,
                                     PrefService* prefs) {
  registrar_.Init(prefs);
  registrar_.Add(pref_name,
                 base::BindRepeating(&PrefChangeWatcher::OnPrefChange,
                                     base::Unretained(this)));
}

void PrefChangeWatcher::Wait() {
  if (!pref_changed_)
    run_loop_.Run();
}

void PrefChangeWatcher::OnPrefChange() {
  pref_changed_ = true;
  run_loop_.Quit();
}

// Observes OOBE screens and can be queried to see if the error screen has been
// displayed since ErrorScreenWatcher has been constructed.
class ErrorScreenWatcher : public OobeUI::Observer {
 public:
  ErrorScreenWatcher() {
    OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
    oobe_ui_observation_.Observe(oobe_ui);

    if (oobe_ui->current_screen() == ErrorScreenView::kScreenId)
      has_error_screen_been_shown_ = true;
  }

  ErrorScreenWatcher(const ErrorScreenWatcher& other) = delete;
  ErrorScreenWatcher& operator=(const ErrorScreenWatcher& other) = delete;

  ~ErrorScreenWatcher() override = default;

  bool has_error_screen_been_shown() const {
    return has_error_screen_been_shown_;
  }

  // OobeUI::Observer:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override {
    if (new_screen == ErrorScreenView::kScreenId)
      has_error_screen_been_shown_ = true;
  }

  // OobeUI::Observer:
  void OnDestroyingOobeUI() override {}

 private:
  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observation_{this};

  bool has_error_screen_been_shown_ = false;
};

bool EqualsTestCert(const net::X509Certificate& cert,
                    const std::string& expected_test_cert_name) {
  const base::FilePath cert_file_name =
      base::FilePath::FromASCII(expected_test_cert_name)
          .AddExtensionASCII("pem");
  scoped_refptr<net::X509Certificate> expected = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), cert_file_name.MaybeAsASCII());
  if (!expected) {
    ADD_FAILURE() << "Failed to read test certificate "
                  << expected_test_cert_name;
    return false;
  }
  return expected->EqualsExcludingChain(&cert);
}

MATCHER_P(EqualsCert,
          cert_name,
          base::StringPrintf("Is test certificate %s", cert_name.c_str())) {
  return EqualsTestCert(arg, cert_name);
}

}  // namespace

class WebviewLoginTest : public OobeBaseTest {
 public:
  WebviewLoginTest() = default;

  WebviewLoginTest(const WebviewLoginTest&) = delete;
  WebviewLoginTest& operator=(const WebviewLoginTest&) = delete;

  ~WebviewLoginTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    OobeBaseTest::SetUpCommandLine(command_line);
  }
  base::HistogramTester histogram_tester_;

 protected:
  void ExpectIdentifierPage() {
    // First page: back button, #identifier input field.
    test::OobeJS().ExpectVisiblePath(kBackButton);
    test::OobeJS().ExpectTrue(
        base::StrCat({kSigninWebview, ".src.indexOf('#identifier') != -1"}));
  }

  void ExpectPasswordPage() {
    // Second page: back button, #challengepassword input field.
    test::OobeJS().ExpectVisiblePath(kBackButton);
    test::OobeJS().ExpectTrue(base::StrCat(
        {kSigninWebview, ".src.indexOf('#challengepassword') != -1"}));
  }

  // Returns true if a webview which has a WebContents associated with
  // `storage_partition` currently exists in the login UI's main WebContents.
  bool IsLoginScreenHasWebviewWithStoragePartition(
      const content::StoragePartition* storage_partition) {
    bool web_view_found = false;

    auto* login_main_frame =
        GetLoginUI()->GetWebContents()->GetPrimaryMainFrame();
    login_main_frame->ForEachRenderFrameHostWithAction(
        [&](content::RenderFrameHost* rfh) {
          if (rfh->GetStoragePartition() == storage_partition) {
            web_view_found = true;
            return content::RenderFrameHost::FrameIterationAction::kStop;
          }
          return content::RenderFrameHost::FrameIterationAction::kContinue;
        });

    return web_view_found;
  }

  void DisableImplicitServices() {
    SigninFrameJS().ExecuteAsync(
        "gaia.chromeOSLogin.shouldSendImplicitServices = false");
  }

  void DisableCloseViewMessage() {
    SigninFrameJS().ExecuteAsync(
        "gaia.chromeOSLogin.shouldSendCloseView = false");
  }

  void WaitForServicesSet() {
    test::OobeJS()
        .CreateWaiter("$('gaia-signin').authenticator_.services_")
        ->Wait();
  }

 protected:
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

class WebviewCloseViewLoginTest : public WebviewLoginTest,
                                  /* Does Gaia send the 'closeView' message */
                                  public ::testing::WithParamInterface<bool> {
 public:
  static std::string GetName(const testing::TestParamInfo<bool>& param) {
    return param.param ? "ServerEnabled" : "ServerDisabled";
  }

 protected:
  void SendCloseViewOrEmulateTimeout() {
    if (GetParam()) {
      SigninFrameJS().ExecuteAsync("gaia.chromeOSLogin.sendCloseView()");
      return;
    }

    EmulateGaiaDoneTimeout();
  }

  void EmulateGaiaDoneTimeout() {
    // Wait for user info timer to be set.
    test::OobeJS()
        .CreateWaiter("$('gaia-signin').authenticator_.gaiaDoneTimer_")
        ->Wait();

    // Emulate timeout fire.
    test::OobeJS().ExecuteAsync(
        "$('gaia-signin').authenticator_.onGaiaDoneTimeout_()");
  }
};

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest, NativeTest) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  test::OobeJS().ExpectVisiblePath(kSecondaryButton);
  test::OobeJS().ExpectEnabledPath(kSecondaryButton);

  // Check events propagation.
  SigninFrameJS().ExecuteAsync("sendSetAllActionsEnabled(false)");
  test::OobeJS().CreateEnabledWaiter(false, kPrimaryButton)->Wait();
  test::OobeJS().CreateEnabledWaiter(false, kSecondaryButton)->Wait();
  test::OobeJS().ExpectVisiblePath(kPrimaryButton);
  test::OobeJS().ExpectVisiblePath(kSecondaryButton);

  SigninFrameJS().ExecuteAsync("sendSetSecondaryActionEnabled(true)");
  test::OobeJS().CreateEnabledWaiter(true, kSecondaryButton)->Wait();
  test::OobeJS().ExpectVisiblePath(kSecondaryButton);

  // Click on the secondary button disables it.
  test::OobeJS().ClickOnPath(kSecondaryButton);
  test::OobeJS().CreateEnabledWaiter(false, kSecondaryButton)->Wait();

  SigninFrameJS().ExecuteAsync("sendSetPrimaryActionEnabled(true)");
  test::OobeJS().CreateEnabledWaiter(true, kPrimaryButton)->Wait();
  test::OobeJS().ExpectVisiblePath(kPrimaryButton);

  SigninFrameJS().ExecuteAsync("sendSetPrimaryActionLabel(null)");
  test::OobeJS().CreateVisibilityWaiter(false, kPrimaryButton)->Wait();

  SigninFrameJS().ExecuteAsync("sendSetSecondaryActionLabel(null)");
  test::OobeJS().CreateVisibilityWaiter(false, kSecondaryButton)->Wait();

  SigninFrameJS().ExecuteAsync("sendSetPrimaryActionLabel('Submit')");
  test::OobeJS().CreateVisibilityWaiter(true, kPrimaryButton)->Wait();
  test::OobeJS().ExpectElementText("Submit", kPrimaryButton);

  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  SendCloseViewOrEmulateTimeout();

  test::WaitForPrimaryUserSessionStart();

  histogram_tester_.ExpectUniqueSample("ChromeOS.Gaia.Message.Gaia.UserInfo",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample("ChromeOS.Gaia.Message.Gaia.CloseView",
                                       GetParam(), 1);
}

// Basic signin with username and password.
IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest, Basic) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  ExpectIdentifierPage();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  ASSERT_TRUE(LoginDisplayHost::default_host());
  EXPECT_TRUE(LoginDisplayHost::default_host()->GetWebUILoginView());

  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  SendCloseViewOrEmulateTimeout();

  // The login view should be destroyed after the browser window opens.
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_FALSE(LoginDisplayHost::default_host()->GetWebUILoginView());

  test::WaitForPrimaryUserSessionStart();

  // Wait for the LoginDisplayHost to delete itself, which is a posted task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LoginDisplayHost::default_host());

  histogram_tester_.ExpectUniqueSample("ChromeOS.SAML.APILogin", 0, 1);
  histogram_tester_.ExpectTotalCount("OOBE.GaiaLoginTime", 1);
  histogram_tester_.ExpectUniqueSample(kLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kSuccessLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kPasswordIgnoredChars,
                                       0 /* no ignored chars */, 1);
}

IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest,
                       PasswordWithTrailingWhitespaces) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  ExpectIdentifierPage();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  ASSERT_TRUE(LoginDisplayHost::default_host());
  EXPECT_TRUE(LoginDisplayHost::default_host()->GetWebUILoginView());

  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath("password-with-whitespace ",
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  SendCloseViewOrEmulateTimeout();

  // The login view should be destroyed after the browser window opens.
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_FALSE(LoginDisplayHost::default_host()->GetWebUILoginView());

  test::WaitForPrimaryUserSessionStart();

  // Wait for the LoginDisplayHost to delete itself, which is a posted task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LoginDisplayHost::default_host());

  histogram_tester_.ExpectUniqueSample("ChromeOS.SAML.APILogin", 0, 1);
  histogram_tester_.ExpectTotalCount("OOBE.GaiaLoginTime", 1);
  histogram_tester_.ExpectUniqueSample(kLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kSuccessLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kPasswordIgnoredChars,
                                       1 /* has ignored chars */, 1);
}

IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest,
                       PasswordWithLeadingWhitespaces) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  ExpectIdentifierPage();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  ASSERT_TRUE(LoginDisplayHost::default_host());
  EXPECT_TRUE(LoginDisplayHost::default_host()->GetWebUILoginView());

  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath(" password-with-whitespace",
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  SendCloseViewOrEmulateTimeout();

  // The login view should be destroyed after the browser window opens.
  ui_test_utils::WaitForBrowserToOpen();
  EXPECT_FALSE(LoginDisplayHost::default_host()->GetWebUILoginView());

  test::WaitForPrimaryUserSessionStart();

  // Wait for the LoginDisplayHost to delete itself, which is a posted task.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(LoginDisplayHost::default_host());

  histogram_tester_.ExpectUniqueSample("ChromeOS.SAML.APILogin", 0, 1);
  histogram_tester_.ExpectTotalCount("OOBE.GaiaLoginTime", 1);
  histogram_tester_.ExpectUniqueSample(kLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kSuccessLoginRequests,
                                       GaiaView::GaiaLoginVariant::kOobe, 1);
  histogram_tester_.ExpectUniqueSample(kPasswordIgnoredChars,
                                       1 /* has ignored chars */, 1);
}

IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest, BackButton) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Start with identifer page.
  ExpectIdentifierPage();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();

  // Move to password page.
  auto back_button_waiter = CreateGaiaPageEventWaiter("backButton");
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  back_button_waiter->Wait();
  ExpectPasswordPage();

  // Click back to identifier page.
  back_button_waiter = CreateGaiaPageEventWaiter("backButton");
  test::OobeJS().ClickOnPath(kBackButton);
  back_button_waiter->Wait();
  ExpectIdentifierPage();

  back_button_waiter = CreateGaiaPageEventWaiter("backButton");
  // Click next to password page, user id is remembered.
  test::OobeJS().ClickOnPath(kPrimaryButton);
  back_button_waiter->Wait();
  ExpectPasswordPage();

  // Finish sign-up.
  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  SendCloseViewOrEmulateTimeout();

  test::WaitForPrimaryUserSessionStart();
}

class WebviewLoginTestWithSyncTrustedVaultEnabled : public WebviewLoginTest {
 public:
  WebviewLoginTestWithSyncTrustedVaultEnabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        ::syncer::kSyncTrustedVaultPassphraseRecovery);
  }
};

IN_PROC_BROWSER_TEST_F(WebviewLoginTestWithSyncTrustedVaultEnabled,
                       BasicWithKeys) {
  // Set up some fake keys in the server.
  FakeGaia::SyncTrustedVaultKeys fake_gaia_keys;
  // Create an arbitrary encryption key, the precisely value is not relevant,
  // but used as test expectation later down.
  fake_gaia_keys.encryption_key.resize(16, 123);
  fake_gaia_keys.encryption_key_version = 91;
  // Create a random-but-valid public key, the precisely value is not relevant.
  fake_gaia_keys.trusted_public_keys.push_back(
      syncer::SecureBoxKeyPair::GenerateRandom()->public_key().ExportToBytes());
  fake_gaia_.fake_gaia()->SetSyncTrustedVaultKeys(FakeGaiaMixin::kFakeUserEmail,
                                                  fake_gaia_keys);

  WaitForGaiaPageLoadAndPropertyUpdate();

  ExpectIdentifierPage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();

  ASSERT_TRUE(LoginDisplayHost::default_host());

  SigninFrameJS().TypeIntoPath("[]", {"services"});
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  Browser* browser = ui_test_utils::WaitForBrowserToOpen();
  test::WaitForPrimaryUserSessionStart();

  // AddRecoveryMethod() logic is deferred until refresh tokens are loaded.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser->profile());
  signin::WaitForRefreshTokensLoaded(identity_manager);

  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser->profile());
  syncer::TrustedVaultClient* trusted_vault_client =
      sync_service->GetSyncClientForTest()->GetTrustedVaultClient();

  // Verify that the sync trusted vault keys have been received and stored.
  {
    base::RunLoop loop;
    std::vector<std::vector<uint8_t>> actual_keys;
    trusted_vault_client->FetchKeys(
        sync_service->GetAccountInfo(),
        base::BindLambdaForTesting(
            [&](const std::vector<std::vector<uint8_t>>& keys) {
              actual_keys = keys;
              loop.Quit();
            }));
    loop.Run();

    EXPECT_THAT(actual_keys,
                testing::ElementsAre(fake_gaia_keys.encryption_key));
  }

  // Verify that the recovery method was passed too.
  {
    base::RunLoop loop;
    std::vector<uint8_t> actual_public_key;
    static_cast<syncer::StandaloneTrustedVaultClient*>(
        sync_service->GetSyncClientForTest()->GetTrustedVaultClient())
        ->GetLastAddedRecoveryMethodPublicKeyForTesting(
            base::BindLambdaForTesting([&](const std::vector<uint8_t>& key) {
              actual_public_key = key;
              loop.Quit();
            }));
    loop.Run();

    EXPECT_EQ(actual_public_key, fake_gaia_keys.trusted_public_keys.back());
  }
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, ErrorScreenOnGaiaError) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();

  // Make gaia landing page unreachable
  fake_gaia_.fake_gaia()->SetFixedResponse(
      GaiaUrls::GetInstance()->embedded_setup_chromeos_url(2),
      net::HTTP_NOT_FOUND);

  // Click back to reload (unreachable) identifier page.
  test::OobeJS().ClickOnPath(kBackButton);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest,
                       NavigationButtonsDisabledBeforeGaiaLoaded) {
  WaitForSigninScreen();
  test::WaitForOobeJSReady();

  test::OobeJS().ExpectHiddenPath(kPrimaryButton);
  test::OobeJS().ExpectDisabledPath(kPrimaryButton);
  test::OobeJS().ExpectHiddenPath(kSecondaryButton);
  test::OobeJS().ExpectDisabledPath(kSecondaryButton);
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest,
                       NavigationButtonsDisabledOnGaiaReload) {
  // Progress to password page, so that both buttons are enabled.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectPasswordPage();
  test::OobeJS().ExpectEnabledPath(kPrimaryButton);
  test::OobeJS().ExpectEnabledPath(kSecondaryButton);

  // Return empty gaia page so that we do not re-enable buttons again.
  fake_gaia_.fake_gaia()->SetFixedResponse(
      GaiaUrls::GetInstance()->embedded_setup_chromeos_url(2), net::HTTP_OK,
      "<body>no-op gaia</body>");
  test::OobeJS().ExecuteAsync("$('gaia-signin').authenticator_.reload()");

  // Wait for both buttons to become disabled due to reload.
  test::OobeJS().CreateEnabledWaiter(false, kPrimaryButton)->Wait();
  test::OobeJS().CreateEnabledWaiter(false, kSecondaryButton)->Wait();
}

// Device settings could only change on the owned device.
class WebviewDeviceOwnedLoginTest : public WebviewLoginTest {
 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Create new account option should be available only if the settings allow it.
IN_PROC_BROWSER_TEST_F(WebviewDeviceOwnedLoginTest, AllowNewUser) {
  WaitForGaiaPageLoad();

  std::string frame_url = "$('gaia-signin').authenticator_.reloadUrl_";
  // New users are allowed.
  test::OobeJS().ExpectTrue(frame_url + ".search('flow=nosignup') == -1");

  // Disallow new users - we also need to set an allowlist due to weird logic.
  scoped_testing_cros_settings_.device_settings()->Set(kAccountsPrefUsers,
                                                       base::ListValue());
  scoped_testing_cros_settings_.device_settings()->Set(
      kAccountsPrefAllowNewUser, base::Value(false));
  WaitForGaiaPageReload();

  // flow=nosignup indicates that user creation is not allowed.
  test::OobeJS().ExpectTrue(frame_url + ".search('flow=nosignup') != -1");
}

class ReauthWebviewLoginTest : public WebviewLoginTest {
 protected:
  LoginManagerMixin::TestUserInfo reauth_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId),
      user_manager::USER_TYPE_REGULAR,
      /* invalid token status to force online signin */
      user_manager::User::OAUTH2_TOKEN_STATUS_INVALID};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {reauth_user_}};
};

IN_PROC_BROWSER_TEST_F(ReauthWebviewLoginTest, EmailPrefill) {
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(reauth_user_.account_id));
  // Focus triggers online signin.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(reauth_user_.account_id));
  WaitForGaiaPageLoad();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            reauth_user_.account_id.GetUserEmail());
}

class ReauthTokenWebviewLoginTest : public ReauthWebviewLoginTest {
 public:
  ReauthTokenWebviewLoginTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(
        features::kCryptohomeRecoveryFlow);
    login_manager_mixin_.AppendRegularUsers(1);
    user_with_invalid_token_ = login_manager_mixin_.users().back().account_id;
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    ReauthWebviewLoginTest::SetUpInProcessBrowserTestFixture();
    TokenHandleUtil::SetInvalidTokenForTesting(kTestTokenHandle);
  }

  void TearDownInProcessBrowserTestFixture() override {
    TokenHandleUtil::SetInvalidTokenForTesting(nullptr);
    ReauthWebviewLoginTest::TearDownInProcessBrowserTestFixture();
  }

  AccountId user_with_invalid_token_;
  FakeRecoveryServiceMixin fake_recovery_service_{&mixin_host_,
                                                  embedded_test_server()};
};

IN_PROC_BROWSER_TEST_F(ReauthTokenWebviewLoginTest, FetchSuccess) {
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTestTokenHandle);
  // Force to remain in OOBE after login instead of start session, so we could
  // verify the value in UserContext.
  user_manager::KnownUser(g_browser_process->local_state())
      .SetPendingOnboardingScreen(user_with_invalid_token_,
                                  MarketingOptInScreenView::kScreenId.name);
  // Focus triggers token check and updates the user pod to online sign-in
  // state.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(user_with_invalid_token_));

  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  // Focus triggers online signin.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(user_with_invalid_token_));
  WaitForGaiaPageLoadAndPropertyUpdate();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            user_with_invalid_token_.GetUserEmail());
  EXPECT_EQ(fake_gaia_.fake_gaia()->reauth_request_token(),
            "fake-reauth-request-token");
  test::OobeJS().ClickOnPath(kPrimaryButton);

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  OobeScreenExitWaiter(GaiaView::kScreenId).Wait();

  UserContext* user_context = LoginDisplayHost::default_host()
                                  ->GetWizardContext()
                                  ->extra_factors_auth_session.get();
  EXPECT_EQ(user_context->GetReauthProofToken(), "fake-reauth-proof-token");
}

IN_PROC_BROWSER_TEST_F(ReauthTokenWebviewLoginTest, FetchFailure) {
  fake_recovery_service_.SetErrorResponse("/v1/rart",
                                          net::HTTP_SERVICE_UNAVAILABLE);
  TokenHandleUtil::StoreTokenHandle(user_with_invalid_token_, kTestTokenHandle);
  // Force to remain in OOBE after login instead of start session, so we could
  // verify the value in UserContext.
  user_manager::KnownUser(g_browser_process->local_state())
      .SetPendingOnboardingScreen(user_with_invalid_token_,
                                  MarketingOptInScreenView::kScreenId.name);
  // Focus triggers token check and updates the user pod to online sign-in
  // state.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(user_with_invalid_token_));

  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(user_with_invalid_token_));
  // Focus triggers online signin.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(user_with_invalid_token_));
  WaitForGaiaPageLoadAndPropertyUpdate();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            user_with_invalid_token_.GetUserEmail());
  EXPECT_TRUE(fake_gaia_.fake_gaia()->reauth_request_token().empty());
  test::OobeJS().ClickOnPath(kPrimaryButton);

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  OobeScreenExitWaiter(GaiaView::kScreenId).Wait();
  UserContext* user_context = LoginDisplayHost::default_host()
                                  ->GetWizardContext()
                                  ->extra_factors_auth_session.get();
  EXPECT_TRUE(user_context->GetReauthProofToken().empty());
}

class ReauthEndpointWebviewLoginTest : public WebviewLoginTest {
 protected:
  ReauthEndpointWebviewLoginTest() {
    // TODO(https://crbug.com/1153912) Makes tests work with
    // kParentAccessCodeForOnlineLogin enabled.
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        ::features::kParentAccessCodeForOnlineLogin);
  }
  ~ReauthEndpointWebviewLoginTest() override = default;

  LoginManagerMixin::TestUserInfo reauth_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId),
      user_manager::USER_TYPE_CHILD,
      /* invalid token status to force online signin */
      user_manager::User::OAUTH2_TOKEN_STATUS_INVALID};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {reauth_user_}};
};

IN_PROC_BROWSER_TEST_F(ReauthEndpointWebviewLoginTest, SupervisedUser) {
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(reauth_user_.account_id));
  // Focus triggers online signin.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(reauth_user_.account_id));
  WaitForGaiaPageLoad();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            reauth_user_.account_id.GetUserEmail());
  EXPECT_EQ(fake_gaia_.fake_gaia()->is_supervised(), "1");
  EXPECT_TRUE(fake_gaia_.fake_gaia()->is_device_owner().empty());
}

class ReauthEndpointWebviewLoginOwnerTest
    : public ReauthEndpointWebviewLoginTest {
 protected:
  ReauthEndpointWebviewLoginOwnerTest() {
    scoped_testing_cros_settings_.device_settings()->Set(
        kDeviceOwner, base::Value(FakeGaiaMixin::kFakeUserEmail));
  }
  ~ReauthEndpointWebviewLoginOwnerTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ReauthEndpointWebviewLoginOwnerTest, SupervisedUser) {
  EXPECT_TRUE(
      LoginScreenTestApi::IsForcedOnlineSignin(reauth_user_.account_id));
  // Focus triggers online signin.
  EXPECT_TRUE(LoginScreenTestApi::FocusUser(reauth_user_.account_id));
  WaitForGaiaPageLoad();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(fake_gaia_.fake_gaia()->prefilled_email(),
            reauth_user_.account_id.GetUserEmail());
  EXPECT_EQ(fake_gaia_.fake_gaia()->is_supervised(), "1");
  EXPECT_EQ(fake_gaia_.fake_gaia()->is_device_owner(), "1");
  histogram_tester_.ExpectTotalCount("OOBE.GaiaLoginTime", 0);
}

IN_PROC_BROWSER_TEST_F(WebviewLoginTest, StoragePartitionHandling) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Start with identifier page.
  ExpectIdentifierPage();

  // WebContents of the embedding frame
  content::WebContents* web_contents = GetLoginUI()->GetWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();

  std::string signin_frame_partition_name_1 =
      test::OobeJS().GetString(base::StrCat({kSigninWebview, ".partition"}));
  content::StoragePartition* signin_frame_partition_1 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_1.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_1);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));
  // Inject a cookie into the currently used StoragePartition, so we can test
  // later if it has been cleared.
  InjectCookie(signin_frame_partition_1);

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  test::OobeJS().ClickOnPath(kBackButton);
  WaitForGaiaPageBackButtonUpdate();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();

  std::string signin_frame_partition_name_2 =
      test::OobeJS().GetString(base::StrCat({kSigninWebview, ".partition"}));
  content::StoragePartition* signin_frame_partition_2 =
      login::GetSigninPartition();

  EXPECT_FALSE(signin_frame_partition_name_2.empty());
  EXPECT_EQ(login::SigninPartitionManager::Factory::GetForBrowserContext(
                browser_context)
                ->GetCurrentStoragePartitionName(),
            signin_frame_partition_name_2);
  EXPECT_TRUE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_2));
  InjectCookie(signin_frame_partition_2);

  // Make sure that the partitions differ and that the old one is not in use
  // anymore.
  EXPECT_NE(signin_frame_partition_name_1, signin_frame_partition_name_2);
  EXPECT_NE(signin_frame_partition_1, signin_frame_partition_2);
  EXPECT_FALSE(
      IsLoginScreenHasWebviewWithStoragePartition(signin_frame_partition_1));

  // The StoragePartition which is not in use is supposed to have been cleared.
  EXPECT_EQ("", GetAllCookies(signin_frame_partition_1));
  EXPECT_NE("", GetAllCookies(signin_frame_partition_2));
}

enum class FrameUrlOrigin { kSameOrigin, kDifferentOrigin };

// Parametrized test fixture that configures FakeGaia to server an iframe in the
// embedded ChromeOS setup response. If the parameter is
// FrameUrlOrigin::kSameOrigin, the frame URL will be on the same origin as fake
// gaia. If it's FrameUrlOrigin::kDifferentOrigin, it will be on a different
// origin.
// The frame URL serves an empty HTTP document with the X-Frame-Options header
// set to SAMEORIGIN, so the frame load will fail when
// FrameUrlOrigin::kDifferentOrigin is set as the parameter.
class WebviewLoginWithIframeTest
    : public WebviewLoginTest,
      public ::testing::WithParamInterface<FrameUrlOrigin> {
 public:
  WebviewLoginWithIframeTest() = default;
  ~WebviewLoginWithIframeTest() override = default;

  WebviewLoginWithIframeTest(const WebviewLoginWithIframeTest& other) = delete;
  WebviewLoginWithIframeTest& operator=(
      const WebviewLoginWithIframeTest& other) = delete;

  // WebviewLoginTest:
  void RegisterAdditionalRequestHandlers() override {
    WebviewLoginTest::RegisterAdditionalRequestHandlers();

    // For simplicity the request handler is registered on both servers. The
    // test will only request the path from one of them, depending on the
    // FrameUrlOrigin test parameter.
    fake_gaia_.gaia_server()->RegisterRequestHandler(base::BindRepeating(
        &WebviewLoginWithIframeTest::HandleFrameRelativePath));
    other_origin_server_.RegisterRequestHandler(base::BindRepeating(
        &WebviewLoginWithIframeTest::HandleFrameRelativePath));
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebviewLoginTest::SetUpInProcessBrowserTestFixture();

    net::EmbeddedTestServer::ServerCertificateConfig other_origin_cert_config;
    other_origin_cert_config.dns_names = {kOtherOriginHost};
    other_origin_server_.SetSSLConfig(other_origin_cert_config);
    // Initialize the server so the port is known, but don't start the IO thread
    // until SetupThreadMain().
    ASSERT_TRUE(other_origin_server_.InitializeAndListen());

    switch (GetParam()) {
      case FrameUrlOrigin::kSameOrigin:
        frame_url_ = fake_gaia_.GetFakeGaiaURL(kFrameRelativePath);
        break;
      case FrameUrlOrigin::kDifferentOrigin:
        frame_url_ =
            other_origin_server_.GetURL(kOtherOriginHost, kFrameRelativePath);
        break;
    }

    fake_gaia_.fake_gaia()->SetIframeOnEmbeddedSetupChromeosUrl(frame_url_);
  }

  void SetUpOnMainThread() override {
    other_origin_server_.StartAcceptingConnections();
    WebviewLoginTest::SetUpOnMainThread();
  }

 protected:
  static constexpr const char* kOtherOriginHost = "other.example.com";
  static constexpr const char* kFrameRelativePath =
      "/frame_with_same_origin_requirement";

  static std::unique_ptr<net::test_server::HttpResponse>
  HandleFrameRelativePath(const net::test_server::HttpRequest& request) {
    if (request.relative_url != kFrameRelativePath) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("<!DOCTYPE html>");
    response->AddCustomHeader("X-Frame-Options", "SAMEORIGIN");
    return response;
  }

  net::EmbeddedTestServer other_origin_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  GURL frame_url_;
};

IN_PROC_BROWSER_TEST_P(WebviewLoginWithIframeTest, GaiaWithIframe) {
  ErrorScreenWatcher error_screen_watcher;

  content::TestNavigationObserver navigation_observer(frame_url_);
  navigation_observer.StartWatchingNewWebContents();

  WaitForGaiaPageLoadAndPropertyUpdate();

  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(navigation_observer.last_navigation_url(), frame_url_);
  const net::Error expected_error = (GetParam() == FrameUrlOrigin::kSameOrigin)
                                        ? net::OK
                                        : net::ERR_BLOCKED_BY_RESPONSE;
  EXPECT_EQ(navigation_observer.last_net_error_code(), expected_error);

  ExpectIdentifierPage();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  // Make sure that the error screen has not been shown in the meantime.
  // It is not sufficient to just wait for the Gaia screen / check that the gaia
  // screen is currently being replaced, because the error screen could have
  // been shown in the meantime (and then exited again because the "device" has
  // internet connectivity).
  EXPECT_FALSE(error_screen_watcher.has_error_screen_been_shown());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebviewLoginWithIframeTest,
                         testing::Values(FrameUrlOrigin::kSameOrigin,
                                         FrameUrlOrigin::kDifferentOrigin));

// Base class for tests of the client certificates in the sign-in frame.
class WebviewClientCertsLoginTestBase : public WebviewLoginTest {
 public:
  WebviewClientCertsLoginTestBase() = default;
  WebviewClientCertsLoginTestBase(const WebviewClientCertsLoginTestBase&) =
      delete;
  WebviewClientCertsLoginTestBase& operator=(
      const WebviewClientCertsLoginTestBase&) = delete;

  // Sets up the DeviceLoginScreenAutoSelectCertificateForUrls policy.
  void SetAutoSelectCertificatePatterns(
      const std::vector<std::string>& autoselect_patterns) {
    em::ChromeDeviceSettingsProto& proto(device_policy_builder_.payload());
    auto* field =
        proto.mutable_device_login_screen_auto_select_certificate_for_urls();
    for (const std::string& autoselect_pattern : autoselect_patterns)
      field->add_login_screen_auto_select_certificate_rules(autoselect_pattern);

    device_policy_builder_.Build();

    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder_.GetBlob());
    PrefChangeWatcher watcher(prefs::kManagedAutoSelectCertificateForUrls,
                              ProfileHelper::GetSigninProfile()->GetPrefs());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);

    watcher.Wait();
  }

  // Adds the certificate from `authority_file_path` (PEM) as untrusted
  // authority in device OpenNetworkConfiguration policy.
  void SetIntermediateAuthorityInDeviceOncPolicy(
      const base::FilePath& authority_file_path) {
    std::string x509_contents;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      ASSERT_TRUE(base::ReadFileToString(authority_file_path, &x509_contents));
    }
    base::Value::Dict onc_dict =
        BuildDeviceOncDictForUntrustedAuthority(x509_contents);

    em::ChromeDeviceSettingsProto& proto(device_policy_builder_.payload());
    base::JSONWriter::Write(onc_dict,
                            proto.mutable_open_network_configuration()
                                ->mutable_open_network_configuration());

    device_policy_builder_.Build();

    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder_.GetBlob());
    PrefChangeWatcher watcher(onc::prefs::kDeviceOpenNetworkConfiguration,
                              g_browser_process->local_state());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
    watcher.Wait();
  }

  // Sets the DeviceLoginScreenPromptOnMultipleMatchingCertificates device
  // policy.
  void SetPromptOnMultipleMatchingCertificatesPolicy(
      bool prompt_on_multiple_matches) {
    em::ChromeDeviceSettingsProto& proto(device_policy_builder_.payload());
    proto.mutable_login_screen_prompt_on_multiple_matching_certificates()
        ->set_value(prompt_on_multiple_matches);
    device_policy_builder_.Build();

    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder_.GetBlob());
    PrefChangeWatcher watcher(prefs::kPromptOnMultipleMatchingCertificates,
                              ProfileHelper::GetSigninProfile()->GetPrefs());
    FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
    watcher.Wait();
  }

  // Starts the Test HTTPS server with `ssl_options`.
  void StartHttpsServer(const net::SSLServerConfig& server_config) {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK,
                                server_config);
    https_server_->RegisterRequestHandler(base::BindLambdaForTesting(
        [this](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != "/client-cert") {
            return nullptr;
          }
          {
            // Save the `SSLInfo` for `RequestClientCertTestPageInFrame`.
            base::AutoLock lock(server_ssl_info_lock_);
            DCHECK(request.ssl_info);
            server_ssl_info_ = request.ssl_info;
          }
          return std::make_unique<net::test_server::BasicHttpResponse>();
        }));
    ASSERT_TRUE(https_server_->Start());
  }

  // Requests `http_server_`'s client-cert test page in the webview specified by
  // the given `webview_path`. Returns the `net::SSLInfo` as observed by the
  // server, or `absl::nullopt` if the server did not report any such value.
  absl::optional<net::SSLInfo> RequestClientCertTestPageInFrame(
      test::JSChecker js_checker,
      const std::string& webview_path) {
    const GURL url = https_server_->GetURL("/client-cert");
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();
    navigation_observer.StartWatchingNewWebContents();

    js_checker.Evaluate(base::StringPrintf("%s.src='%s'", webview_path.c_str(),
                                           url.spec().c_str()));
    navigation_observer.Wait();

    base::AutoLock lock(server_ssl_info_lock_);
    absl::optional<net::SSLInfo> server_ssl_info = std::move(server_ssl_info_);
    server_ssl_info_ = absl::nullopt;
    return server_ssl_info;
  }

  void ShowEulaScreen() {
    LoginDisplayHost::default_host()->StartWizard(EulaView::kScreenId);
    OobeScreenWaiter(EulaView::kScreenId).Wait();
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    // Override FakeSessionManagerClient. This will be shut down by the browser.
    SessionManagerClient::InitializeFakeInMemory();
    device_policy_builder_.Build();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder_.GetBlob());

    WebviewLoginTest::SetUpInProcessBrowserTestFixture();
  }

  void ImportSystemSlotClientCerts(
      const std::vector<std::string>& client_cert_names,
      PK11SlotInfo* system_slot) {
    base::ScopedAllowBlockingForTesting allow_io;
    for (const auto& client_cert_name : client_cert_names) {
      const base::FilePath base_file_name =
          base::FilePath::FromASCII(client_cert_name);
      const base::FilePath pem_file_name =
          base_file_name.AddExtensionASCII("pem");
      const base::FilePath pk8_file_name =
          base_file_name.AddExtensionASCII("pk8");
      scoped_refptr<net::X509Certificate> client_cert =
          net::ImportClientCertAndKeyFromFile(
              net::GetTestCertsDirectory(), pem_file_name.MaybeAsASCII(),
              pk8_file_name.MaybeAsASCII(), system_slot);
      if (!client_cert)
        ADD_FAILURE() << "Failed to import cert from " << client_cert_name;
    }
  }

 private:
  // Builds a device ONC dictionary defining a single untrusted authority
  // certificate.
  static base::Value::Dict BuildDeviceOncDictForUntrustedAuthority(
      const std::string& x509_authority_cert) {
    base::Value::Dict onc_certificate;
    onc_certificate.Set(onc::certificate::kGUID, base::Value(kTestGuid));
    onc_certificate.Set(onc::certificate::kType,
                        base::Value(onc::certificate::kAuthority));
    onc_certificate.Set(onc::certificate::kX509,
                        base::Value(x509_authority_cert));

    base::Value::List onc_certificates;
    onc_certificates.Append(std::move(onc_certificate));

    base::Value::Dict onc_dict;
    onc_dict.Set(onc::toplevel_config::kCertificates,
                 std::move(onc_certificates));
    onc_dict.Set(onc::toplevel_config::kType,
                 base::Value(onc::toplevel_config::kUnencryptedConfiguration));
    return onc_dict;
  }

  policy::DevicePolicyBuilder device_policy_builder_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  // `net::EmbeddedTestServer`'s callbacks run on a background thread, so this
  // field must be protected with a lock.
  base::Lock server_ssl_info_lock_;
  absl::optional<net::SSLInfo> server_ssl_info_
      GUARDED_BY(server_ssl_info_lock_);

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Tests of the client certificates in the sign-in frame. The testing system
// slot is pre-initialized with a client cert.
class WebviewClientCertsLoginTest : public WebviewClientCertsLoginTestBase {
 public:
  WebviewClientCertsLoginTest() = default;

  WebviewClientCertsLoginTest(const WebviewClientCertsLoginTest&) = delete;
  WebviewClientCertsLoginTest& operator=(const WebviewClientCertsLoginTest&) =
      delete;

  // Imports specified client certificates into the system slot.
  void SetUpClientCertsInSystemSlot(
      const std::vector<std::string>& client_cert_names) {
    ImportSystemSlotClientCerts(client_cert_names,
                                system_nss_key_slot_mixin_.slot());
  }

 protected:
  LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId),
      user_manager::USER_TYPE_REGULAR};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {test_user_}};

 private:
  ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
};

// Tests that client certificate authentication is not enabled in a webview on
// the sign-in screen which is not the sign-in frame. In this case, the EULA
// webview is used.
// TODO(pmarko): This is DISABLED because the eula UI it depends on has been
// deprecated and removed. https://crbug.com/849710.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsLoginTest,
                       DISABLED_ClientCertRequestedInOtherWebView) {
  ASSERT_NO_FATAL_FAILURE(SetUpClientCertsInSystemSlot({kClientCert1Name}));
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(server_config));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  ShowEulaScreen();

  // Use `watch_new_webcontents` because the EULA webview has not navigated yet.
  absl::optional<net::SSLInfo> ssl_info =
      RequestClientCertTestPageInFrame(test::OobeJS(), "$('cros-eula-frame')");
  ASSERT_TRUE(ssl_info);
  EXPECT_FALSE(ssl_info->cert);
}

namespace {

// Parameter type for the `SigninFrameWebviewClientCertsLoginTest` parameterized
// test fixture.
struct SigninCertParam {
  // Arrange the test to install these client certificates (specified by name,
  // e.g., "client1") into the system slot - see
  // `SetUpClientCertsInSystemSlot()`.
  std::vector<std::string> client_certs;
  // If non-null, arrange the test to configure this intermediate CA (specified
  // by name, e.g., "client_1_ca") as known to the client via device policy -
  // see `SetIntermediateAuthorityInDeviceOncPolicy()`.
  absl::optional<std::string> intermediate_cert;
  // Arrange the test to configure these certificate auto-selection patterns in
  // device policy - see `SetAutoSelectCertificatePatterns()`.
  std::vector<std::string> autoselect_patterns;
  // If non-null, arrange the test to configure the device policy for prompting
  // when multiple certificates are auto-selected - see
  // `SetPromptOnMultipleMatchingCertificatesPolicy()`.
  absl::optional<bool> prompt_on_multiple_matches;
  // Make the web server include the specified CA certificates in its client
  // certificate request. Entries should be DER-encoded X.509 names.
  std::vector<std::string> ca_certs;
  // If non-null, simulate a user gesture to select the given client certificate
  // (specified by name, e.g., "client1") in the cert selector dialog.
  absl::optional<std::string> manually_select_cert;
  // Assert that the selected certificate is the one specified here. When null,
  // asserts that no certificate is selected.
  absl::optional<std::string> assert_cert;
};

}  // namespace

// Parameterized test fixture for simple testing of the client certificate
// selection behavior in the sign-in frame.
class SigninFrameWebviewClientCertsLoginTest
    : public WebviewClientCertsLoginTest,
      public ::testing::WithParamInterface<SigninCertParam> {
 protected:
  // Configures the specified certificate to be chosen in the certificate
  // selector dialog once it's opened.
  void SimulateUserWillSelectClientCert(
      const std::string& cert_name_to_select) {
    chrome::SetShowSSLClientCertificateSelectorHookForTest(base::BindRepeating(
        &SigninFrameWebviewClientCertsLoginTest::OnClientCertSelectorRequested,
        cert_name_to_select));
  }

 private:
  static base::OnceClosure OnClientCertSelectorRequested(
      const std::string& cert_name_to_select,
      content::WebContents* contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) {
    for (auto& cert_identity : client_certs) {
      if (EqualsTestCert(*cert_identity->certificate(), cert_name_to_select)) {
        scoped_refptr<net::X509Certificate> cert = cert_identity->certificate();
        net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
            std::move(cert_identity),
            base::BindOnce(
                &content::ClientCertificateDelegate::ContinueWithCertificate,
                std::move(delegate), cert));
        // Return a null cancellation callback - cancelling is not supported.
        return base::OnceClosure();
      }
    }
    ADD_FAILURE() << "Cannot select cert " << cert_name_to_select
                  << ": not present in the cert selector";
    // Return a null cancellation callback - cancelling is not supported.
    return base::OnceClosure();
  }
};

IN_PROC_BROWSER_TEST_P(SigninFrameWebviewClientCertsLoginTest,
                       LoginScreenTest) {
  // Arrange the system slot.
  ASSERT_NO_FATAL_FAILURE(
      SetUpClientCertsInSystemSlot(GetParam().client_certs));
  // Arrange the device policy.
  if (GetParam().intermediate_cert) {
    const base::FilePath intermediate_cert_path =
        net::GetTestCertsDirectory()
            .AppendASCII(*GetParam().intermediate_cert)
            .AddExtensionASCII("pem");
    ASSERT_NO_FATAL_FAILURE(
        SetIntermediateAuthorityInDeviceOncPolicy(intermediate_cert_path));
  }
  SetAutoSelectCertificatePatterns(GetParam().autoselect_patterns);
  if (GetParam().prompt_on_multiple_matches) {
    SetPromptOnMultipleMatchingCertificatesPolicy(
        *GetParam().prompt_on_multiple_matches);
  }

  // Prepare the test server for the "act" part of the test.
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
  server_config.cert_authorities = GetParam().ca_certs;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(server_config));
  // Prepare the certificate selector hook for simulating the user gesture in
  // the "act" part of the test.
  if (GetParam().manually_select_cert)
    SimulateUserWillSelectClientCert(*GetParam().manually_select_cert);

  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Act: navigate to the page hosted by the test server.
  absl::optional<net::SSLInfo> ssl_info =
      RequestClientCertTestPageInFrame(test::OobeJS(), kSigninWebview);
  ASSERT_TRUE(ssl_info);

  // Assert the expectation on the client certificate that got selected.
  if (GetParam().assert_cert) {
    ASSERT_TRUE(ssl_info->cert);
    EXPECT_THAT(*ssl_info->cert, EqualsCert(*GetParam().assert_cert));
  } else {
    EXPECT_FALSE(ssl_info->cert);
  }
}

IN_PROC_BROWSER_TEST_P(SigninFrameWebviewClientCertsLoginTest, LockscreenTest) {
  // Arrange the system slot.
  ASSERT_NO_FATAL_FAILURE(
      SetUpClientCertsInSystemSlot(GetParam().client_certs));
  // Arrange the device policy.
  if (GetParam().intermediate_cert) {
    const base::FilePath intermediate_cert_path =
        net::GetTestCertsDirectory()
            .AppendASCII(*GetParam().intermediate_cert)
            .AddExtensionASCII("pem");
    ASSERT_NO_FATAL_FAILURE(
        SetIntermediateAuthorityInDeviceOncPolicy(intermediate_cert_path));
  }
  SetAutoSelectCertificatePatterns(GetParam().autoselect_patterns);
  if (GetParam().prompt_on_multiple_matches) {
    SetPromptOnMultipleMatchingCertificatesPolicy(
        *GetParam().prompt_on_multiple_matches);
  }

  // Prepare the test server for the "act" part of the test.
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
  server_config.cert_authorities = GetParam().ca_certs;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(server_config));
  // Prepare the certificate selector hook for simulating the user gesture in
  // the "act" part of the test.
  if (GetParam().manually_select_cert)
    SimulateUserWillSelectClientCert(*GetParam().manually_select_cert);

  // Log in a user and lock the screen, then trigger the lock screen SAML reauth
  // dialog.
  login_manager_mixin_.LoginWithDefaultContext(test_user_);
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> lock_screen_reauth_dialog =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(lock_screen_reauth_dialog);
  lock_screen_reauth_dialog->ForceSamlRedirect();
  lock_screen_reauth_dialog->ExpectVerifyAccountScreenVisible();
  lock_screen_reauth_dialog->ClickVerifyButton();
  lock_screen_reauth_dialog->WaitForSamlScreen();

  // Act: navigate to the page hosted by the test server in the sign-in frame of
  // the lock screen SAML reauth dialog.
  absl::optional<net::SSLInfo> ssl_info = RequestClientCertTestPageInFrame(
      lock_screen_reauth_dialog->DialogJS(), kSigninWebviewOnLockScreen);
  ASSERT_TRUE(ssl_info);

  // Assert the expectation on the client certificate that got selected.
  if (GetParam().assert_cert) {
    ASSERT_TRUE(ssl_info->cert);
    EXPECT_THAT(*ssl_info->cert, EqualsCert(*GetParam().assert_cert));
  } else {
    EXPECT_FALSE(ssl_info->cert);
  }
}

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server does not request
// certificates signed by a specific authority.
INSTANTIATE_TEST_SUITE_P(
    SuccessSimple,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name, kClientCert2Name},
        /*intermediate_cert=*/absl::nullopt,
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/{},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/kClientCert1Name}));

// Test that client certificate autoselect selects the right certificate even
// with multiple filters for the same pattern.
INSTANTIATE_TEST_SUITE_P(
    SuccessMultipleFilters,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name, kClientCert2Name},
        /*intermediate_cert=*/absl::nullopt,
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})",
         R"({"pattern": "*", "filter": {"ISSUER": {"CN": "foo bar"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/{},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/kClientCert1Name}));

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests a
// certificate signed by a specific authority.
INSTANTIATE_TEST_SUITE_P(
    SuccessViaCa,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name, kClientCert2Name},
        /*intermediate_cert=*/absl::nullopt,
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/
        {// client_1_ca ("B CA")
         {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03,
          0x0c, 0x04, 0x42, 0x20, 0x43, 0x41}},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/kClientCert1Name}));

// Test that client certificate will be discovered if the server requests
// certificates signed by a root authority, the installed certificate has been
// issued by an intermediate authority, and the intermediate authority is
// known on the device (it has been made available through device ONC policy).
INSTANTIATE_TEST_SUITE_P(
    SuccessViaCaAndIntermediate,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name, kClientCert2Name},
        /*intermediate_cert=*/"client_1_ca",
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/
        {// client_root_ca ("C Root CA")
         {0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
          0x0c, 0x09, 0x43, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41}},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/kClientCert1Name}));

// Test that if no client certificate is auto-selected using policy on the
// sign-in frame, the client does not send up any client certificate.
INSTANTIATE_TEST_SUITE_P(ErrorNoAutoSelect,
                         SigninFrameWebviewClientCertsLoginTest,
                         testing::Values(SigninCertParam{
                             /*client_certs=*/{kClientCert1Name},
                             /*intermediate_cert=*/absl::nullopt,
                             /*autoselect_patterns=*/{},
                             /*prompt_on_multiple_matches=*/absl::nullopt,
                             /*ca_certs=*/
                             {// client_1_ca ("B CA")
                              {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03,
                               0x55, 0x04, 0x03, 0x0c, 0x04, 0x42, 0x20, 0x43,
                               0x41}},
                             /*manually_select_cert=*/absl::nullopt,
                             /*assert_cert=*/absl::nullopt}));

// Test that client certificate authentication using certificates from the
// system slot is enabled in the sign-in frame. The server requests
// a certificate signed by a specific authority. The client doesn't have a
// matching certificate.
INSTANTIATE_TEST_SUITE_P(
    ErrorWrongCa,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name},
        /*intermediate_cert=*/absl::nullopt,
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/
        {// client_2_ca ("E CA")
         {0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03,
          0x0c, 0x04, 0x45, 0x20, 0x43, 0x41}},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/absl::nullopt}));

// Test that client certificate will not be discovered if the server requests
// certificates signed by a root authority, the installed certificate has been
// issued by an intermediate authority, and the intermediate authority is not
// known on the device (it has not been made available through device ONC
// policy).
INSTANTIATE_TEST_SUITE_P(
    ErrorNoIntermediateCa,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{
        /*client_certs=*/{kClientCert1Name, kClientCert2Name},
        /*intermediate_cert=*/absl::nullopt,
        /*autoselect_patterns=*/
        {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
        /*prompt_on_multiple_matches=*/absl::nullopt,
        /*ca_certs=*/
        {// client_root_ca ("C Root CA")
         {0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
          0x0c, 0x09, 0x43, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41}},
        /*manually_select_cert=*/absl::nullopt,
        /*assert_cert=*/absl::nullopt}));

// Test that the DeviceLoginScreenPromptOnMultipleMatchingCertificates policy
// doesn't prevent the client cert from being auto-selected via policy.
INSTANTIATE_TEST_SUITE_P(
    SuccessRegardlessOfPromptPolicy,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(
        SigninCertParam{
            /*client_certs=*/{kClientCert1Name, kClientCert2Name},
            /*intermediate_cert=*/absl::nullopt,
            /*autoselect_patterns=*/
            {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
            /*prompt_on_multiple_matches=*/false,
            /*ca_certs=*/{},
            /*manually_select_cert=*/absl::nullopt,
            /*assert_cert=*/kClientCert1Name},
        SigninCertParam{
            /*client_certs=*/{kClientCert1Name, kClientCert2Name},
            /*intermediate_cert=*/absl::nullopt,
            /*autoselect_patterns=*/
            {R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"},
            /*prompt_on_multiple_matches=*/true,
            /*ca_certs=*/{},
            /*manually_select_cert=*/absl::nullopt,
            /*assert_cert=*/kClientCert1Name}));
// Test that the DeviceLoginScreenPromptOnMultipleMatchingCertificates policy
// doesn't affect the failure to select a client cert when no auto-selection is
// configured.
INSTANTIATE_TEST_SUITE_P(
    ErrorNoPatternRegardlessOfPromptPolicy,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(SigninCertParam{/*client_certs=*/{kClientCert1Name},
                                    /*intermediate_cert=*/absl::nullopt,
                                    /*autoselect_patterns=*/{},
                                    /*prompt_on_multiple_matches=*/false,
                                    /*ca_certs=*/{},
                                    /*manually_select_cert=*/absl::nullopt,
                                    /*assert_cert=*/absl::nullopt},
                    SigninCertParam{/*client_certs=*/{kClientCert1Name},
                                    /*intermediate_cert=*/absl::nullopt,
                                    /*autoselect_patterns=*/{},
                                    /*prompt_on_multiple_matches=*/true,
                                    /*ca_certs=*/{},
                                    /*manually_select_cert=*/absl::nullopt,
                                    /*assert_cert=*/absl::nullopt}));
// Test that the certificate can be manually selected in case the auto-selection
// matches multiple certificates and the
// DeviceLoginScreenPromptOnMultipleMatchingCertificates policy is set to true.
INSTANTIATE_TEST_SUITE_P(
    SuccessManualSelection,
    SigninFrameWebviewClientCertsLoginTest,
    testing::Values(
        SigninCertParam{/*client_certs=*/{kClientCert1Name, kClientCert2Name},
                        /*intermediate_cert=*/absl::nullopt,
                        /*autoselect_patterns=*/
                        {R"({"pattern": "*", "filter": {}})"},
                        /*prompt_on_multiple_matches=*/true,
                        /*ca_certs=*/{},
                        /*manually_select_cert=*/kClientCert1Name,
                        /*assert_cert=*/kClientCert1Name},
        SigninCertParam{/*client_certs=*/{kClientCert1Name, kClientCert2Name},
                        /*intermediate_cert=*/absl::nullopt,
                        /*autoselect_patterns=*/
                        {R"({"pattern": "*", "filter": {}})"},
                        /*prompt_on_multiple_matches=*/true,
                        /*ca_certs=*/{},
                        /*manually_select_cert=*/kClientCert2Name,
                        /*assert_cert=*/kClientCert2Name}));

// Tests the scenario where the system token is not initialized initially (due
// to the TPM not being ready).
class WebviewClientCertsTokenLoadingLoginTest
    : public WebviewClientCertsLoginTestBase {
 public:
  WebviewClientCertsTokenLoadingLoginTest() {
    // At very early stage, the system slot is being initialized becuase fake
    // tpm manager tells the TPM is owned by default. So, it has to be overriden
    // here instead of in the test body or `SetUpOnMainThread()`.
    chromeos::TpmManagerClient::InitializeFake();
    chromeos::TpmManagerClient::Get()
        ->GetTestInterface()
        ->mutable_nonsensitive_status_reply()
        ->set_is_owned(false);
  }

  WebviewClientCertsTokenLoadingLoginTest(
      const WebviewClientCertsTokenLoadingLoginTest&) = delete;
  WebviewClientCertsTokenLoadingLoginTest& operator=(
      const WebviewClientCertsTokenLoadingLoginTest&) = delete;

  // Prepares a testing system slot (without injecting it as an already
  // initialized yet) and imports a client certificate into it.
  void PrepareSystemSlot() {
    bool out_system_slot_prepared_successfully = false;
    base::RunLoop loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &WebviewClientCertsTokenLoadingLoginTest::PrepareSystemSlotOnIO,
            base::Unretained(this), &out_system_slot_prepared_successfully),
        loop.QuitClosure());
    loop.Run();
    ASSERT_TRUE(out_system_slot_prepared_successfully);

    ASSERT_NO_FATAL_FAILURE(ImportSystemSlotClientCerts(
        {kClientCert1Name}, test_system_slot_nss_db_->slot()));
  }

 protected:
  void SetUpOnMainThread() override {
    TPMTokenLoader::Get()->enable_tpm_loading_for_testing(true);
    WebviewClientCertsLoginTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    TearDownTestSystemSlot();
    WebviewClientCertsLoginTestBase::TearDownOnMainThread();
  }

 private:
  void PrepareSystemSlotOnIO(bool* out_system_slot_prepared_successfully) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_nss_db_ =
        std::make_unique<crypto::ScopedTestSystemNSSKeySlot>(
            /*simulate_token_loader=*/false);
    *out_system_slot_prepared_successfully =
        test_system_slot_nss_db_->ConstructedSuccessfully();
  }

  void TearDownTestSystemSlot() {
    base::RunLoop loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&WebviewClientCertsTokenLoadingLoginTest::
                           TearDownTestSystemSlotOnIO,
                       base::Unretained(this)),
        loop.QuitClosure());
    loop.Run();
  }

  void TearDownTestSystemSlotOnIO() { test_system_slot_nss_db_.reset(); }

  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_nss_db_;
};

namespace {

void GotIsTpmTokenEnabledOnUIThread(base::OnceClosure run_loop_quit_closure,
                                    bool* is_ready,
                                    bool is_tpm_token_enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  *is_ready = is_tpm_token_enabled;
  std::move(run_loop_quit_closure).Run();
}

void GotIsTpmTokenEnabledOnIOThread(base::OnceCallback<void(bool)> ui_callback,
                                    bool is_tpm_token_enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_callback), is_tpm_token_enabled));
}

bool IsTpmTokenEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  bool is_ready = false;

  auto ui_callback =
      base::BindOnce(&GotIsTpmTokenEnabledOnUIThread, run_loop.QuitClosure(),
                     base::Unretained(&is_ready));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&crypto::IsTPMTokenEnabled,
                                base::BindOnce(&GotIsTpmTokenEnabledOnIOThread,
                                               std::move(ui_callback))));
  run_loop.Run();
  return is_ready;
}

void GotIsSystemSlotAvailableOnUIThread(base::OnceClosure run_loop_quit_closure,
                                        bool* result,
                                        bool is_system_slot_available) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  *result = is_system_slot_available;
  std::move(run_loop_quit_closure).Run();
}

void GotSystemSlotOnIOThread(base::OnceCallback<void(bool)> ui_callback,
                             crypto::ScopedPK11Slot system_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_callback), !!system_slot));
}

bool IsSystemSlotAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::RunLoop run_loop;
  bool result = false;

  auto ui_callback =
      base::BindOnce(&GotIsSystemSlotAvailableOnUIThread,
                     run_loop.QuitClosure(), base::Unretained(&result));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&crypto::GetSystemNSSKeySlot,
                                base::BindOnce(&GotSystemSlotOnIOThread,
                                               std::move(ui_callback))));

  run_loop.Run();
  return result;
}

}  // namespace

// Test that the system slot becomes initialized and the client certificate
// authentication works in the sign-in frame after the TPM gets reported as
// ready.
IN_PROC_BROWSER_TEST_F(WebviewClientCertsTokenLoadingLoginTest,
                       SystemSlotEnabled) {
  ASSERT_NO_FATAL_FAILURE(PrepareSystemSlot());
  net::SSLServerConfig server_config;
  server_config.client_cert_type = net::SSLServerConfig::OPTIONAL_CLIENT_CERT;
  ASSERT_NO_FATAL_FAILURE(StartHttpsServer(server_config));

  const std::vector<std::string> autoselect_patterns = {
      R"({"pattern": "*", "filter": {"ISSUER": {"CN": "B CA"}}})"};
  SetAutoSelectCertificatePatterns(autoselect_patterns);

  WaitForGaiaPageLoadAndPropertyUpdate();

  // Report the TPM as ready, triggering the system token initialization by
  // SystemTokenCertDBInitializer.
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(true);
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->EmitOwnershipTakenSignal();

  absl::optional<net::SSLInfo> ssl_info =
      RequestClientCertTestPageInFrame(test::OobeJS(), kSigninWebview);
  ASSERT_TRUE(ssl_info);
  ASSERT_TRUE(ssl_info->cert);
  EXPECT_THAT(*ssl_info->cert, EqualsCert(std::string(kClientCert1Name)));

  EXPECT_TRUE(IsTpmTokenEnabled());
  EXPECT_TRUE(IsSystemSlotAvailable());
}

IN_PROC_BROWSER_TEST_F(WebviewClientCertsTokenLoadingLoginTest,
                       SystemSlotDisabled) {
  WaitForGaiaPageLoadAndPropertyUpdate();

  // Report the TPM as ready, triggering the system token initialization by
  // SystemTokenCertDBInitializer.
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owned(false);
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->EmitOwnershipTakenSignal();

  EXPECT_FALSE(IsTpmTokenEnabled());
  EXPECT_FALSE(IsSystemSlotAvailable());
}

class WebviewProxyAuthLoginTest : public WebviewLoginTest {
 public:
  WebviewProxyAuthLoginTest()
      : auth_proxy_server_(std::make_unique<net::SpawnedTestServer>(
            net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
            base::FilePath())) {}

  WebviewProxyAuthLoginTest(const WebviewProxyAuthLoginTest&) = delete;
  WebviewProxyAuthLoginTest& operator=(const WebviewProxyAuthLoginTest&) =
      delete;

 protected:
  void SetUp() override {
    // Start proxy server
    auth_proxy_server_->set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(auth_proxy_server_->Start());

    WebviewLoginTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        ::switches::kProxyServer,
        auth_proxy_server_->host_port_pair().ToString());
    WebviewLoginTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebviewLoginTest::SetUpInProcessBrowserTestFixture();

    // Prepare device policy which will be used for two purposes:
    // - given to FakeSessionManagerClient, so the device appears to have
    //   registered for policy.
    // - the payload is given to `policy_test_server_`, so we can download fresh
    //   policy.
    device_policy_builder()->policy_data().set_public_key_version(1);
    device_policy_builder()->Build();

    UpdateServedPolicyFromDevicePolicyTestHelper();
    FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_builder()->GetBlob());

    // Set some fake state keys to make sure they are not empty.
    std::vector<std::string> state_keys;
    state_keys.push_back("1");
    FakeSessionManagerClient::Get()->set_server_backed_state_keys(state_keys);
  }

  void SetUpOnMainThread() override {
    // Setup the observer reacting on NOTIFICATION_AUTH_NEEDED before the test
    // runs because there is no action we actively trigger to request proxy
    // authentication. Instead, the sign-in screen automatically shows the gaia
    // webview, which will request the gaia URL, which leads to a login prompt.
    auth_needed_wait_loop_ = std::make_unique<base::RunLoop>();
    auth_needed_observer_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_AUTH_NEEDED,
            base::BindRepeating(&WebviewProxyAuthLoginTest::OnAuthRequested,
                                base::Unretained(this)));

    WebviewLoginTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    WebviewLoginTest::TearDownOnMainThread();

    auth_needed_observer_.reset();
    auth_needed_wait_loop_.reset();
  }

  bool OnAuthRequested(const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    // Only care for notifications originating from the frame which is
    // displaying gaia.
    content::WebContents* main_web_contents = GetLoginUI()->GetWebContents();
    content::RenderFrameHost* gaia_rfh =
        signin::GetAuthFrame(main_web_contents, gaia_frame_parent_);
    LoginHandler* login_handler =
        content::Details<LoginNotificationDetails>(details)->handler();
    if (login_handler->web_contents() !=
        content::WebContents::FromRenderFrameHost(gaia_rfh))
      return false;

    gaia_frame_login_handler_ = login_handler;
    auth_needed_wait_loop_->Quit();
    return true;
  }

  // Waits until proxy authentication has been requested by the frame displaying
  // gaia. Returns the LoginHandler handling this authentication request.
  LoginHandler* WaitForAuthRequested() {
    auth_needed_wait_loop_->Run();
    return gaia_frame_login_handler_;
  }

  void UpdateServedPolicyFromDevicePolicyTestHelper() {
    policy_test_server_mixin_.UpdateDevicePolicy(
        device_policy_builder()->payload());
  }

  policy::DevicePolicyBuilder* device_policy_builder() {
    return &device_policy_builder_;
  }

  content::WindowedNotificationObserver* auth_needed_observer() {
    return auth_needed_observer_.get();
  }

 private:
  std::unique_ptr<content::WindowedNotificationObserver> auth_needed_observer_;
  std::unique_ptr<base::RunLoop> auth_needed_wait_loop_;
  // Unowned pointer - set to the LoginHandler of the frame displaying gaia.
  LoginHandler* gaia_frame_login_handler_ = nullptr;

  // A proxy server which requires authentication using the 'Basic'
  // authentication method.
  std::unique_ptr<net::SpawnedTestServer> auth_proxy_server_;
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  policy::DevicePolicyBuilder device_policy_builder_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// TODO(crbug.com/1377241): The test times out on ASAN.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ProxyAuthTransfer DISABLED_ProxyAuthTransfer
#else
#define MAYBE_ProxyAuthTransfer ProxyAuthTransfer
#endif
IN_PROC_BROWSER_TEST_F(WebviewProxyAuthLoginTest, MAYBE_ProxyAuthTransfer) {
  WaitForSigninScreen();

  LoginHandler* login_handler = WaitForAuthRequested();

  // Before entering auth data, make `policy_test_server_` serve a policy that
  // we can use to detect if policies have been fetched.
  em::ChromeDeviceSettingsProto& device_policy =
      device_policy_builder()->payload();
  device_policy.mutable_device_login_screen_auto_select_certificate_for_urls()
      ->add_login_screen_auto_select_certificate_rules("test_pattern");
  UpdateServedPolicyFromDevicePolicyTestHelper();

  policy::PolicyChangeRegistrar policy_change_registrar(
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetPolicyService(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                              std::string() /* component_id */));

  // Now enter auth data
  login_handler->SetAuth(u"foo", u"bar");
  WaitForGaiaPageLoad();

  base::RunLoop run_loop;
  policy_change_registrar.Observe(
      policy::key::kDeviceLoginScreenAutoSelectCertificateForUrls,
      base::BindRepeating(&PolicyChangedCallback, run_loop.QuitClosure()));
  run_loop.Run();

  // Press the back button at a sign-in screen without pre-existing users to
  // start a new sign-in attempt.
  // This will re-load gaia, rotating the StoragePartition. The new
  // StoragePartition must also have the proxy auth details.
  test::OobeJS().ClickOnPath(kBackButton);
  WaitForGaiaPageLoadAndPropertyUpdate();
  // Expect that we got back to the identifier page, as there are no known users
  // so the sign-in screen will not display user pods.
  ExpectIdentifierPage();
}

class WebviewChildLoginTest : public WebviewLoginTest {
 public:
  WebviewChildLoginTest() = default;

  // WebviewLoginTest:
  void SetUpInProcessBrowserTestFixture() override {
    user_policy_mixin_.RequestPolicyUpdate();
    fake_gaia_.SetupFakeGaiaForChildUser(
        child_account_id_.GetUserEmail(), child_account_id_.GetGaiaId(),
        FakeGaiaMixin::kFakeRefreshToken, false /*issue_any_scope_token*/);

    WebviewLoginTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  AccountId child_account_id_{
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId)};
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, child_account_id_,
                                     &policy_test_server_mixin_};
};

// Test verfies case when user info message sent before authentication is
// finished.
IN_PROC_BROWSER_TEST_F(WebviewChildLoginTest, UserInfoSentBeforeAuthFinished) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();
  DisableImplicitServices();
  SigninFrameJS().TypeIntoPath(child_account_id_.GetUserEmail(),
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  SigninFrameJS().ExecuteAsync("gaia.chromeOSLogin.sendUserInfo(['uca'])");
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  WaitForServicesSet();

  // Timer should not be set.
  test::OobeJS().ExpectFalse("$('gaia-signin').authenticator_.gaiaDoneTimer_");

  test::WaitForPrimaryUserSessionStart();

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->GetActiveUser()->IsChild());
}

// Test verfies that user info message sent after authentication is finished
// still passes through.
IN_PROC_BROWSER_TEST_F(WebviewChildLoginTest, UserInfoSentAfterTimerSet) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();
  DisableImplicitServices();
  SigninFrameJS().TypeIntoPath(child_account_id_.GetUserEmail(),
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  // Wait for user info timer to be set.
  test::OobeJS()
      .CreateWaiter("$('gaia-signin').authenticator_.gaiaDoneTimer_")
      ->Wait();

  // Send user info after that.
  SigninFrameJS().ExecuteAsync("gaia.chromeOSLogin.sendUserInfo(['uca'])");

  test::WaitForPrimaryUserSessionStart();

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->GetActiveUser()->IsChild());
}

// Verifies flow when user info message is never sent.
IN_PROC_BROWSER_TEST_P(WebviewCloseViewLoginTest, UserInfoNeverSent) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();
  DisableImplicitServices();
  // Test will send `closerView` manually (if the feature is enabled).
  DisableCloseViewMessage();
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  if (GetParam())
    SigninFrameJS().ExecuteAsync("gaia.chromeOSLogin.sendCloseView()");

  EmulateGaiaDoneTimeout();

  test::WaitForPrimaryUserSessionStart();

  histogram_tester_.ExpectUniqueSample("ChromeOS.Gaia.Message.Gaia.UserInfo",
                                       false, 1);

  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_FALSE(user_manager->GetActiveUser()->IsChild());
}

// Verifies `ChromeOS.Gaia.PasswordFlow` events are recorded.
IN_PROC_BROWSER_TEST_F(WebviewLoginTest, PasswordMetrics) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  // This should generate first "Started" event.
  SigninFrameJS().ExecuteAsync(
      "gaia.chromeOSLogin.attemptLogin('email@email.com', 'password')");
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  // This should generate second "Started" event. And also eventually
  // "Completed" event.
  test::OobeJS().ClickOnPath(kPrimaryButton);

  test::WaitForPrimaryUserSessionStart();
  histogram_tester_.ExpectBucketCount("ChromeOS.Gaia.PasswordFlow", 0, 2);
  histogram_tester_.ExpectBucketCount("ChromeOS.Gaia.PasswordFlow", 1, 1);
}

class WebviewLoginEnrolledTest : public WebviewLoginTest {
 public:
  WebviewLoginEnrolledTest() = default;
  ~WebviewLoginEnrolledTest() override = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Verifies `OOBE.GaiaScreen.LoginRequests` and
// `OOBE.GaiaScreen.SuccessLoginRequests` are correctly recorded.
IN_PROC_BROWSER_TEST_F(WebviewLoginEnrolledTest, GaiaLoginVariantMetrics) {
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectIdentifierPage();

  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kPrimaryButton);

  // This should generate first "Started" event.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                               FakeGaiaMixin::kPasswordPath);
  // This should generate second "Started" event. And also eventually
  // "Completed" event.
  test::OobeJS().ClickOnPath(kPrimaryButton);

  test::WaitForPrimaryUserSessionStart();
  histogram_tester_.ExpectUniqueSample(kLoginRequests,
                                       GaiaView::GaiaLoginVariant::kAddUser, 1);
  histogram_tester_.ExpectUniqueSample(kSuccessLoginRequests,
                                       GaiaView::GaiaLoginVariant::kAddUser, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebviewCloseViewLoginTest,
                         testing::Bool(),
                         &WebviewCloseViewLoginTest::GetName);

}  // namespace ash
