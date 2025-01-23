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

// Observes `controller` for whether it has a loaded web client.
class GlicInitializedStateObserver
    : public ui::test::PollingStateObserver<bool> {
 public:
  explicit GlicInitializedStateObserver(const GlicWindowController& controller);
  ~GlicInitializedStateObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicInitializedStateObserver,
                               kGlicInitializedState);

DECLARE_ELEMENT_IDENTIFIER_VALUE(kInstrumentedGlicWebContentsElementId);
static constexpr char kMockGlicCloseButtonId[] = "closebn";

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

  // Opens the glic window. It should not already be open.
  auto OpenGlicWindow(GlicWindowMode mode) {
    Api::MultiStep steps;
    switch (mode) {
      case GlicWindowMode::kAttached:
        steps.emplace_back(Api::PressButton(kGlicButtonElementId));
        break;
      case GlicWindowMode::kDetached:
        steps.emplace_back(
            Api::Do([this] { window_controller().Show(nullptr); }));
        break;
    }
    steps.emplace_back(
        // TODO(crbug.com/389729273): currently, the view element being shown is
        // a proxy for the event that the glic UI generates when it is finished
        // loading. Since this may change, we should also ensure that the "load
        // complete" event was generated.
        Api::InAnyContext(Api::WaitForShow(kGlicViewElementId)));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // Closes the glic window, which must be open.
  auto CloseGlicWindow() {
    auto steps =
        Api::Steps(ClickMockGlicElement(kMockGlicCloseButtonId),
                   Api::InAnyContext(Api::WaitForHide(kGlicViewElementId)));
    Api::AddDescriptionPrefix(steps, "CloseGlicWindow");
    return steps;
  }

  // Clicks the element with HTML `id` in the mock glic page.
  auto ClickMockGlicElement(std::string id) {
    auto steps = Api::Steps(
        Api::ObserveState(kGlicInitializedState, std::ref(window_controller())),
        Api::WaitForState(kGlicInitializedState, true),
        ExecuteGlicJs(base::StringPrintf(
                          "window.document.getElementById('%s').click();", id),
                      Api::ExecuteJsMode::kWaitForCompletion),
        Api::StopObservingState(kGlicInitializedState));
    Api::AddDescriptionPrefix(
        steps, base::StringPrintf("ClickMockGlicElement(\"%s\")", id));
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

  // Possibly instruments the glic WebUI if it is not already instrumented.
  auto MaybeInstrumentGlic() {
    return Api::If(
        [this]() {
          return !static_cast<internal::InteractiveBrowserTestPrivate&>(
                      Api::private_test_impl())
                      .IsInstrumentedWebContents(
                          kInstrumentedGlicWebContentsElementId);
        },
        Api::Steps(Api::InAnyContext(Api::InstrumentNonTabWebView(
            kInstrumentedGlicWebContentsElementId,
            GlicView::kWebViewElementIdForTesting))));
  }

  // Executes code in the glic webview, inside the wrapper WebUI.
  //
  // Currently injects via `webview.executeScript()` but alternatively it might
  // be better to add the ability to get child frames of a WebContents and
  // inject directly.
  auto ExecuteGlicJs(std::string func, Api::ExecuteJsMode mode) {
    // Need to escape quotes.
    std::ostringstream oss;
    size_t last = 0;
    for (size_t pos = func.find_first_of("\'\"", 0); pos != std::string::npos;
         pos = func.find_first_of("\'\"", last)) {
      oss << func.substr(last, pos - last) << "\\" << func[pos];
      last = pos + 1;
    }
    oss << func.substr(last);
    const auto actual_func = base::StringPrintf(
        R"(
          function(webview) {
            webview.executeScript({ code: '%s' });
          }
        )",
        oss.str().c_str());
    auto steps = Api::Steps(MaybeInstrumentGlic(),
                            Api::InAnyContext(Api::ExecuteJsAt(
                                kInstrumentedGlicWebContentsElementId,
                                {"#guest-frame"}, actual_func, mode)));
    Api::AddDescriptionPrefix(steps, "ExecuteGlicJs");
    return steps;
  }

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
