// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/test/test_arc_session_manager.h"
#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_eula_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session_runner.h"
#include "components/arc/test/fake_arc_session.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_switches.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {
namespace {

enum class ArcState { kNotAvailable, kAcceptTerms };

std::string ArcStateToString(ArcState arc_state) {
  switch (arc_state) {
    case ArcState::kNotAvailable:
      return "not-available";
    case ArcState::kAcceptTerms:
      return "accept-terms";
  }
  NOTREACHED();
  return "unknown";
}

void RunWelcomeScreenChecks() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  constexpr int kNumberOfVideosPlaying = 1;
#else
  constexpr int kNumberOfVideosPlaying = 0;
#endif
  test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "accessibilityScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "languageScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "timezoneScreen"});

  test::OobeJS().ExpectFocused(
      {"connect", "welcomeScreen", "welcomeNextButton"});

  test::OobeJS().ExpectEQ(
      "(() => {let cnt = 0; for (let v of "
      "$('connect').$.welcomeScreen.root.querySelectorAll('video')) "
      "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
      kNumberOfVideosPlaying);

  EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
}

void RunNetworkSelectionScreenChecks() {
  test::OobeJS().ExpectEnabledPath({"network-selection", "nextButton"});

  EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().CreateFocusWaiter({"network-selection", "nextButton"})->Wait();
}

void RunEulaScreenChecks() {
  // Wait for actual EULA to appear.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-eula-md", "eulaDialog"})
      ->Wait();
  test::OobeJS().ExpectEnabledPath({"oobe-eula-md", "acceptButton"});
  test::OobeJS().CreateFocusWaiter({"oobe-eula-md", "crosEulaFrame"})->Wait();

  EXPECT_TRUE(ash::LoginScreenTestApi::IsShutdownButtonShown);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
}

void WaitForGaiaSignInScreen(bool arc_available) {
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Arc terms of service content gets preloaded when GAIA screen is shown,
  // wait for the preload to finish before proceeding - requesting reload
  // (which may happen when ARC terms of service screen is show) before the
  // preload is done may cause flaky load failures.
  // TODO(https://crbug/com/959902): Fix ARC terms of service screen to better
  //     handle this case.
  if (arc_available) {
    test::OobeJS()
        .CreateHasClassWaiter(true, "arc-tos-loaded",
                              {"arc-tos-root", "arcTosDialog"})
        ->Wait();
  }

  LOG(INFO) << "OobeInteractiveUITest: Switched to 'gaia-signin' screen.";
}

void LogInAsRegularUser() {
  LoginDisplayHost::default_host()
      ->GetOobeUI()
      ->GetView<GaiaScreenHandler>()
      ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                FakeGaiaMixin::kFakeUserPassword,
                                FakeGaiaMixin::kEmptyUserServices);
  LOG(INFO) << "OobeInteractiveUITest: Logged in.";
}

void RunSyncConsentScreenChecks() {
  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          SyncConsentScreenView::kScreenId));
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);

  const std::string button_name =
      chromeos::features::IsSplitSettingsSyncEnabled()
          ? "acceptButton"
          : "settingsSaveAndContinueButton";
  test::OobeJS().ExpectEnabledPath({"sync-consent", button_name});
  test::OobeJS().CreateFocusWaiter({"sync-consent", button_name})->Wait();

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
}

void RunFingerprintScreenChecks() {
  test::OobeJS().ExpectVisible("fingerprint-setup");
  test::OobeJS().ExpectVisiblePath({"fingerprint-setup", "setupFingerprint"});

  test::OobeJS().CreateFocusWaiter({"fingerprint-setup", "next"})->Wait();

  test::OobeJS().TapOnPath({"fingerprint-setup", "next"});
  test::OobeJS().ExpectHiddenPath({"fingerprint-setup", "setupFingerprint"});
  LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
               "to switch to placeFinger.";
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup", "placeFinger"})
      ->Wait();

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
}

void RunDiscoverScreenChecks() {
  test::OobeJS().ExpectVisible("discover");
  test::OobeJS().ExpectVisible("discover-impl");
  test::OobeJS().ExpectVisiblePath({"discover-impl", "pin-setup-impl"});
  test::OobeJS().ExpectVisiblePath(
      {"discover-impl", "pin-setup-impl", "setup"});
  test::OobeJS()
      .CreateFocusWaiter({"discover-impl", "pin-setup-impl", "pinKeyboard"})
      ->Wait();

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());
}

// Waits for the ARC terms of service screen to be shown, it accepts
// the terms, and waits for the flow to leave the ARC terms of service screen.
void HandleArcTermsOfServiceScreen() {
  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'arc-tos' screen.";

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS()
      .CreateEnabledWaiter(true, {"arc-tos-root", "arcTosNextButton"})
      ->Wait();
  test::OobeJS().TapOnPath({"arc-tos-root", "arcTosNextButton"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"arc-tos-root", "arcLocationService"})
      ->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"arc-tos-root", "arcTosAcceptButton"})
      ->Wait();

  test::OobeJS().TapOnPath({"arc-tos-root", "arcTosAcceptButton"});

  OobeScreenExitWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'arc-tos' screen done.";
}

// Waits for the recommend apps screen to be shown, selects the single app
// reported by FakeRecommendAppsFetcher, and requests the apps install. It
// will wait for the flow to progress away from the RecommendAppsScreen before
// returning.
// This assumes that ARC terms of service have bee accepted in
// HandleArcTermsOfServiceScreen.
void HandleRecommendAppsScreen() {
  OobeScreenWaiter(RecommendAppsScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'recommend-apps' screen.";

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps", "appsDialog"})
      ->Wait();

  test::OobeJS().ExpectPathDisplayed(true, {"recommend-apps", "appView"});

  std::string toggle_apps_script = base::StringPrintf(
      "(function() {"
      "  if (!document.getElementById('recommend-apps-container'))"
      "    return false;"
      "  var items ="
      "      Array.from(document.getElementById('recommend-apps-container')"
      "         .querySelectorAll('.item') || [])"
      "         .filter(i => '%s' == i.getAttribute('data-packagename'));"
      "  if (items.length == 0)"
      "    return false;"
      "  items.forEach(i => i.querySelector('.image-picker').click());"
      "  return true;"
      "})();",
      "test.package");

  const std::string webview_path =
      test::GetOobeElementPath({"recommend-apps", "appView"});
  const std::string script = base::StringPrintf(
      "(function() {"
      "  var toggleApp = function() {"
      "    %s.executeScript({code: \"%s\"}, r => {"
      "      if (!r || !r[0]) {"
      "        setTimeout(toggleApp, 50);"
      "        return;"
      "      }"
      "      window.domAutomationController.send(true);"
      "    });"
      "  };"
      "  toggleApp();"
      "})();",
      webview_path.c_str(), toggle_apps_script.c_str());

  bool result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      LoginDisplayHost::default_host()->GetOobeWebContents(), script, &result));
  EXPECT_TRUE(result);

  const std::initializer_list<base::StringPiece> install_button = {
      "recommend-apps", "installButton"};
  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().TapOnPath(install_button);

  OobeScreenExitWaiter(RecommendAppsScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'recommend-apps' screen done.";
}

// Waits for AppDownloadingScreen to be shown, clicks 'Continue' button, and
// waits until the flow moves to the next screen. This assumes that an app was
// selected in HandleRecommendAppsScreen.
void HandleAppDownloadingScreen() {
  OobeScreenWaiter(AppDownloadingScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'app-downloading' screen.";

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  const std::initializer_list<base::StringPiece> continue_button = {
      "app-downloading", "continue-setup-button"};
  test::OobeJS().TapOnPath(continue_button);

  OobeScreenExitWaiter(AppDownloadingScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'app-downloading' screen done.";
}

// Waits for AssistantOptInFlowScreen to be shown, skips the opt-in, and waits
// for the flow to move away from the screen.
// Note that due to test setup, the screen will fail to load assistant value
// proposal error (as the URL is not faked in this test), and display an
// error, This is good enough for this tests, whose goal is to verify the
// screen is shown, and how the setup progresses after the screen. The actual
// assistant opt-in flow is tested separately.
void HandleAssistantOptInScreen() {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  OobeScreenWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'assistant-optin' screen.";

  EXPECT_FALSE(ash::LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(ash::LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"assistant-optin-flow-card", "loading"})
      ->Wait();

  std::initializer_list<base::StringPiece> skip_button_path = {
      "assistant-optin-flow-card", "loading", "skip-button"};
  test::OobeJS().CreateEnabledWaiter(true, skip_button_path)->Wait();
  test::OobeJS().TapOnPath(skip_button_path);

  OobeScreenExitWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'assistant-optin' screen done.";
#endif
}

// Waits for gesture navigation to get shown, runs through all pages in the
// screen, and waits for the screen to exit.
void HandleGestureNavigationScreen() {
  OobeScreenWaiter(GestureNavigationScreenView::kScreenId).Wait();

  struct {
    std::string page_id;
    std::string button_id;
  } kPages[] = {{"gestureIntro", "gesture-intro-next-button"},
                {"gestureHome", "gesture-home-next-button"},
                {"gestureOverview", "gesture-overview-next-button"},
                {"gestureBack", "gesture-back-next-button"}};

  for (const auto& page : kPages) {
    SCOPED_TRACE(page.page_id);

    test::OobeJS()
        .CreateVisibilityWaiter(true, {"gesture-navigation", page.page_id})
        ->Wait();
    test::OobeJS().TapOnPath({"gesture-navigation", page.button_id});
  }

  OobeScreenExitWaiter(GestureNavigationScreenView::kScreenId).Wait();
}

// Waits for marketing opt in screen to get shown, then taps through the screen
// and waits for the screen to exit.
void HandleMarketingOptInScreen() {
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Enable the tablet mode shelf navigation buttons so that the next button
  // is shown on the marketing opt in screen in tablet mode.
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);

  test::OobeJS().CreateVisibilityWaiter(true, {"marketing-opt-in"})->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"marketing-opt-in", "marketing-opt-in-next-button"})
      ->Wait();
  test::OobeJS()
      .CreateFocusWaiter({"marketing-opt-in", "marketing-opt-in-next-button"})
      ->Wait();
  test::OobeJS().TapOnPath(
      {"marketing-opt-in", "marketing-opt-in-next-button"});

  OobeScreenExitWaiter(MarketingOptInScreenView::kScreenId).Wait();
}

class FakeRecommendAppsFetcher : public RecommendAppsFetcher {
 public:
  explicit FakeRecommendAppsFetcher(RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~FakeRecommendAppsFetcher() override = default;

  // RecommendAppsFetcher:
  void Start() override {
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("package_name", base::Value("test.package"));
    base::Value app_list(base::Value::Type::LIST);
    app_list.Append(std::move(app));
    delegate_->OnLoadSuccess(std::move(app_list));
  }
  void Retry() override { NOTREACHED(); }

 private:
  RecommendAppsFetcherDelegate* const delegate_;
};

std::unique_ptr<RecommendAppsFetcher> CreateRecommendAppsFetcher(
    RecommendAppsFetcherDelegate* delegate) {
  return std::make_unique<FakeRecommendAppsFetcher>(delegate);
}

class ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver
    : public extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver {
 public:
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver() {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(this);
  }

  ~ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver() {
    extensions::QuickUnlockPrivateGetAuthTokenFunction::SetTestObserver(
        nullptr);
  }

  // extensions::QuickUnlockPrivateGetAuthTokenFunction::TestObserver:
  void OnGetAuthTokenCalled(const std::string& password) override {
    get_auth_token_password_ = password;
  }

  const base::Optional<std::string>& get_auth_token_password() const {
    return get_auth_token_password_;
  }

 private:
  base::Optional<std::string> get_auth_token_password_;

  DISALLOW_COPY_AND_ASSIGN(
      ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver);
};

// Observes an |aura::Window| to see if the window was visible at some point in
// time.
class NativeWindowVisibilityObserver : public aura::WindowObserver {
 public:
  NativeWindowVisibilityObserver() = default;
  // aura::Window will remove observers on destruction.
  ~NativeWindowVisibilityObserver() override = default;

  void Observe(aura::Window* window) {
    window_ = window;
    window_->AddObserver(this);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible)
      was_visible_ = visible;
  }

  bool was_visible() { return was_visible_; }

 private:
  // The window was visible at some point in time.
  bool was_visible_ = false;

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowVisibilityObserver);
};

// Sets the |NativeWindowVisibilityObserver| to observe the
// |LoginDisplayHost|'s |NativeWindow|. This needs to be done in
// |PostProfileInit()| as the |default_host| will not be initialized before
// this.
class NativeWindowVisibilityBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  NativeWindowVisibilityBrowserMainExtraParts(
      NativeWindowVisibilityObserver* observer)
      : observer_(observer) {}
  ~NativeWindowVisibilityBrowserMainExtraParts() override = default;

  // ChromeBrowserMainExtraParts:
  void PostProfileInit() override {
    observer_->Observe(LoginDisplayHost::default_host()->GetNativeWindow());
  }

 private:
  NativeWindowVisibilityObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowVisibilityBrowserMainExtraParts);
};

class OobeEndToEndTestSetupMixin : public InProcessBrowserTestMixin {
 public:
  struct Parameters {
    bool is_tablet;
    bool is_quick_unlock_enabled;
    bool hide_shelf_controls_in_tablet_mode;
    ArcState arc_state;

    std::string ToString() const {
      return std::string("{is_tablet: ") + (is_tablet ? "true" : "false") +
             ", is_quick_unlock_enabled: " +
             (is_quick_unlock_enabled ? "true" : "false") +
             ", hide_shelf_controls_in_tablet_mode: " +
             (hide_shelf_controls_in_tablet_mode ? "true" : "false") +
             ", arc_state: " + ArcStateToString(arc_state) + "}";
    }
  };

  explicit OobeEndToEndTestSetupMixin(
      InProcessBrowserTestMixinHost* mixin_host,
      net::EmbeddedTestServer* arc_tos_server,
      const std::tuple<bool, bool, bool, ArcState>& parameters)
      : InProcessBrowserTestMixin(mixin_host), arc_tos_server_(arc_tos_server) {
    std::tie(params_.is_tablet, params_.is_quick_unlock_enabled,
             params_.hide_shelf_controls_in_tablet_mode, params_.arc_state) =
        parameters;
    // TODO(crbug.com/1101318): disable ChildSpecificSign in feature for now due
    // to test flakiness
    if (params_.hide_shelf_controls_in_tablet_mode) {
      feature_list_.InitWithFeatures(
          {ash::features::kHideShelfControlsInTabletMode},
          {features::kChildSpecificSignin});
    } else {
      feature_list_.InitWithFeatures(
          {}, {ash::features::kHideShelfControlsInTabletMode,
               features::kChildSpecificSignin});
    }
  }
  ~OobeEndToEndTestSetupMixin() override = default;

  // InProcessBrowserTestMixin:
  void SetUp() override {
    LOG(INFO) << "OOBE end-to-end test  started with params "
              << params_.ToString();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (params_.is_tablet) {
      // Makes the device capable to entering tablet mode.
      command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
      // Having an active internal display so that tablet mode does not end
      // on display config change.
      command_line->AppendSwitch(::switches::kUseFirstDisplayAsInternal);
    }

    if (params_.arc_state != ArcState::kNotAvailable) {
      arc::SetArcAvailableCommandLineForTesting(command_line);
      // Prevent encryption migration screen from showing up after user login
      // with ARC available.
      command_line->AppendSwitch(switches::kDisableEncryptionMigration);
      command_line->AppendSwitchASCII(
          switches::kArcTosHostForTests,
          arc_tos_server_->GetURL("/arc-tos").spec());
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (params_.is_quick_unlock_enabled)
      quick_unlock::EnabledForTesting(true);

    if (params_.arc_state != ArcState::kNotAvailable) {
      recommend_apps_fetcher_factory_ =
          std::make_unique<ScopedTestRecommendAppsFetcherFactory>(
              base::BindRepeating(&CreateRecommendAppsFetcher));
      if (arc_tos_server_) {
        arc_tos_server_->RegisterRequestHandler(
            base::BindRepeating(&OobeEndToEndTestSetupMixin::HandleRequest,
                                base::Unretained(this)));
      }
    }
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(arc_temp_dir_.CreateUniqueTempDir());

    if (params_.is_tablet)
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    if (params_.arc_state != ArcState::kNotAvailable) {
      // Init ArcSessionManager for testing.
      arc::ArcServiceLauncher::Get()->ResetForTesting();
      arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
          std::make_unique<arc::ArcSessionRunner>(
              base::BindRepeating(arc::FakeArcSession::Create)));
      EXPECT_TRUE(arc::ExpandPropertyFilesForTesting(
          arc::ArcSessionManager::Get(), arc_temp_dir_.GetPath()));
    }
  }
  void TearDownInProcessBrowserTestFixture() override {
    recommend_apps_fetcher_factory_.reset();
  }

  void TearDown() override { quick_unlock::EnabledForTesting(false); }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url != "/arc-tos/about/play-terms.html") {
      response->set_code(net::HTTP_NOT_FOUND);
    } else {
      response->set_code(net::HTTP_OK);
      response->set_content("<html><body>Test Terms of Service</body></html>");
      response->set_content_type("text/html");
    }
    return response;
  }

  bool is_tablet() const { return params_.is_tablet; }

  bool is_quick_unlock_enabled() const {
    return params_.is_quick_unlock_enabled;
  }

  bool hide_shelf_controls_in_tablet_mode() const {
    return params_.hide_shelf_controls_in_tablet_mode;
  }

  ArcState arc_state() const { return params_.arc_state; }

 private:
  Parameters params_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;
  net::EmbeddedTestServer* arc_tos_server_;
  base::ScopedTempDir arc_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(OobeEndToEndTestSetupMixin);
};

}  // namespace

class OobeInteractiveUITest : public OobeBaseTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<bool, bool, bool, ArcState>> {
 public:
  OobeInteractiveUITest() {
    // TODO(https://crbug.com/1130502): Merge these functions into one.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();
    sync_branded_build_override_ =
        SyncConsentScreen::ForceBrandedBuildForTesting(true);
  }
  ~OobeInteractiveUITest() override = default;

  // OobeBaseTest:
  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
    OobeBaseTest::TearDownOnMainThread();
  }

  void WaitForLoginDisplayHostShutdown() {
    if (!LoginDisplayHost::default_host())
      return;

    LOG(INFO) << "OobeInteractiveUITest: Waiting for LoginDisplayHost to "
                 "shut down.";
    test::TestPredicateWaiter(base::BindRepeating([]() {
      return !LoginDisplayHost::default_host();
    })).Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void PerformStepsBeforeEnrollmentCheck();
  void PerformSessionSignInSteps(
      const ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver&
          get_auth_token_observer);

  void SimpleEndToEnd();

  const OobeEndToEndTestSetupMixin* test_setup() const { return &setup_; }

 private:
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
  std::unique_ptr<base::AutoReset<bool>> sync_branded_build_override_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};

  net::EmbeddedTestServer arc_tos_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  EmbeddedTestServerSetupMixin arc_tos_server_setup_{&mixin_host_,
                                                     &arc_tos_server_};
  OobeEndToEndTestSetupMixin setup_{&mixin_host_, &arc_tos_server_, GetParam()};
  DISALLOW_COPY_AND_ASSIGN(OobeInteractiveUITest);
};

void OobeInteractiveUITest::PerformStepsBeforeEnrollmentCheck() {
  test::WaitForWelcomeScreen();
  RunWelcomeScreenChecks();
  test::TapWelcomeNext();

  test::WaitForNetworkSelectionScreen();
  RunNetworkSelectionScreenChecks();
  test::TapNetworkSelectionNext();

  test::WaitForEulaScreen();
  RunEulaScreenChecks();
  test::TapEulaAccept();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();
}

void OobeInteractiveUITest::PerformSessionSignInSteps(
    const ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver&
        get_auth_token_observer) {
  if (features::IsChildSpecificSigninEnabled()) {
    test::WaitForUserCreationScreen();
    test::TapUserCreationNext();
  }
  WaitForGaiaSignInScreen(test_setup()->arc_state() != ArcState::kNotAvailable);
  LogInAsRegularUser();

  test::WaitForSyncConsentScreen();
  RunSyncConsentScreenChecks();
  test::ExitScreenSyncConsent();

  if (test_setup()->is_quick_unlock_enabled()) {
    test::WaitForFingerprintScreen();
    RunFingerprintScreenChecks();
    test::ExitFingerprintPinSetupScreen();
  }

  if (test_setup()->is_tablet()) {
    test::WaitForDiscoverScreen();
    RunDiscoverScreenChecks();

    EXPECT_TRUE(get_auth_token_observer.get_auth_token_password().has_value());
    EXPECT_EQ(get_auth_token_observer.get_auth_token_password().value(),
              FakeGaiaMixin::kFakeUserPassword);

    test::ExitDiscoverPinSetupScreen();
  }

  if (test_setup()->arc_state() != ArcState::kNotAvailable) {
    HandleArcTermsOfServiceScreen();
  }

  if (test_setup()->arc_state() == ArcState::kAcceptTerms) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  HandleAssistantOptInScreen();

  if (test_setup()->is_tablet() &&
      test_setup()->hide_shelf_controls_in_tablet_mode()) {
    HandleGestureNavigationScreen();
  }

  HandleMarketingOptInScreen();
}

void OobeInteractiveUITest::SimpleEndToEnd() {
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver get_auth_token_observer;
  PerformStepsBeforeEnrollmentCheck();
  PerformSessionSignInSteps(get_auth_token_observer);

  WaitForLoginDisplayHostShutdown();
}

// Disabled on *San bots since they time out.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER)
#define MAYBE_SimpleEndToEnd DISABLED_SimpleEndToEnd
#else
#define MAYBE_SimpleEndToEnd SimpleEndToEnd
#endif

// Note that this probably the largest test that is run on ChromeOS, and it
// might be running close to time limits especially on instrumented builds.
// As such it might sometimes cause flakiness.
// Please do not disable it for whole ChromeOS, only for specific instrumented
// bots. Another alternative is to increase respective multiplier in
// base/test/test_timeouts.h.
IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, MAYBE_SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OobeInteractiveUITest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms)));

class OobeZeroTouchInteractiveUITest : public OobeInteractiveUITest {
 public:
  OobeZeroTouchInteractiveUITest() = default;
  ~OobeZeroTouchInteractiveUITest() override = default;

  void SetUpOnMainThread() override {
    OobeInteractiveUITest::SetUpOnMainThread();
    policy_server_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeInteractiveUITest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        AutoEnrollmentController::kInitialEnrollmentAlways);
  }

  void ZeroTouchEndToEnd();

 private:
  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(OobeZeroTouchInteractiveUITest);
};

void OobeZeroTouchInteractiveUITest::ZeroTouchEndToEnd() {
  policy_server_.SetupZeroTouchForcedEnrollment();

  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver get_auth_token_observer;

  PerformStepsBeforeEnrollmentCheck();

  test::WaitForEnrollmentScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  auto login_screen_waiter =
      std::make_unique<content::WindowedNotificationObserver>(
          chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
          content::NotificationService::AllSources());
  enrollment_ui_.LeaveSuccessScreen();
  login_screen_waiter->Wait();

  PerformSessionSignInSteps(get_auth_token_observer);

  WaitForLoginDisplayHostShutdown();
}

// crbug.com/997987. Disabled on MSAN since they time out.
// crbug.com/1055853: EndToEnd is flaky on Linux Chromium OS ASan LSan
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER)
#define MAYBE_EndToEnd DISABLED_EndToEnd
#else
#define MAYBE_EndToEnd EndToEnd
#endif

// Note that this probably the largest test that is run on ChromeOS, and it
// might be running close to time limits especially on instrumented builds.
// As such it might sometimes cause flakiness.
// Please do not disable it for whole ChromeOS, only for specific instrumented
// bots. Another alternative is to increase respective multiplier in
// base/test/test_timeouts.h.
IN_PROC_BROWSER_TEST_P(OobeZeroTouchInteractiveUITest, MAYBE_EndToEnd) {
  ZeroTouchEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OobeZeroTouchInteractiveUITest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms)));

class PublicSessionOobeTest : public MixinBasedInProcessBrowserTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<bool, bool, bool, ArcState>> {
 public:
  PublicSessionOobeTest()
      : PublicSessionOobeTest(false /*requires_terms_of_service*/) {}

  explicit PublicSessionOobeTest(bool requires_terms_of_service)
      : requires_terms_of_service_(requires_terms_of_service),
        observer_(std::make_unique<NativeWindowVisibilityObserver>()) {
    // Prevents Chrome from starting to quit right after login display is
    // finalized.
    login_manager_.set_should_launch_browser(true);
  }

  ~PublicSessionOobeTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();

    const std::string kAccountId = "public-session@test";

    enterprise_management::DeviceLocalAccountsProto* const
        device_local_accounts = device_policy_update->policy_payload()
                                    ->mutable_device_local_accounts();

    // Add public session account.
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);

    // Set the public session to auto-launch.
    device_local_accounts->set_auto_login_id(kAccountId);
    device_policy_update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_.RequestDeviceLocalAccountPolicyUpdate(kAccountId);
    // Specify terms of service if needed.
    if (requires_terms_of_service_) {
      device_local_account_policy_update->policy_payload()
          ->mutable_termsofserviceurl()
          ->set_value(embedded_test_server()
                          ->GetURL("/chromeos/enterprise/tos.txt")
                          .spec());
    }
    device_local_account_policy_update.reset();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    MixinBasedInProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<NativeWindowVisibilityBrowserMainExtraParts>(
            observer_.get()));
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  bool DialogWasVisible() { return observer_->was_visible(); }

  LoginManagerMixin login_manager_{&mixin_host_, {}};

 private:
  const bool requires_terms_of_service_;

  std::unique_ptr<NativeWindowVisibilityObserver> observer_;

  OobeEndToEndTestSetupMixin setup_{&mixin_host_, nullptr, GetParam()};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_P(PublicSessionOobeTest, NoTermsOfService) {
  login_manager_.WaitForActiveSession();
  // Check that the dialog was never shown.
  EXPECT_FALSE(DialogWasVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PublicSessionOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable)));

class PublicSessionWithTermsOfServiceOobeTest : public PublicSessionOobeTest {
 public:
  PublicSessionWithTermsOfServiceOobeTest()
      : PublicSessionOobeTest(true /*requires_terms_od_service*/) {}
  ~PublicSessionWithTermsOfServiceOobeTest() override = default;

  // Use embedded test server to serve Public session Terms of Service.
  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
};

IN_PROC_BROWSER_TEST_P(PublicSessionWithTermsOfServiceOobeTest,
                       AcceptTermsOfService) {
  OobeScreenWaiter(TermsOfServiceScreenView::kScreenId).Wait();

  test::OobeJS()
      .CreateWaiter(
          test::GetOobeElementPath({"terms-of-service", "acceptButton"}))
      ->Wait();
  test::OobeJS().ClickOnPath({"terms-of-service", "acceptButton"});

  OobeScreenExitWaiter(TermsOfServiceScreenView::kScreenId).Wait();

  login_manager_.WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PublicSessionWithTermsOfServiceOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable)));

class EphemeralUserOobeTest : public MixinBasedInProcessBrowserTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<bool, bool, bool, ArcState>> {
 public:
  EphemeralUserOobeTest() { login_manager_.set_should_launch_browser(true); }
  ~EphemeralUserOobeTest() override = default;

  // MixinBaseInProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);
    device_policy_update.reset();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    base::RunLoop run_loop;
    if (!LoginDisplayHost::default_host()->GetOobeUI()->IsJSReady(
            run_loop.QuitClosure())) {
      run_loop.Run();
    }

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void WaitForActiveSession() { login_manager_.WaitForActiveSession(); }

  const OobeEndToEndTestSetupMixin* test_setup() const { return &setup_; }

 private:
  EmbeddedTestServerSetupMixin gaia_server_setup_{&mixin_host_,
                                                  embedded_test_server()};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

  net::EmbeddedTestServer arc_tos_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  EmbeddedTestServerSetupMixin arc_tos_server_setup_{&mixin_host_,
                                                     &arc_tos_server_};
  OobeEndToEndTestSetupMixin setup_{&mixin_host_, &arc_tos_server_, GetParam()};

  LoginManagerMixin login_manager_{&mixin_host_, {}};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// TODO(crbug.com/1004561) Disabled due to flake.
IN_PROC_BROWSER_TEST_P(EphemeralUserOobeTest, DISABLED_RegularEphemeralUser) {
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver get_auth_token_observer;

  WaitForGaiaSignInScreen(test_setup()->arc_state() != ArcState::kNotAvailable);
  LogInAsRegularUser();

  test::WaitForSyncConsentScreen();
  RunSyncConsentScreenChecks();
  test::ExitScreenSyncConsent();

  if (test_setup()->is_quick_unlock_enabled()) {
    test::WaitForFingerprintScreen();
    RunFingerprintScreenChecks();
    test::ExitFingerprintPinSetupScreen();
  }

  if (test_setup()->is_tablet()) {
    test::WaitForDiscoverScreen();
    RunDiscoverScreenChecks();

    EXPECT_TRUE(get_auth_token_observer.get_auth_token_password().has_value());
    EXPECT_EQ("", get_auth_token_observer.get_auth_token_password().value());

    test::ExitDiscoverPinSetupScreen();
  }

  if (test_setup()->arc_state() != ArcState::kNotAvailable) {
    HandleArcTermsOfServiceScreen();
  }

  if (test_setup()->arc_state() == ArcState::kAcceptTerms) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  HandleAssistantOptInScreen();

  if (test_setup()->is_tablet() &&
      test_setup()->hide_shelf_controls_in_tablet_mode()) {
    HandleGestureNavigationScreen();
    HandleMarketingOptInScreen();
  }

  WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EphemeralUserOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms)));
}  //  namespace chromeos
