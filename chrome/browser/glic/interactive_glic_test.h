// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace glic::test {

namespace internal {

// Observes `controller` for whether it has a loaded web client.
// This can only happen when the glic page has sent the appropriate "I'm fully
// initialized" message, so by this point the glic window should be capable of
// sending messages to the browser.
class GlicInitializedStateObserver
    : public ui::test::PollingStateObserver<bool> {
 public:
  explicit GlicInitializedStateObserver(const GlicWindowController& controller);
  ~GlicInitializedStateObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicInitializedStateObserver,
                               kGlicInitializedState);

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

  template <typename... Args>
  explicit InteractiveGlicTestT(Args&&... args)
      : T(std::forward<Args>(args)...) {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }

  ~InteractiveGlicTestT() override = default;

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
  }

  auto WaitForElementVisible(
      const InteractiveBrowserTestApi::DeepQuery& path_to_element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementIsVisible);

    InteractiveBrowserTestApi::StateChange element_visibility_change;
    element_visibility_change.event = kElementIsVisible;
    element_visibility_change.where = path_to_element;
    element_visibility_change.test_function = "(el) => !el.hidden";
    return InteractiveBrowserTestApi::WaitForStateChange(
        kGlicHostElementId, element_visibility_change);
  }

  // Waits for glic to be ready and instruments it as `kGlicContentsElementId`.
  // This method does not wait for glic to show (only for it to be loaded), so
  // this should not be used directly by interactive ui tests. Instead, use
  // ToggleGlicWindowAndWaitForShow.
  auto WaitForAndInstrumentGlic() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::Steps(
        Api::UninstrumentWebContents(kGlicContentsElementId, false),
        Api::UninstrumentWebContents(kGlicHostElementId, false),
        Api::ObserveState(internal::kGlicInitializedState,
                          std::ref(window_controller())),
        Api::InAnyContext(Api::Steps(
            Api::InstrumentNonTabWebView(kGlicHostElementId,
                                         GlicView::kWebViewElementIdForTesting),
            Api::InstrumentInnerWebContents(kGlicContentsElementId,
                                            kGlicHostElementId, 0),
            Api::WaitForWebContentsReady(kGlicContentsElementId))),
        Api::WaitForState(internal::kGlicInitializedState, true),
        Api::StopObservingState(internal::kGlicInitializedState),
        WaitForElementVisible(kPathToGuestPanel));
    Api::AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
    return steps;
  }

  // Activate one of the glic entrypoints.
  // Also instruments the glic UI as `kGlicContentsElementId`.
  auto ToggleGlicWindow(GlicWindowMode mode) {
    // TODO steps = Api::Steps(std::move(steps), WaitForAndInstrumentGlic());
    switch (mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(kGlicButtonElementId);
      case GlicWindowMode::kDetached:
        return Api::Do([this] { window_controller().Toggle(nullptr); });
    }
  }

  auto ForceShowDetached() {
    return Api::Do([this] { window_controller().ShowDetachedForTesting(); });
  }

  // Activate a glic entrypoint and wait for the window to open.
  auto ToggleGlicWindowAndWaitForShow(GlicWindowMode mode) {
    auto steps = Api::Steps(
        Api::ObserveState(internal::kGlicWindowControllerState,
                          std::ref(window_controller())),
        // TODO: figure out how to deactivate browser so Toggle(nullptr) doesn't
        // try to attach
        mode == GlicWindowMode::kDetached ? ForceShowDetached()
                                          : ToggleGlicWindow(mode),
        WaitForAndInstrumentGlic(),
        Api::WaitForState(internal::kGlicWindowControllerState,
                          GlicWindowController::State::kOpen),
        Api::StopObservingState(internal::kGlicWindowControllerState), );
    Api::AddDescriptionPrefix(steps, "ToggleGlicWindowAndWaitForShow");
    return steps;
  }

  // Activate a glic entrypoint and wait for the window to hide.
  auto ToggleGlicWindowAndWaitForHide(GlicWindowMode mode) {
    auto steps = Api::Steps(
        Api::ObserveState(internal::kGlicWindowControllerState,
                          std::ref(window_controller())),
        ToggleGlicWindow(mode),
        Api::WaitForState(internal::kGlicWindowControllerState,
                          GlicWindowController::State::kClosed),
        Api::StopObservingState(internal::kGlicWindowControllerState), );
    Api::AddDescriptionPrefix(steps, "ToggleGlicWindowAndWaitForHide");
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
  auto CloseGlicWindow() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::InAnyContext(Api::Steps(
        Api::ClickElement(kGlicContentsElementId, kPathToMockGlicCloseButton),
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

 private:
  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  base::test::ScopedFeatureList features_;
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
