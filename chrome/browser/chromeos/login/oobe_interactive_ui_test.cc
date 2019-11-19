
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/extensions/quick_unlock_private/quick_unlock_private_api.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_delegate.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/scoped_test_recommend_apps_fetcher_factory.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
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
#include "chrome/browser/ui/webui/chromeos/login/app_downloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
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

enum class ArcState { kNotAvailable, kAcceptTerms, kDeclineTerms };

std::string ArcStateToString(ArcState arc_state) {
  switch (arc_state) {
    case ArcState::kNotAvailable:
      return "not-available";
    case ArcState::kAcceptTerms:
      return "accept-terms";
    case ArcState::kDeclineTerms:
      return "decline-terms";
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

  test::OobeJS().ExpectEQ(
      "(() => {let cnt = 0; for (let v of "
      "$('connect').$.welcomeScreen.root.querySelectorAll('video')) "
      "{  cnt += v.paused ? 0 : 1; }; return cnt; })()",
      kNumberOfVideosPlaying);
}

void RunNetworkSelectionScreenChecks() {
  test::OobeJS().ExpectTrue(
      "!$('oobe-network-md').$.networkDialog.querySelector('oobe-next-button'"
      ").disabled");
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void RunEulaScreenChecks() {
  // Wait for actual EULA to appear.
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"oobe-eula-md", "eulaDialog"})
      ->Wait();
  test::OobeJS().ExpectTrue("!$('oobe-eula-md').$.acceptButton.disabled");
}
#endif

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
                              {"arc-tos-root", "arc-tos-dialog"})
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

void RunFingerprintScreenChecks() {
  test::OobeJS().ExpectVisible("fingerprint-setup");
  test::OobeJS().ExpectVisible("fingerprint-setup-impl");
  test::OobeJS().ExpectVisiblePath(
      {"fingerprint-setup-impl", "setupFingerprint"});
  test::OobeJS().TapOnPath(
      {"fingerprint-setup-impl", "showSensorLocationButton"});
  test::OobeJS().ExpectHiddenPath(
      {"fingerprint-setup-impl", "setupFingerprint"});
  LOG(INFO) << "OobeInteractiveUITest: Waiting for fingerprint setup "
               "to switch to placeFinger.";
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"fingerprint-setup-impl", "placeFinger"})
      ->Wait();
}

void RunDiscoverScreenChecks() {
  test::OobeJS().ExpectVisible("discover");
  test::OobeJS().ExpectVisible("discover-impl");
  test::OobeJS().ExpectTrue(
      "!$('discover-impl').root.querySelector('discover-pin-setup-module')."
      "hidden");
  test::OobeJS().ExpectTrue(
      "!$('discover-impl').root.querySelector('discover-pin-setup-module').$."
      "setup.hidden");
}

// Waits for the ARC terms of service screen to be shown, it accepts or
// declines the terms based in |accept_terms| value, and waits for the flow to
// leave the ARC terms of service screen.
void HandleArcTermsOfServiceScreen(bool accept_terms) {
  OobeScreenWaiter(ArcTermsOfServiceScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'arc-tos' screen.";

  test::OobeJS()
      .CreateEnabledWaiter(true, {"arc-tos-root", "arc-tos-next-button"})
      ->Wait();
  test::OobeJS().TapOnPath({"arc-tos-root", "arc-tos-next-button"});
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"arc-tos-root", "arc-location-service"})
      ->Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, {"arc-tos-root", "arc-tos-accept-button"})
      ->Wait();

  const std::string button_to_click =
      accept_terms ? "arc-tos-accept-button" : "arc-tos-skip-button";
  test::OobeJS().TapOnPath({"arc-tos-root", button_to_click});

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

  test::OobeJS()
      .CreateDisplayedWaiter(true, {"recommend-apps-screen", "app-list-view"})
      ->Wait();

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
      test::GetOobeElementPath({"recommend-apps-screen", "app-list-view"});
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
      "recommend-apps-screen", "recommend-apps-install-button"};
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

  const std::initializer_list<base::StringPiece> continue_button = {
      "app-downloading-screen", "app-downloading-continue-setup-button"};
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
#if !BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  return;
#endif

  OobeScreenWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: Switched to 'assistant-optin' screen.";

  test::OobeJS()
      .CreateVisibilityWaiter(true, {"assistant-optin-flow-card", "loading"})
      ->Wait();

  std::initializer_list<base::StringPiece> skip_button_path = {
      "assistant-optin-flow-card", "loading", "skip-button"};
  test::OobeJS().CreateEnabledWaiter(true, skip_button_path)->Wait();
  test::OobeJS().TapOnPath(skip_button_path);

  OobeScreenExitWaiter(AssistantOptInFlowScreenView::kScreenId).Wait();
  LOG(INFO) << "OobeInteractiveUITest: 'assistant-optin' screen done.";
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
    ArcState arc_state;

    std::string ToString() const {
      return std::string("{is_tablet: ") + (is_tablet ? "true" : "false") +
             ", is_quick_unlock_enabled: " +
             (is_quick_unlock_enabled ? "true" : "false") +
             ", arc_state: " + ArcStateToString(arc_state) + "}";
    }
  };

  explicit OobeEndToEndTestSetupMixin(
      InProcessBrowserTestMixinHost* mixin_host,
      net::EmbeddedTestServer* arc_tos_server,
      const std::tuple<bool, bool, ArcState>& parameters)
      : InProcessBrowserTestMixin(mixin_host), arc_tos_server_(arc_tos_server) {
    std::tie(params_.is_tablet, params_.is_quick_unlock_enabled,
             params_.arc_state) = parameters;
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
    if (params_.is_tablet)
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    if (params_.arc_state != ArcState::kNotAvailable) {
      // Init ArcSessionManager for testing.
      arc::ArcServiceLauncher::Get()->ResetForTesting();
      arc::ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
          std::make_unique<arc::ArcSessionRunner>(
              base::BindRepeating(arc::FakeArcSession::Create)));

      if (arc_tos_server_) {
        test::OobeJS().Evaluate(base::StringPrintf(
            "login.ArcTermsOfServiceScreen.setTosHostNameForTesting('%s');",
            arc_tos_server_->GetURL("/arc-tos").spec().c_str()));
      }
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

  ArcState arc_state() const { return params_.arc_state; }

 private:
  Parameters params_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedTestRecommendAppsFetcherFactory>
      recommend_apps_fetcher_factory_;
  net::EmbeddedTestServer* arc_tos_server_;

  DISALLOW_COPY_AND_ASSIGN(OobeEndToEndTestSetupMixin);
};

}  // namespace

class OobeInteractiveUITest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, ArcState>> {
 public:
  OobeInteractiveUITest() = default;
  ~OobeInteractiveUITest() override = default;

  // OobeInteractiveUITest:
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
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};

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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  test::WaitForEulaScreen();
  RunEulaScreenChecks();
  test::TapEulaAccept();
#endif

  test::WaitForUpdateScreen();
  test::ExitUpdateScreenNoUpdate();
}

void OobeInteractiveUITest::PerformSessionSignInSteps(
    const ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver&
        get_auth_token_observer) {
  WaitForGaiaSignInScreen(test_setup()->arc_state() != ArcState::kNotAvailable);
  LogInAsRegularUser();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  test::WaitForSyncConsentScreen();
  test::ExitScreenSyncConsent();
#endif

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
    HandleArcTermsOfServiceScreen(test_setup()->arc_state() ==
                                  ArcState::kAcceptTerms);
  }

  if (test_setup()->arc_state() == ArcState::kAcceptTerms) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  HandleAssistantOptInScreen();
}

void OobeInteractiveUITest::SimpleEndToEnd() {
  ScopedQuickUnlockPrivateGetAuthTokenFunctionObserver get_auth_token_observer;
  PerformStepsBeforeEnrollmentCheck();
  PerformSessionSignInSteps(get_auth_token_observer);

  WaitForLoginDisplayHostShutdown();
}

// Timing out on debug bots. crbug.com/1004327
#if defined(NDEBUG)
#define MAYBE_SimpleEndToEnd SimpleEndToEnd
#else
#define MAYBE_SimpleEndToEnd DISABLED_SimpleEndToEnd
#endif
IN_PROC_BROWSER_TEST_P(OobeInteractiveUITest, MAYBE_SimpleEndToEnd) {
  SimpleEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(
    OobeInteractiveUITestImpl,
    OobeInteractiveUITest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms,
                                     ArcState::kDeclineTerms)));

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
  enrollment_ui_.LeaveSuccessScreen();

  PerformSessionSignInSteps(get_auth_token_observer);

  WaitForLoginDisplayHostShutdown();
}

// crbug.com/997987. Disabled in debug since they time out.crbug.com/1004327
#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)
#define MAYBE_EndToEnd DISABLED_EndToEnd
#else
#define MAYBE_EndToEnd EndToEnd
#endif

IN_PROC_BROWSER_TEST_P(OobeZeroTouchInteractiveUITest, MAYBE_EndToEnd) {
  ZeroTouchEndToEnd();
}

INSTANTIATE_TEST_SUITE_P(
    OobeZeroTouchInteractiveUITestImpl,
    OobeZeroTouchInteractiveUITest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms,
                                     ArcState::kDeclineTerms)));

class PublicSessionOobeTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, ArcState>> {
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
        new NativeWindowVisibilityBrowserMainExtraParts(observer_.get()));
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
    PublicSessionOobeTestImpl,
    PublicSessionOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kDeclineTerms)));

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
    PublicSessionWithTermsOfServiceOobeTestImpl,
    PublicSessionWithTermsOfServiceOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kDeclineTerms)));

class EphemeralUserOobeTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, ArcState>> {
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  test::WaitForSyncConsentScreen();
  test::ExitScreenSyncConsent();
#endif

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
    HandleArcTermsOfServiceScreen(test_setup()->arc_state() ==
                                  ArcState::kAcceptTerms);
  }

  if (test_setup()->arc_state() == ArcState::kAcceptTerms) {
    HandleRecommendAppsScreen();
    HandleAppDownloadingScreen();
  }

  HandleAssistantOptInScreen();

  WaitForActiveSession();
}

INSTANTIATE_TEST_SUITE_P(
    EphemeralUserOobeTestImpl,
    EphemeralUserOobeTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::Values(ArcState::kNotAvailable,
                                     ArcState::kAcceptTerms,
                                     ArcState::kDeclineTerms)));
}  //  namespace chromeos
