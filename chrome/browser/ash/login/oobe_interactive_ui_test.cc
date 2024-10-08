// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/apps/app_discovery_service/recommended_arc_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/fake_arc_tos_mixin.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/attestation/stub_attestation_features.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_switches.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

enum class ArcState { kNotAvailable, kAcceptTerms };

std::string ArcStateToString(ArcState arc_state) {
  switch (arc_state) {
    case ArcState::kNotAvailable:
      return "not-available";
    case ArcState::kAcceptTerms:
      return "accept-terms";
  }
}

void RunWelcomeScreenChecks() {
  test::OobeJS()
      .CreateFocusWaiter({"connect", "welcomeScreen", "getStarted"})
      ->Wait();

  test::OobeJS().ExpectVisiblePath({"connect", "welcomeScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "accessibilityScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "languageScreen"});
  test::OobeJS().ExpectHiddenPath({"connect", "timezoneScreen"});

  test::OobeJS().ExpectFocused({"connect", "welcomeScreen", "getStarted"});

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  EXPECT_TRUE(test::IsScanningRequestedOnNetworkScreen());
}

void RunNetworkSelectionScreenChecks() {
  test::OobeJS().ExpectEnabledPath({"network-selection", "nextButton"});

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS().CreateFocusWaiter({"network-selection", "nextButton"})->Wait();
  EXPECT_TRUE(test::IsScanningRequestedOnNetworkScreen());
}

void HandleGaiaInfoScreen() {
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'gaia-info' screen.";

  test::OobeJS().ClickOnPath({"gaia-info", "nextButton"});
  LOG(INFO) << "OobeInteractiveUITest: Exiting 'gaia-info' screen.";
}

void HandleConsumerUpdateScreen() {
  OobeScreenWaiter(ConsumerUpdateScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'consumer-update' screen.";

  test::ExitConsumerUpdateScreenNoUpdate();
}

void WaitForGaiaSignInScreen() {
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"gaia-signin", "signin-frame-dialog"})
      ->Wait();

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

void RunConsolidatedConsentScreenChecks() {
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"consolidated-consent", "loadedDialog"})
      ->Wait();

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
}

void RunSyncConsentScreenChecks() {
  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          SyncConsentScreenView::kScreenId));
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);

  const std::string button_name = "acceptButton";
  test::OobeJS().ExpectEnabledPath({"sync-consent", button_name});
  test::OobeJS().CreateFocusWaiter({"sync-consent", button_name})->Wait();

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
}

void RunFingerprintScreenChecks() {
  test::OobeJS().ExpectVisible("fingerprint-setup");
  test::OobeJS().ExpectVisiblePath({"fingerprint-setup", "setupFingerprint"});

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
}

void RunPinSetupScreenChecks() {
  test::OobeJS().ExpectVisible("pin-setup");
  test::OobeJS().ExpectVisiblePath({"pin-setup", "setup"});
  test::OobeJS().CreateFocusWaiter({"pin-setup", "pinKeyboard"})->Wait();

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());
}

// Waits for the recommend apps screen to be shown, selects the single app
// reported by FakeRecommendAppsFetcher, and requests the apps install. It
// will wait for the flow to progress away from the RecommendAppsScreen before
// returning.
void HandleRecommendAppsScreen() {
  OobeScreenWaiter(RecommendAppsScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'recommend-apps' screen.";

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"recommend-apps", "appsDialog"})
      ->Wait();

  test::OobeJS().ClickOnPath(
      {"recommend-apps", "appsList", R"(test\\.package)"});

  const std::initializer_list<std::string_view> install_button = {
      "recommend-apps", "installButton"};
  test::OobeJS().CreateEnabledWaiter(true, install_button)->Wait();
  test::OobeJS().TapOnPath(install_button);

  OobeScreenExitWaiter(RecommendAppsScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'recommend-apps' screen done.";
}

// Waits for PasswordSelectionScreen to be shown, selects 'Gaia password' option
// and clicks next to go to the next screen.
void HandlePasswordSelectionScreen() {
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  LOG(INFO)
      << "OobeInteractiveUITest: Switched to 'password-selection' screen.";

  test::OobeJS().CreateVisibilityWaiter(true, {"password-selection"})->Wait();

  test::OobeJS().ClickOnPath({"password-selection", "gaiaPasswordButton"});

  test::OobeJS().ExpectVisiblePath({"password-selection", "nextButton"});
  test::OobeJS().ExecuteAsync("$('password-selection').$.nextButton.click()");

  OobeScreenExitWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'password-selection' screen done.";
}

// Waits for AppDownloadingScreen to be shown, clicks 'Continue' button, and
// waits until the flow moves to the next screen. This assumes that an app was
// selected in HandleRecommendAppsScreen.
void HandleAppDownloadingScreen() {
  OobeScreenWaiter(AppDownloadingScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'app-downloading' screen.";

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  const std::initializer_list<std::string_view> continue_button = {
      "app-downloading", "continue-setup-button"};
  test::OobeJS().TapOnPath(continue_button);

  OobeScreenExitWaiter(AppDownloadingScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'app-downloading' screen done.";
}

// Waits for AiIntroScreen to be shown and clicks next to go to the next screen.
void HandleAiIntroScreen() {
  OobeScreenWaiter(AiIntroScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'ai-intro' screen.";

  test::OobeJS().TapOnPathAsync({"ai-intro", "nextButton"});

  OobeScreenExitWaiter(AiIntroScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'ai-intro' screen done.";
}

// Waits for GeminiIntroScreen to be shown and clicks next to go to the next
// screen.
void HandleGeminiIntroScreen() {
  OobeScreenWaiter(GeminiIntroScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'gemini-intro' screen.";

  test::OobeJS().TapOnPathAsync({"gemini-intro", "nextButton"});

  OobeScreenExitWaiter(GeminiIntroScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'gemini-intro' screen done.";
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

  EXPECT_FALSE(LoginScreenTestApi::IsShutdownButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());
  EXPECT_FALSE(LoginScreenTestApi::IsAddUserButtonShown());

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"assistant-optin-flow", "card", "loading"})
      ->Wait();

  std::initializer_list<std::string_view> skip_button_path = {
      "assistant-optin-flow", "card", "loading", "skip-button"};
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

// Waits for theme selection to get shown, then taps through the screen
// and waits for the screen to exit.
void HandleThemeSelectionScreen() {
  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();

  // Enable the tablet mode shelf navigation buttons so that the next button
  // is shown on the marketing opt in screen in tablet mode.
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);

  test::OobeJS().CreateVisibilityWaiter(true, {"theme-selection"})->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"theme-selection", "nextButton"})
      ->Wait();
  test::OobeJS().TapOnPath({"theme-selection", "nextButton"});

  OobeScreenExitWaiter(ThemeSelectionScreenView::kScreenId).Wait();
}

// Waits for display size screen to get shown, then taps through the screen
// and waits for the screen to exit.
void HandleDisplaySizeScreen() {
  OobeScreenWaiter(DisplaySizeScreenView::kScreenId).Wait();

  test::OobeJS().ClickOnPath({"display-size", "nextButton"});

  OobeScreenExitWaiter(DisplaySizeScreenView::kScreenId).Wait();
}

// Waits for touchpad scroll screen to get shown, then taps through the screen
// and waits for the screen to exit.
void HandleTouchpadScrollScreen() {
  OobeScreenWaiter(TouchpadScrollScreenView::kScreenId).Wait();

  test::OobeJS().ClickOnPath({"touchpad-scroll", "nextButton"});

  OobeScreenExitWaiter(TouchpadScrollScreenView::kScreenId).Wait();
}

// Waits for CHOOBE screen to get shown, selects all screens cards, then taps
// through the screen and waits for the screen to exit.
void HandleChoobeScreen() {
  OobeScreenWaiter(ChoobeScreenView::kScreenId).Wait();

  const test::UIPath screens_cards[] = {
      {"choobe", "screensList", "cr-button-touchpad-scroll"},
      {"choobe", "screensList", "cr-button-display-size"},
      {"choobe", "screensList", "cr-button-theme-selection"}};
  for (const auto& card : screens_cards) {
    test::OobeJS().TapOnPath(card);
  }
  test::OobeJS().TapOnPath({"choobe", "nextButton"});

  OobeScreenExitWaiter(ChoobeScreenView::kScreenId).Wait();
}

// Taps through CHOOBE screen (if it should be shown), then calls the handle
// methods for the optional sreens.
void HandleChoobeFlow() {
  // CHOOBE screen will only be enabled when there are at least 3 eligible
  // optional screens. So, for the screen to be shown, both `OobeDisplaySize`
  // and `OobeTouchpadScroll` must be enabled to have at least 3 optional
  // screens.
  bool should_show_choobe = features::IsOobeDisplaySizeEnabled() &&
                            features::IsOobeTouchpadScrollEnabled();

  if (should_show_choobe) {
    HandleChoobeScreen();
  }

  if (features::IsOobeTouchpadScrollEnabled()) {
    HandleTouchpadScrollScreen();
  }

  if (features::IsOobeDisplaySizeEnabled()) {
    HandleDisplaySizeScreen();
  }

  HandleThemeSelectionScreen();
}

// Waits for marketing opt in screen to get shown, then taps through the screen
// and waits for the screen to exit.
void HandleMarketingOptInScreen() {
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();

  // Enable the tablet mode shelf navigation buttons so that the next button
  // is shown on the marketing opt in screen in tablet mode.
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, true);

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

class FakeRecommendAppsFetcher : public apps::RecommendAppsFetcher {
 public:
  explicit FakeRecommendAppsFetcher(
      apps::RecommendAppsFetcherDelegate* delegate)
      : delegate_(delegate) {}
  ~FakeRecommendAppsFetcher() override = default;

  // RecommendAppsFetcher:
  void Start() override {
    base::Value::Dict app;
    app.Set("packageName", "test.package");
    app.Set("title", "TestName");
    base::Value::Dict big_app;
    big_app.Set("androidApp", std::move(app));
    base::Value::List app_list;
    app_list.Append(std::move(big_app));
    base::Value::Dict response_dict;
    response_dict.Set("recommendedApp", std::move(app_list));
    delegate_->OnLoadSuccess(base::Value(std::move(response_dict)));
  }

  void Retry() override { NOTREACHED_IN_MIGRATION(); }

 private:
  const raw_ptr<apps::RecommendAppsFetcherDelegate> delegate_;
};

std::unique_ptr<apps::RecommendAppsFetcher> CreateRecommendAppsFetcher(
    apps::RecommendAppsFetcherDelegate* delegate) {
  return std::make_unique<FakeRecommendAppsFetcher>(delegate);
}

// Observes an `aura::Window` to see if the window was visible at some point in
// time.
class NativeWindowVisibilityObserver : public aura::WindowObserver {
 public:
  NativeWindowVisibilityObserver() = default;

  NativeWindowVisibilityObserver(const NativeWindowVisibilityObserver&) =
      delete;
  NativeWindowVisibilityObserver& operator=(
      const NativeWindowVisibilityObserver&) = delete;

  // aura::Window will remove observers on destruction.
  ~NativeWindowVisibilityObserver() override = default;

  void Observe(aura::Window* window) {
    window_ = window;
    window_->AddObserver(this);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible) {
      was_visible_ = visible;
    }
  }

  bool was_visible() { return was_visible_; }

 private:
  // The window was visible at some point in time.
  bool was_visible_ = false;

  raw_ptr<aura::Window> window_;
};

// Sets the `NativeWindowVisibilityObserver` to observe the
// `LoginDisplayHost`'s `NativeWindow`. This needs to be done in
// `PostProfileInit()` as the `default_host` will not be initialized before
// this.
class NativeWindowVisibilityBrowserMainExtraParts
    : public ChromeBrowserMainExtraParts {
 public:
  explicit NativeWindowVisibilityBrowserMainExtraParts(
      NativeWindowVisibilityObserver* observer)
      : observer_(observer) {}

  NativeWindowVisibilityBrowserMainExtraParts(
      const NativeWindowVisibilityBrowserMainExtraParts&) = delete;
  NativeWindowVisibilityBrowserMainExtraParts& operator=(
      const NativeWindowVisibilityBrowserMainExtraParts&) = delete;

  ~NativeWindowVisibilityBrowserMainExtraParts() override = default;

  // ChromeBrowserMainExtraParts:
  void PostProfileInit(Profile* profile, bool is_initial_profile) override {
    // The setup below is intended to run for only the initial profile.
    if (!is_initial_profile) {
      return;
    }

    gfx::NativeWindow window =
        LoginDisplayHost::default_host()->GetNativeWindow();
    if (window) {
      observer_->Observe(window);
    }
  }

 private:
  raw_ptr<NativeWindowVisibilityObserver, DanglingUntriaged> observer_;
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
      const std::tuple<bool, bool, bool, ArcState>& parameters)
      : InProcessBrowserTestMixin(mixin_host) {
    std::tie(params_.is_tablet, params_.is_quick_unlock_enabled,
             params_.hide_shelf_controls_in_tablet_mode, params_.arc_state) =
        parameters;
    std::vector<base::test::FeatureRef> enabled_features = {
        ash::features::kFeatureManagementOobeAiIntro,
        ash::features::kFeatureManagementOobeGeminiIntro,
    };
    std::vector<base::test::FeatureRef> disabled_features;
    if (params_.hide_shelf_controls_in_tablet_mode) {
      enabled_features.push_back(features::kHideShelfControlsInTabletMode);
    } else {
      disabled_features.push_back(features::kHideShelfControlsInTabletMode);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  OobeEndToEndTestSetupMixin(const OobeEndToEndTestSetupMixin&) = delete;
  OobeEndToEndTestSetupMixin& operator=(const OobeEndToEndTestSetupMixin&) =
      delete;

  ~OobeEndToEndTestSetupMixin() override = default;

  // InProcessBrowserTestMixin:
  void SetUp() override {
    LOG(INFO) << "OOBE end-to-end test  started with params "
              << params_.ToString();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (params_.is_tablet) {
      // Makes the device capable to entering tablet mode.
      command_line->AppendSwitch(switches::kAshEnableTabletMode);
      // Having an active internal display so that tablet mode does not end
      // on display config change.
      command_line->AppendSwitch(::switches::kUseFirstDisplayAsInternal);
    }

    if (params_.arc_state != ArcState::kNotAvailable) {
      arc::SetArcAvailableCommandLineForTesting(command_line);
    }

    // This will change the verification key to be used by the
    // CloudPolicyValidator. It will allow for the policy provided by the
    // PolicyBuilder to pass the signature validation.
    command_line->AppendSwitchASCII(
        policy::switches::kPolicyVerificationKey,
        policy::PolicyBuilder::GetEncodedPolicyVerificationKey());
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (params_.is_quick_unlock_enabled) {
      test_api_ = std::make_unique<quick_unlock::TestApi>(
          /*override_quick_unlock=*/true);
      test_api_->EnableFingerprintByPolicy(quick_unlock::Purpose::kAny);
      test_api_->EnablePinByPolicy(quick_unlock::Purpose::kAny);
    }

    if (params_.arc_state != ArcState::kNotAvailable) {
      recommend_apps_fetcher_factory_ =
          std::make_unique<apps::ScopedTestRecommendAppsFetcherFactory>(
              base::BindRepeating(&CreateRecommendAppsFetcher));
    }
  }

  void SetUpOnMainThread() override {
    if (params_.is_tablet) {
      ShellTestApi().SetTabletModeEnabledForTest(true);
    }

    if (params_.arc_state != ArcState::kNotAvailable) {
      // Init ArcSessionManager for testing.
      arc::ArcServiceLauncher::Get()->ResetForTesting();
      arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
          std::make_unique<arc::ArcSessionRunner>(
              base::BindRepeating(arc::FakeArcSession::Create)));
      arc::ExpandPropertyFilesForTesting(arc::ArcSessionManager::Get());
    }
  }
  void TearDownInProcessBrowserTestFixture() override {
    recommend_apps_fetcher_factory_.reset();
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
  std::unique_ptr<apps::ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;
  std::unique_ptr<quick_unlock::TestApi> test_api_;
};

}  // namespace

class OobeInteractiveUITest : public OobeBaseTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<bool, bool, bool, ArcState>> {
 public:
  OobeInteractiveUITest(const OobeInteractiveUITest&) = delete;
  OobeInteractiveUITest& operator=(const OobeInteractiveUITest&) = delete;

  OobeInteractiveUITest() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  ~OobeInteractiveUITest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
    OobeBaseTest::TearDownOnMainThread();
  }

  void WaitForLoginDisplayHostShutdown() {
    if (!LoginDisplayHost::default_host()) {
      return;
    }

    LOG(INFO) << "OobeInteractiveUITest: Waiting for LoginDisplayHost to "
                 "shut down.";
    test::TestPredicateWaiter(base::BindRepeating([]() {
      return !LoginDisplayHost::default_host();
    })).Wait();
    LOG(INFO) << "OobeInteractiveUITest: LoginDisplayHost is down.";
  }

  void PerformStepsBeforeEnrollmentCheck();
  void PerformSessionSignInSteps();

  void SimpleEndToEnd();

  const OobeEndToEndTestSetupMixin* test_setup() const { return &setup_; }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  void ForceBrandedBuild() const;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
  FakeArcTosMixin fake_arc_tos_{&mixin_host_, embedded_test_server()};

  OobeEndToEndTestSetupMixin setup_{&mixin_host_, GetParam()};
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

void OobeInteractiveUITest::ForceBrandedBuild() const {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
}

void OobeInteractiveUITest::PerformStepsBeforeEnrollmentCheck() {
  histogram_tester()->ExpectUniqueSample("OOBE.OobeFlowStatus", 0 /*Started*/,
                                         1);
  ForceBrandedBuild();
  test::WaitForWelcomeScreen();
  RunWelcomeScreenChecks();
  test::TapWelcomeNext();

  test::WaitForNetworkSelectionScreen();
  RunNetworkSelectionScreenChecks();
  test::TapNetworkSelectionNext();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();
}

void OobeInteractiveUITest::PerformSessionSignInSteps() {
  ForceBrandedBuild();
  if (GetFirstSigninScreen() == UserCreationView::kScreenId) {
    test::WaitForUserCreationScreen();

    if (features::IsOobeSoftwareUpdateEnabled()) {
      test::TapForPersonalUseCrRadioButton();
      test::TapUserCreationNext();
      HandleConsumerUpdateScreen();
    } else {
      test::TapUserCreationNext();
    }

    if (features::IsOobeGaiaInfoScreenEnabled()) {
      HandleGaiaInfoScreen();
    }
  }

  WaitForGaiaSignInScreen();
  LogInAsRegularUser();

  test::WaitForConsolidatedConsentScreen();
  histogram_tester()->ExpectUniqueSample(
      "OOBE.OnboardingFlowStatus.FirstOnboarding", 0 /*Started*/, 1);
  histogram_tester()->ExpectTotalCount("OOBE.OobeStartToOnboardingStartTime",
                                       1);

  RunConsolidatedConsentScreenChecks();
  test::TapConsolidatedConsentAccept();

  test::WaitForSyncConsentScreen();
  RunSyncConsentScreenChecks();
  test::ExitScreenSyncConsent();

  HandlePasswordSelectionScreen();

  if (test_setup()->is_quick_unlock_enabled()) {
    test::WaitForFingerprintScreen();
    RunFingerprintScreenChecks();
    test::ExitFingerprintPinSetupScreen();
  }

  if (test_setup()->is_tablet()) {
    test::WaitForPinSetupScreen();
    RunPinSetupScreenChecks();
    test::ExitPinSetupScreen();
  }

  if (test_setup()->arc_state() != ArcState::kNotAvailable) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  if (ash::features::IsOobeAiIntroEnabled()) {
    HandleAiIntroScreen();
  }

  if (ash::features::IsOobeGeminiIntroEnabled()) {
    HandleGeminiIntroScreen();
  }

  if (!features::IsOobeSkipAssistantEnabled()) {
    HandleAssistantOptInScreen();
  }

  if (test_setup()->is_tablet() &&
      test_setup()->hide_shelf_controls_in_tablet_mode()) {
    HandleGestureNavigationScreen();
  }

  if (features::IsOobeChoobeEnabled()) {
    HandleChoobeFlow();
  } else {
    HandleThemeSelectionScreen();
  }

  HandleMarketingOptInScreen();
  histogram_tester()->ExpectBucketCount("OOBE.OobeFlowStatus", 1 /*Completed*/,
                                        1);
  histogram_tester()->ExpectBucketCount(
      "OOBE.OnboardingFlowStatus.FirstOnboarding", 1 /*Completed*/, 1);
  histogram_tester()->ExpectTotalCount("OOBE.OobeFlowDuration", 1);
  histogram_tester()->ExpectTotalCount(
      "OOBE.OnboardingFlowDuration.FirstOnboarding", 1);
}

void OobeInteractiveUITest::SimpleEndToEnd() {
  test::SetFakeTouchpadDevice();
  PerformStepsBeforeEnrollmentCheck();
  PerformSessionSignInSteps();

  WaitForLoginDisplayHostShutdown();
}

// Disabled on *San bots since they time out.
// crbug.com/1260131: SimpleEndToEnd is flaky on builder "linux-chromeos-dbg"
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER) || !defined(NDEBUG)
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

  OobeZeroTouchInteractiveUITest(const OobeZeroTouchInteractiveUITest&) =
      delete;
  OobeZeroTouchInteractiveUITest& operator=(
      const OobeZeroTouchInteractiveUITest&) = delete;

  ~OobeZeroTouchInteractiveUITest() override = default;

  void SetUpOnMainThread() override {
    AttestationClient::Get()
        ->GetTestInterface()
        ->AllowlistSignSimpleChallengeKey(
            /*username=*/"", attestation::kEnterpriseEnrollmentKey);
    OobeInteractiveUITest::SetUpOnMainThread();
    policy_test_server_mixin_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeInteractiveUITest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        policy::AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);
    // TODO(b/353731379): Remove when removing legacy state determination code.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  }

  void ZeroTouchEndToEnd();

 private:
  EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  attestation::ScopedStubAttestationFeatures attestation_features_;
};

void OobeZeroTouchInteractiveUITest::ZeroTouchEndToEnd() {
  test::SetFakeTouchpadDevice();
  policy_test_server_mixin_.SetupZeroTouchForcedEnrollment();

  WizardController::default_controller()
      ->GetAutoEnrollmentControllerForTesting()
      ->SetRlweClientFactoryForTesting(
          policy::psm::testing::CreateClientFactory());

  PerformStepsBeforeEnrollmentCheck();

  test::WaitForEnrollmentScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  auto login_screen_waiter = std::make_unique<LoginOrLockScreenVisibleWaiter>();
  enrollment_ui_.LeaveSuccessScreen();
  login_screen_waiter->WaitEvenIfShown();

  PerformSessionSignInSteps();

  WaitForLoginDisplayHostShutdown();
}

// crbug.com/997987. Disabled on MSAN since they time out.
// crbug.com/1055853: EndToEnd is flaky on Linux Chromium OS ASan LSan
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER) || !defined(NDEBUG)
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
    login_manager_.SetShouldLaunchBrowser(true);
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

  OobeEndToEndTestSetupMixin setup_{&mixin_host_, GetParam()};
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

class EphemeralUserOobeTest : public OobeBaseTest,
                              public ::testing::WithParamInterface<
                                  std::tuple<bool, bool, bool, ArcState>> {
 public:
  EphemeralUserOobeTest() { login_manager_.SetShouldLaunchBrowser(true); }
  ~EphemeralUserOobeTest() override = default;

  // OobeBaseTest:
  void SetUpInProcessBrowserTestFixture() override {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);
    device_policy_update.reset();

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
  }

  void WaitForActiveSession() { login_manager_.WaitForActiveSession(); }

  const OobeEndToEndTestSetupMixin* test_setup() const { return &setup_; }

 private:
  // Fake GAIA setup.
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  LoginManagerMixin login_manager_{&mixin_host_, {}};

  // Fake Arc server and EULA server.
  FakeArcTosMixin fake_arc_tos_{&mixin_host_, embedded_test_server()};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
  OobeEndToEndTestSetupMixin setup_{&mixin_host_, GetParam()};

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// In this test we login as a regular user, which means it is not affilated
// with the domain of the device. Thus we still need a consent from user.
IN_PROC_BROWSER_TEST_P(EphemeralUserOobeTest, RegularEphemeralUser) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  WaitForGaiaSignInScreen();
  LogInAsRegularUser();

  test::WaitForConsolidatedConsentScreen();
  RunConsolidatedConsentScreenChecks();
  test::TapConsolidatedConsentAccept();

  test::WaitForSyncConsentScreen();
  RunSyncConsentScreenChecks();
  test::ExitScreenSyncConsent();

  if (test_setup()->arc_state() != ArcState::kNotAvailable) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  if (ash::features::IsOobeAiIntroEnabled()) {
    HandleAiIntroScreen();
  }

  if (ash::features::IsOobeGeminiIntroEnabled()) {
    HandleGeminiIntroScreen();
  }

  HandleThemeSelectionScreen();
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

class OobeFlexInteractiveUITest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<::tpm_manager::TpmManagerStatus> {
 public:
  OobeFlexInteractiveUITest() = default;
  OobeFlexInteractiveUITest(const OobeFlexInteractiveUITest&) = delete;
  OobeFlexInteractiveUITest& operator=(const OobeFlexInteractiveUITest&) =
      delete;

  ~OobeFlexInteractiveUITest() override = default;

  // EnrollmentScreenTest:
  void SetUpOnMainThread() override {
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    original_tpm_check_callback_ =
        enrollment_screen->get_tpm_ownership_callback_for_testing();
    enrollment_screen->set_tpm_ownership_callback_for_testing(base::BindOnce(
        &OobeFlexInteractiveUITest::HandleTakeTPMOwnershipResponse,
        base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kTpmIsDynamic);
    OobeBaseTest::SetUpCommandLine(command_line);

    // This will change the verification key to be used by the
    // CloudPolicyValidator. It will allow for the policy provided by the
    // PolicyBuilder to pass the signature validation.
    command_line->AppendSwitchASCII(
        policy::switches::kPolicyVerificationKey,
        policy::PolicyBuilder::GetEncodedPolicyVerificationKey());
  }

  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};

 private:
  void HandleTakeTPMOwnershipResponse(
      const ::tpm_manager::TakeOwnershipReply& reply) {
    EXPECT_FALSE(tpm_reply_.has_value());
    tpm_reply_ = reply;
    // Here we substitute fake reply with status that we want to test.
    tpm_reply_.value().set_status(GetParam());

    if (original_tpm_check_callback_) {
      std::move(original_tpm_check_callback_).Run(tpm_reply_.value());
    }
  }

  EnrollmentScreen::TpmStatusCallback original_tpm_check_callback_;
  std::optional<::tpm_manager::TakeOwnershipReply> tpm_reply_;
};

// Verify that ChromeOS Flex behaves as expected on devices with different TPM
// configurations.
IN_PROC_BROWSER_TEST_P(OobeFlexInteractiveUITest, SmokeEnroll) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  test::WaitForWelcomeScreen();
  RunWelcomeScreenChecks();
  test::TapWelcomeNext();

  test::WaitForNetworkSelectionScreen();
  RunNetworkSelectionScreenChecks();
  test::TapNetworkSelectionNext();

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();

  LoginDisplayHost* host = LoginDisplayHost::default_host();
  host->HandleAccelerator(LoginAcceleratorAction::kStartEnrollment);

  test::WaitForEnrollmentScreen();
  switch (GetParam()) {
    case ::tpm_manager::STATUS_SUCCESS:
    case ::tpm_manager::STATUS_NOT_AVAILABLE:
      enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);
      return;
    case ::tpm_manager::STATUS_DBUS_ERROR: {
      OobeScreenExitWaiter(TpmErrorView::kScreenId).Wait();
      test::OobeJS().ExpectVisiblePath({"tpm-error-message", "restartButton"});
      ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
          {"tpm-error-message", "restartButton"});

      EXPECT_EQ(
          chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(),
          1);
      return;
    }

    case ::tpm_manager::STATUS_DEVICE_ERROR: {
      OobeScreenExitWaiter(TpmErrorView::kScreenId).Wait();
      test::OobeJS().ExpectVisiblePath({"tpm-error-message", "restartButton"});
      ash::test::TapOnPathAndWaitForOobeToBeDestroyed(
          {"tpm-error-message", "restartButton"});
      EXPECT_EQ(
          chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(),
          1);
      return;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         OobeFlexInteractiveUITest,
                         ::testing::Values(::tpm_manager::STATUS_SUCCESS,
                                           ::tpm_manager::STATUS_DEVICE_ERROR,
                                           ::tpm_manager::STATUS_NOT_AVAILABLE,
                                           ::tpm_manager::STATUS_DBUS_ERROR));

}  //  namespace ash
