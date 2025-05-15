// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_

#include <map>
#include <sstream>
#include <string_view>

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/test/event_generator.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace glic::test {

extern const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton;
extern const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel;

// Mixin class that adds a mock glic to the current browser.
// If all you need is the combination of this + interactive browser test, use
// `InteractiveGlicTest` (defined below) instead.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest> &&
           std::derived_from<T, InteractiveBrowserTestApi>)
class InteractiveGlicTestT : public T {
 public:
  // Determines whether this is an attached or detached Glic window.
  enum GlicWindowMode {
    kAttached,
    kDetached,
  };

  // What portions of the glic window should be instrumented on open.
  enum GlicInstrumentMode {
    // Instruments the host as `kGlicHostElementId` and contents as
    // `kGlicContentsElementId`.
    kHostAndContents,
    // Instruments only the host as `kGlicHostElementId`.
    kHostOnly,
    // Does not instrument either.
    kNone
  };

  // Constructor that takes `FieldTrialParams` for the glic flag and then
  // forwards the rest of the args.
  template <typename... Args>
  explicit InteractiveGlicTestT(const base::FieldTrialParams& glic_params,
                                Args&&... args)
      : T(std::forward<Args>(args)...) {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlic, glic_params},
         {features::kTabstripComboButton, {}},
         {features::kGlicRollout, {}},
         {features::kGlicKeyboardShortcutNewBadge, {}}},
        {});
  }

  // Default constructor (no forwarded args or field trial parameters).
  InteractiveGlicTestT() : InteractiveGlicTestT(base::FieldTrialParams()) {}

  // Constructor with no field trial params; all arguments are forwarded to the
  // base class.
  template <typename Arg, typename... Args>
    requires(!std::same_as<base::FieldTrialParams, std::remove_cvref_t<Arg>>)
  explicit InteractiveGlicTestT(Arg&& arg, Args&&... args)
      : InteractiveGlicTestT(base::FieldTrialParams(),
                             std::forward<Arg>(arg),
                             std::forward<Args>(args)...) {}

  ~InteractiveGlicTestT() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    T::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();

    Test::embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    Test::embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/webui/glic/");

    ASSERT_TRUE(Test::embedded_test_server()->Start());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    std::ostringstream path;
    path << glic_page_path_;

    // Append the query parameters to the URL.
    bool first_param = true;
    auto encode = [](const std::string_view& value) {
      url::RawCanonOutputT<char> encoded;
      url::EncodeURIComponent(value, &encoded);
      return std::string(encoded.view());
    };
    for (const auto& [key, value] : mock_glic_query_params_) {
      path << (first_param ? "?" : "&");
      first_param = false;
      path << encode(key);
      if (!value.empty()) {
        path << "=" << encode(value);
      }
    }

    auto* command_line = base::CommandLine::ForCurrentProcess();
    guest_url_ = Test::embedded_test_server()->GetURL(path.str());
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL,
                                    guest_url_.spec());

    Browser* browser = InProcessBrowserTest::browser();

    // Individual test could disable the glic feature.
    if (GlicEnabling::IsProfileEligible(browser->profile())) {
      glic_test_environment_ =
          std::make_unique<glic::GlicTestEnvironment>(browser->profile());
    }
  }

  void TearDownOnMainThread() override {
    T::TearDownOnMainThread();
    glic_test_environment_.reset();
  }

  void SetGlicPagePath(const std::string& glic_page_path) {
    glic_page_path_ = glic_page_path;
  }

  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode) {
    return WaitForAndInstrumentGlic(instrument_mode, window_controller());
  }

  // Ensures that the WebContents for some combination of glic host and contents
  // are instrumented, per `instrument_mode`. Takes a window controller, to
  // permit instrumenting for a different profile.
  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode,
                                GlicWindowController& window_controller) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;

    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller)),
            Api::InAnyContext(Api::Steps(
                Api::InstrumentNonTabWebView(kGlicHostElementId,
                                             kGlicViewElementId),
                Api::InstrumentInnerWebContents(kGlicContentsElementId,
                                                kGlicHostElementId, 0),
                Api::WaitForWebContentsReady(kGlicContentsElementId))),
            Api::WaitForState(internal::kGlicWindowControllerState,
                              GlicWindowController::State::kOpen),
            Api::StopObservingState(internal::kGlicWindowControllerState)
            /*, WaitForElementVisible(kPathToGuestPanel)*/);
        break;
      case GlicInstrumentMode::kHostOnly:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller)),
            Api::InAnyContext(Api::InstrumentNonTabWebView(kGlicHostElementId,
                                                           kGlicViewElementId)),
            Api::WaitForState(
                internal::kGlicWindowControllerState,
                testing::Matcher<GlicWindowController::State>(testing::AnyOf(
                    GlicWindowController::State::kWaitingForGlicToLoad,
                    GlicWindowController::State::kOpen))),
            Api::StopObservingState(internal::kGlicWindowControllerState));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
    }

    Api::AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
    return steps;
  }
  // Activate one of the glic entrypoints.
  // If `instrument_glic_contents` is true both the host and contents will be
  // instrumented (see `WaitForAndInstrumentGlic()`) else only the host will be
  // instrumented (`WaitForAndInstrumentGlicHostOnly()`).
  auto OpenGlicWindow(GlicWindowMode window_mode,
                      GlicInstrumentMode instrument_mode =
                          GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::Steps(
        EnsureGlicWindowState("window must be closed in order to open it",
                              GlicWindowController::State::kClosed),
        // Technically, this toggles the window, but we've already ensured that
        // it's closed.
        ToggleGlicWindow(window_mode),
        WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // Toggles Glic through one of the entrypoints.
  // Does not wait for Glic to open or close, tests using this should check for
  // the correct window state after toggling.
  auto ToggleGlicWindow(GlicWindowMode window_mode) {
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(kGlicButtonElementId);
      case GlicWindowMode::kDetached:
        return Api::Do(
            [this] { window_controller().ShowDetachedForTesting(); });
    }
  }

  // Ensures a mock glic button is present and then clicks it. Works even if the
  // element is off-screen.
  auto ClickMockGlicElement(
      const WebContentsInteractionTestUtil::DeepQuery& where,
      const bool click_closes_window = false) {
    auto steps = Api::Steps(
        // Note: Elements on the test client don't need to be in the viewport to
        // be used. Ideally we would wait until the element is visible, but not
        // necessarily on screen. Because we don't have any elements that get
        // hidden on the test client, waiting for body visibility is good
        // enough.
        Api::WaitForElementVisible(kGlicContentsElementId, {"body"}),
        // TODO(dfried): Figure out why Api::CheckJsResultAt() here doesn't
        // work. Error:
        // Interactive test failed on step 28 (ClickMockGlicElement:
        // CheckJsResultAt( {"#contextAccessIndicator"}, " ... with reason
        // kSequenceDestroyed; step type kShown; id ElementIdentifier
        // kGlicContentsElementId.
        Api::ExecuteJsAt(
            kGlicContentsElementId, where, "(el)=>el.click()",
            click_closes_window
                ? InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget
                : InteractiveBrowserTestApi::ExecuteJsMode::
                      kWaitForCompletion));

    Api::AddDescriptionPrefix(steps, "ClickMockGlicElement");
    return steps;
  }

  // Closes the glic window, which must be open.
  //
  // TODO: this only works if glic is actually loaded; handle the case where the
  // contents pane has either not loaded or failed to load.
  auto CloseGlicWindow() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::InAnyContext(Api::Steps(
        EnsureGlicWindowState("cannot close window if it is not open",
                              GlicWindowController::State::kOpen),
        ClickMockGlicElement(kPathToMockGlicCloseButton),
        Api::WaitForHide(kGlicViewElementId)));
    Api::AddDescriptionPrefix(steps, "CloseGlicWindow");
    return steps;
  }

  auto SimulateAcceleratorPress(const ui::Accelerator& accelerator) {
    return Api::Do([this, accelerator] {
      gfx::NativeWindow target_window =
          window_controller().GetGlicWidget()->GetNativeWindow();
#if (USE_AURA)
      ui::test::EventGenerator event_generator(target_window->GetRootWindow(),
                                               target_window);
#else
      ui::test::EventGenerator event_generator(target_window);
#endif
      event_generator.set_target(ui::test::EventGenerator::Target::WINDOW);
      event_generator.PressAndReleaseKeyAndModifierKeys(
          accelerator.key_code(), accelerator.modifiers());
    });
  }

  auto CheckControllerHasWidget(bool expect_widget) {
    return Api::CheckResult(
        [this]() { return window_controller().GetGlicWidget() != nullptr; },
        expect_widget, "CheckControllerHasWidget");
  }

  auto CheckControllerShowing(bool expect_showing) {
    return Api::CheckResult(
        [this]() { return window_controller().IsShowing(); }, expect_showing,
        "CheckControllerShowing");
  }

  auto CheckControllerWidgetMode(GlicWindowMode mode) {
    return Api::CheckResult(
        [this]() {
          return window_controller().IsAttached() ? GlicWindowMode::kAttached
                                                  : GlicWindowMode::kDetached;
        },
        mode, "CheckControllerWidgetMode");
  }

  auto CheckPointIsWithinDraggableArea(const gfx::Point& point,
                                       bool expect_within_area) {
    return Api::CheckResult(
        [this, point]() {
          return window_controller().GetGlicView()->IsPointWithinDraggableArea(
              point);
        },
        expect_within_area,
        "CheckPointIsWithinDraggableArea_" + point.ToString());
  }

  auto CheckIfAttachedToBrowser(Browser* new_browser) {
    return Api::CheckResult(
        [this] { return window_controller().attached_browser(); }, new_browser,
        "attached to the other browser");
  }

  auto CheckWidgetMinimumSize(const gfx::Size& size) {
    // Size can't be smaller than the initial size.
    auto expected_size = glic::GlicWidget::GetInitialSize();
    expected_size.SetToMax(size);
    return Api::CheckResult(
        [this]() {
          return window_controller().GetGlicWidget()->GetMinimumSize();
        },
        expected_size, "CheckWidgetMinimumSize");
  }

  auto ExpectUserCanResize(bool expect_resize) {
    return Api::CheckResult(
        [this]() {
          return window_controller()
              .GetGlicWidget()
              ->widget_delegate()
              ->CanResize();
        },
        expect_resize, "ExpectUserCanResize");
  }

  auto CheckTabCount(int expected_count) {
    return Api::CheckResult(
        [this] {
          return InProcessBrowserTest::browser()
              ->tab_strip_model()
              ->GetTabCount();
        },
        expected_count, "CheckTabCount");
  }

  auto Wait(base::TimeDelta timeout) {
    auto observer = std::make_unique<internal::WaitingStateObserver>();
    auto observer_ptr = observer.get();
    return Api::Steps(
        Api::Do(base::BindRepeating(
            [](internal::WaitingStateObserver* observer,
               base::TimeDelta timeout) { observer->Start(timeout); },
            base::Unretained(observer_ptr), timeout)),
        Api::ObserveState(glic::test::internal::kDelayState,
                          std::move(observer)),
        Api::WaitForState(glic::test::internal::kDelayState, true));
  }

  content::RenderFrameHost* FindGlicGuestMainFrame() {
    for (GlicPageHandler* handler : host().GetPageHandlersForTesting()) {
      if (handler->GetGuestMainFrame()) {
        return handler->GetGuestMainFrame();
      }
    }
    return nullptr;
  }

  glic::GlicTestEnvironment& glic_test_environment() {
    return *glic_test_environment_;
  }

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        InProcessBrowserTest::browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  Host& host() { return glic_service()->host(); }

  template <typename... M>
  auto EnsureGlicWindowState(const std::string& desc, M&&... matchers) {
    return Api::CheckResult([this]() { return window_controller().state(); },
                            testing::Matcher<GlicWindowController::State>(
                                testing::AnyOf(std::forward<M>(matchers)...)),
                            desc);
  }

  // Adds a query param to the URL that will be used to load the mock glic.
  // Must be called before `SetUpOnMainThread()`. Both `key` and `value` (if
  // specified) will be URL-encoded for safety.
  void add_mock_glic_query_param(const std::string_view& key,
                                 const std::string_view& value = "") {
    mock_glic_query_params_.emplace(key, value);
  }

  GURL GetGuestURL() {
    CHECK(guest_url_.is_valid()) << "Guest URL not yet configured.";
    return guest_url_;
  }

 private:
  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  // This is the default test file. Tests can override with a different path.
  std::string glic_page_path_ = "/glic/test_client/index.html";
  GURL guest_url_;

  base::test::ScopedFeatureList features_;

  std::unique_ptr<glic::GlicTestEnvironment> glic_test_environment_;
  std::map<std::string, std::string> mock_glic_query_params_;
};

// For most tests, you can alias or inherit from this instead of deriving your
// own `InteractiveGlicTestT<...>`.
using InteractiveGlicTest = InteractiveGlicTestT<InteractiveBrowserTest>;

// For testing IPH associated with glic - i.e. help bubbles that anchor in the
// chrome browser rather than showing up in the glic content itself - inherit
// from this.
using InteractiveGlicFeaturePromoTest =
    InteractiveGlicTestT<InteractiveFeaturePromoTest>;

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_INTERACTIVE_GLIC_TEST_H_
