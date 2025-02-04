// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_test_environment.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace glic::test {

namespace internal {

// Observes `controller` for changes to state().
class GlicWindowControllerStateObserver
    : public ui::test::PollingStateObserver<GlicWindowController::State> {
 public:
  explicit GlicWindowControllerStateObserver(
      const GlicWindowController& controller);
  ~GlicWindowControllerStateObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicWindowControllerStateObserver,
                               kGlicWindowControllerState);

}  // namespace internal

DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);
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
        {{features::kGlic, glic_params}, {features::kTabstripComboButton, {}}},
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

  void SetUpInProcessBrowserTestFixture() override {
    T::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &InteractiveGlicTestT<T>::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();

    Test::embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    ASSERT_TRUE(Test::embedded_test_server()->Start());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL,
                                    Test::embedded_test_server()
                                        ->GetURL("/glic/test_client/index.html")
                                        .spec());

    // Mark the glic FRE as accepted by default.
    // TODO(cuianthony): Move this logic to glic_test_util.h after
    // https://chromium-review.googlesource.com/c/chromium/src/+/6197534 lands.
    PrefService* prefs = InProcessBrowserTest::browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kGlicCompletedFre, true);
    Browser* browser = InProcessBrowserTest::browser();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser->profile());

    // Signing in is a prerequisite for Glic.
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("test@example.com",
                                      signin::ConsentLevel::kSync);
    glic_test_environment_ =
        std::make_unique<glic::GlicTestEnvironment>(browser->profile());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Ensures that the WebContents for some combination of glic host and contents
  // are instrumented, per `instrument_mode`.
  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode) {
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
                              std::ref(window_controller())),
            Api::InAnyContext(Api::Steps(
                Api::InstrumentNonTabWebView(
                    kGlicHostElementId, GlicView::kWebViewElementIdForTesting),
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
                              std::ref(window_controller())),
            Api::InAnyContext(Api::InstrumentNonTabWebView(
                kGlicHostElementId, GlicView::kWebViewElementIdForTesting)),
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
  //
  // If `instrument_glic_contents` is true both the host and contents will be
  // instrumented (see `WaitForAndInstrumentGlic()`) else only the host will be
  // instrumented (`WaitForAndInstrumentGlicHostOnly()`).
  auto OpenGlicWindow(GlicWindowMode window_mode,
                      GlicInstrumentMode instrument_mode =
                          GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;
    steps.push_back(
        EnsureGlicWindowState("window must be closed in order to open it",
                              GlicWindowController::State::kClosed));
    // Technically, this toggles the window, but we've already ensured that it's
    // closed.
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        steps.push_back(Api::PressButton(kGlicButtonElementId));
        break;
      case GlicWindowMode::kDetached:
        steps.push_back(
            Api::Do([this] { window_controller().ShowDetachedForTesting(); }));
        break;
    }
    steps =
        Api::Steps(std::move(steps), WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // Ensures a mock glic button is visible and then clicks it.
  //
  // The mock glic window takes a moment to resize to the correct size for the
  // window, so some elements may start offscreen/unrendered.
  auto ClickMockGlicElement(
      const WebContentsInteractionTestUtil::DeepQuery& where) {
    auto steps =
        Api::Steps(Api::WaitForElementVisible(kGlicContentsElementId, where),
                   Api::ClickElement(kGlicContentsElementId, where));
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

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        InProcessBrowserTest::browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  template <typename... M>
  auto EnsureGlicWindowState(const std::string& desc, M&&... matchers) {
    return Api::CheckResult([this]() { return window_controller().state(); },
                            testing::Matcher<GlicWindowController::State>(
                                testing::AnyOf(std::forward<M>(matchers)...)),
                            desc);
  }

 private:
  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  base::test::ScopedFeatureList features_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<glic::GlicTestEnvironment> glic_test_environment_;
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

#endif  // CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_
