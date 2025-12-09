// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"

namespace glic {

namespace {

using mojom::WebUiState;

class GlicUiStateHistoryObserver
    : public ui::test::StateObserver<std::vector<WebUiState>>,
      public Host::Observer {
 public:
  explicit GlicUiStateHistoryObserver(Host* host) : host_(*host) {
    states_.push_back(host->GetPrimaryWebUiState());
    host->AddObserver(this);
  }

  ~GlicUiStateHistoryObserver() override { host_->RemoveObserver(this); }

  ValueType GetStateObserverInitialState() const override { return states_; }

 private:
  void WebUiStateChanged(WebUiState state) override {
    states_.push_back(state);
    OnStateObserverStateChanged(states_);
  }

  const raw_ref<Host> host_;
  ValueType states_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(GlicUiStateHistoryObserver,
                                    kGlicUiStateHistory);

class ContextAccessIndicatorObserver
    : public ui::test::StateObserver<std::vector<bool>> {
 public:
  explicit ContextAccessIndicatorObserver(GlicKeyedService* service) {
    scoped_subscription_ =
        service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(&ContextAccessIndicatorObserver::
                                    ContextAccessIndicatorStatusChanged,
                                base::Unretained(this)));
  }

  ~ContextAccessIndicatorObserver() override = default;

  ValueType GetStateObserverInitialState() const override { return states_; }

 private:
  void ContextAccessIndicatorStatusChanged(bool status) {
    states_.push_back(status);
    OnStateObserverStateChanged(states_);
  }

  base::CallbackListSubscription scoped_subscription_;
  ValueType states_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ContextAccessIndicatorObserver,
                                    kGlicContextAccessIndicatorHistory);

// Specifies artificial parameters for how network and loading should behave for
// tests in this file.
struct TestParams {
  TestParams() = default;
  explicit TestParams(bool connected) : start_connected(connected) {}
  TestParams(base::TimeDelta time_before_loading_page_,
             base::TimeDelta min_loading_page_duration_,
             base::TimeDelta max_loading_page_duration_,
             base::TimeDelta loading_delay_)
      : time_before_loading_page(time_before_loading_page_),
        min_loading_page_duration(min_loading_page_duration_),
        max_loading_page_duration(max_loading_page_duration_),
        loading_delay(loading_delay_) {}
  ~TestParams() = default;

  // Time before loading page shows.
  std::optional<base::TimeDelta> time_before_loading_page;
  // Minimum time loading page shows.
  std::optional<base::TimeDelta> min_loading_page_duration;
  // Maximum time loading page shows before error.
  std::optional<base::TimeDelta> max_loading_page_duration;
  // Time to load the glic UI.
  std::optional<base::TimeDelta> loading_delay;
  // Whether the page believes it has network at startup.
  bool start_connected = true;

  base::FieldTrialParams GetFieldTrialParams() const {
    base::FieldTrialParams params;
    if (time_before_loading_page) {
      params.emplace(
          features::kGlicPreLoadingTimeMs.name,
          base::StringPrintf("%u", time_before_loading_page->InMilliseconds()));
    }
    if (min_loading_page_duration) {
      params.emplace(features::kGlicMinLoadingTimeMs.name,
                     base::StringPrintf(
                         "%u", min_loading_page_duration->InMilliseconds()));
    }
    if (max_loading_page_duration) {
      params.emplace(features::kGlicMaxLoadingTimeMs.name,
                     base::StringPrintf(
                         "%u", max_loading_page_duration->InMilliseconds()));
    }
    return params;
  }
};

const ui::Accelerator escape_key(ui::VKEY_ESCAPE, ui::EF_NONE);

}  // namespace

// Base class that sets up network connection mode and timeouts based on
// `TestParams` (see above).
class GlicUiInteractiveUiTestBase : public test::InteractiveGlicTest {
 public:
  explicit GlicUiInteractiveUiTestBase(const TestParams& params)
      : InteractiveGlicTestMixin(params.GetFieldTrialParams()) {
    if (!params.start_connected) {
      GlicUI::simulate_no_connection_for_testing();
    }
    if (params.loading_delay) {
      std::ostringstream oss;
      oss << params.loading_delay->InMilliseconds();
      add_mock_glic_query_param("delay_ms", oss.str());
    }
  }
  ~GlicUiInteractiveUiTestBase() override = default;

  auto CheckElementVisible(const DeepQuery& where, bool visible) {
    MultiStep steps;
    if (visible) {
      steps =
          InAnyContext(WaitForElementVisible(test::kGlicHostElementId, where));
    }
    steps += InAnyContext(CheckJsResultAt(test::kGlicHostElementId, where,
                                          "(el) => el.hidden",
                                          testing::Ne(visible)));
    AddDescriptionPrefix(steps, "CheckElementVisible");
    return steps;
  }

  auto CheckMockElementChecked(const DeepQuery& where, bool checked) {
    MultiStep steps = Steps(InAnyContext(WaitForElementVisible(
                                test::kGlicContentsElementId, {"body"})),
                            InAnyContext(CheckJsResultAt(
                                test::kGlicContentsElementId, where,
                                "(el) => el.checked", testing::Eq(checked))));
    AddDescriptionPrefix(steps, "CheckElementChecked");
    return steps;
  }

  auto CheckEscapeKeyDismisses(const DeepQuery& panel) {
    return InAnyContext(
        WaitForShow(test::kGlicHostElementId), CheckElementVisible(panel, true),
        InSameContext(SendAccelerator(test::kGlicHostElementId, escape_key)
                          .SetMustRemainVisible(false),
                      WaitForHide(kGlicViewElementId)),
        CheckControllerHasWidget(false));
  }

  auto ChangeConnectionState(bool online) {
    return ExecuteJs(test::kGlicHostElementId,
                     base::StringPrintf(R"(
        function () {
          const controller = window.appRouter.glicController;
          controller.simulateNoConnection = %s;
          controller.%s();
        }
      )",
                                        base::ToString(!online),
                                        online ? "online" : "offline"));
  }

  static auto CurrentStateMatcher(testing::Matcher<WebUiState> target) {
    return testing::AllOf(
        testing::Not(testing::IsEmpty()),
        testing::Property(&GlicUiStateHistoryObserver::ValueType::back,
                          target));
  }

  static auto IsCurrently(WebUiState state) {
    return CurrentStateMatcher(state);
  }

  static auto IsNotCurrently(WebUiState state) {
    return CurrentStateMatcher(testing::Ne(state));
  }

  static auto IsContextAccessIndicatorCurrently(bool showing) {
    return testing::AllOf(
        testing::Not(testing::IsEmpty()),
        testing::Property(&ContextAccessIndicatorObserver::ValueType::back,
                          showing));
  }

  const DeepQuery kOfflinePanel = {"#offlinePanel"};
  const DeepQuery kLoadingPanel = {"#loadingPanel"};
  const DeepQuery kErrorPanel = {"#errorPanel"};
  const DeepQuery kContentsPanel = {"#guestPanel"};
};

class GlicUiInteractiveTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiInteractiveTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/true)) {}
  ~GlicUiInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiInteractiveTest, OpenGlicWindow) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  base::HistogramTester histogram_tester;
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly));
  // The browser is active when opening the Glic window.
  histogram_tester.ExpectUniqueSample("Glic.Session.Open.BrowserActiveState",
                                      0 /*kBrowserActive*/, 1);
}

// Tests the network being connected at startup (as normal).
class GlicUiConnectedUiTest : public GlicUiInteractiveUiTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  GlicUiConnectedUiTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/true)) {
    if (IsDetachedOnlyModeEnabled()) {
      feature_list_.InitAndEnableFeature(features::kGlicDetached);
    } else {
      feature_list_.InitAndDisableFeature(features::kGlicDetached);
    }
  }
  ~GlicUiConnectedUiTest() override = default;

  bool IsDetachedOnlyModeEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, GlicUiConnectedUiTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest, DisconnectedPanelHidden) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsNotCurrently(WebUiState::kOffline)),
      CheckElementVisible(kOfflinePanel, false));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest,
                       DoesNotHidePanelWhenReadyButOffline) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      ChangeConnectionState(false), CheckElementVisible(kContentsPanel, true),
      CheckState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest, CanAttachWithBrowserWindow) {
  if (IsDetachedOnlyModeEnabled()) {
    GTEST_SKIP() << "Skipping for kGlicDetached only mode";
  }
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckMockElementChecked({"#canAttachCheckbox"}, true));
}

// TODO(crbug.com/454087646): Not reliable yet.
IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest,
                       DISABLED_CanNotAttachWithMinimizedBrowser) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached,
                     GlicInstrumentMode::kHostAndContents),
      CheckMockElementChecked({"#canAttachCheckbox"}, true),
      Do([&]() { browser()->GetBrowserView().Minimize(); }),
      // TODO(harringtond): Ideally this would wait until not checked, rather
      // than check only once. There's no guarantee the web client
      // has been updated before this code runs. Currently, this
      // test works, though it's a risk for flakiness.
      CheckMockElementChecked({"#canAttachCheckbox"}, false));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest,
                       DoesNotNavigateToUnsupportedOrigin) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      InAnyContext(ExecuteJs(test::kGlicContentsElementId,
                             R"js(()=>{location = 'http://b.test/page';})js")),
      // Just wait a bit and make sure the page doesn't navigate.
      InAnyContext(CheckJsResult(test::kGlicContentsElementId, R"js(
  ()=>{
    const {promise, resolve} = Promise.withResolvers();
    window.setTimeout(() => resolve(true), 1000);
    return promise;
  })js")));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest,
                       HidesTabAccessUIOnWebClientCrash) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      ObserveState(kGlicContextAccessIndicatorHistory, glic_service()),
      OpenGlicWindow(GlicWindowMode::kAttached,
                     GlicInstrumentMode::kHostAndContents),
      WaitForElementVisible(test::kGlicContentsElementId, {"body"}),
      InAnyContext(ExecuteJs(
          test::kGlicContentsElementId,
          R"js(()=>{client.browser.setContextAccessIndicator(true);})js")),
      InAnyContext(WaitForState(kGlicContextAccessIndicatorHistory,
                                IsContextAccessIndicatorCurrently(true))),
      // Kills the web client process, simulating a renderer crash.
      Do([this] { FindGlicGuestMainFrame()->GetProcess()->Shutdown(0); }),
      InAnyContext(
          WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kError))),
      InAnyContext(WaitForState(kGlicContextAccessIndicatorHistory,
                                IsContextAccessIndicatorCurrently(false))));
}

// Tests the network being unavailable at startup.
class GlicUiDisconnectedUiTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiDisconnectedUiTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/false)) {
    feature_list_.InitAndDisableFeature(features::kGlicIgnoreOfflineState);
  }
  ~GlicUiDisconnectedUiTest() override = default;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, DisconnectedPanelShown) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kOffline)),
      CheckElementVisible(kOfflinePanel, true));
}

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, LoadsWhenBackOnline) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      ChangeConnectionState(true),
      WaitForState(kGlicUiStateHistory, IsNotCurrently(WebUiState::kOffline)),
      WaitForElementVisible(test::kGlicHostElementId, kContentsPanel),
      CheckElementVisible(kOfflinePanel, false),
      CheckState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)));
}

// Tests the entire loading sequence through to error if Glic is slow to load.
class GlicUiFullLoadingSequenceTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiFullLoadingSequenceTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Milliseconds(250),  // Pre-load
                       base::Milliseconds(250),  // Min loading time
                       base::Milliseconds(500),  // Max loading time
                       base::Hours(1)))          // Actual loading time
  {}
  ~GlicUiFullLoadingSequenceTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiFullLoadingSequenceTest, Test) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kError)),
      CheckElementVisible(kErrorPanel, true),
      CheckElementVisible(kContentsPanel, false),
      CheckState(kGlicUiStateHistory,
                 testing::ElementsAre(
                     WebUiState::kUninitialized, WebUiState::kBeginLoad,
                     WebUiState::kShowLoading, WebUiState::kFinishLoading,
                     WebUiState::kError)));
}

// Tests the loading sequence where Glic loads almost immediately and there is
// no hold.
class GlicUiQuickLoadingSequenceNoHoldTest
    : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiQuickLoadingSequenceNoHoldTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Milliseconds(250),  // Pre-load
                       base::Milliseconds(0),    // Min loading time
                       base::Hours(1),           // Max loading time
                       base::Seconds(1)))        // Actual loading time
  {}
  ~GlicUiQuickLoadingSequenceNoHoldTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiQuickLoadingSequenceNoHoldTest, Test) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      CheckElementVisible(kContentsPanel, true),
      CheckState(kGlicUiStateHistory,
                 testing::ElementsAre(
                     WebUiState::kUninitialized, WebUiState::kBeginLoad,
                     WebUiState::kShowLoading, WebUiState::kFinishLoading,
                     WebUiState::kReady)));
}

// Tests the loading sequence where Glic loads almost immediately and there is a
// hold.
class GlicUiQuickLoadingSequenceWithHoldTest
    : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiQuickLoadingSequenceWithHoldTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Milliseconds(0),     // Pre-load
                       base::Seconds(5),          // Min loading time
                       base::Hours(1),            // Max loading time
                       base::Milliseconds(500)))  // Actual loading time
  {}
  ~GlicUiQuickLoadingSequenceWithHoldTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiQuickLoadingSequenceWithHoldTest, Test) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      CheckElementVisible(kContentsPanel, true),
      CheckState(
          kGlicUiStateHistory,
          testing::ElementsAre(WebUiState::kUninitialized,
                               WebUiState::kBeginLoad, WebUiState::kShowLoading,
                               WebUiState::kHoldLoading, WebUiState::kReady)));
}

// Tests the loading sequence where Glic loads almost immediately during
// preload.
class GlicUiQuickLoadingSequenceWithPreloadTest
    : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiQuickLoadingSequenceWithPreloadTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Seconds(3),         // Pre-load
                       base::Seconds(5),         // Min loading time
                       base::Seconds(10),        // Max loading time
                       base::Milliseconds(10)))  // Actual loading time
  {}
  ~GlicUiQuickLoadingSequenceWithPreloadTest() override = default;
};

// TODO(crbug.com/418639389): these probably need to be broken into unit
// tests with only integration tests for the messaging being passed back and
// forth; slow test runners can cause any time limit to be overrun.
IN_PROC_BROWSER_TEST_F(GlicUiQuickLoadingSequenceWithPreloadTest,
                       DISABLED_Test) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      CheckElementVisible(kContentsPanel, true),
      CheckState(
          kGlicUiStateHistory,
          testing::ElementsAre(WebUiState::kUninitialized,
                               WebUiState::kBeginLoad, WebUiState::kReady)));
}

// Tests the loading panel is visible during load.
class GlicUiLoadingPanelWaitingTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiLoadingPanelWaitingTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Milliseconds(0),  // Pre-load
                       base::Milliseconds(0),  // Min loading time
                       base::Hours(1),         // Max loading time
                       base::Hours(1)))        // Actual loading time
  {}
  ~GlicUiLoadingPanelWaitingTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelWaitingTest, Test) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory,
                   IsCurrently(WebUiState::kFinishLoading)),
      CheckElementVisible(kLoadingPanel, true));
}

// Tests the loading panel is visible during hold.
class GlicUiLoadingPanelHoldingTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiLoadingPanelHoldingTest()
      : GlicUiInteractiveUiTestBase(
            TestParams(base::Milliseconds(0),     // Pre-load
                       base::Hours(1),            // Min loading time
                       base::Hours(2),            // Max loading time
                       base::Milliseconds(500)))  // Actual loading time
  {}
  ~GlicUiLoadingPanelHoldingTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelHoldingTest, Test) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kHoldLoading)),
      CheckElementVisible(kLoadingPanel, true));
}

// Test that the escape key can be used to dismiss the floaty window in various
// loading and error states.

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, EscapeKeyDismisses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kOffline)),
      CheckEscapeKeyDismisses(kOfflinePanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelWaitingTest, EscapeKeyDismisses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory,
                   IsCurrently(WebUiState::kFinishLoading)),
      CheckEscapeKeyDismisses(kLoadingPanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelHoldingTest, EscapeKeyDismisses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kHoldLoading)),
      CheckEscapeKeyDismisses(kLoadingPanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiFullLoadingSequenceTest, EscapeKeyDismisses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kError)),
      CheckEscapeKeyDismisses(kErrorPanel));
}

#if !BUILDFLAG(IS_CHROMEOS)
// Multi-profile is not supported on ChromeOS.
class GlicWithMultipleProfilesTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicWithMultipleProfilesTest() : GlicUiInteractiveUiTestBase({}) {}
  ~GlicWithMultipleProfilesTest() override = default;

  Browser* CreateBrowserWithNewProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile& new_profile =
        profiles::testing::CreateProfileSync(profile_manager, new_path);

    return CreateBrowser(&new_profile);
  }
};

// Creates two browsers with different profiles. Opens glic in each and verifies
// it loads, doesn't crash, and hides the other glic window.
IN_PROC_BROWSER_TEST_F(GlicWithMultipleProfilesTest, OpenGlicInEachProfile) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  Browser* first_browser = browser();
  Browser* second_browser = CreateBrowserWithNewProfile();
  SetActiveBrowser(second_browser);

  RunTestSequence(
      // Warning!: `kAttached` really just clicks the glic button, the window
      // will open in detached mode because `features::kGlicDetached` is
      // enabled. We do this because InteractiveGlicTestMixin::ToggleGlicWindow
      // doesn't work right in detached mode with multiple profiles.
      // TODO(b/418284946): Fix ToggleGlicWindow.
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly));

  SetActiveBrowser(first_browser);
  RunTestSequence(
      CheckControllerShowing(false),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly));

  SetActiveBrowser(second_browser);
  RunTestSequence(
      CheckControllerShowing(false),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<GURL>,
                                    kOpenedTabUrlState);

class GlicApiUiRedirectTest : public test::InteractiveGlicTest,
                              public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    admin_hostname_ =
        GetParam() ? "admin.google.com" : "access.workspace.google.com";

    embedded_https_test_server().SetCertHostnames(
        {admin_hostname_, "gemini.google.com", "127.0.0.1"});
    embedded_https_test_server().AddDefaultHandlers();
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());

    GURL admin_url_base = embedded_https_test_server().GetURL("/echo?");
    GURL::Replacements replacements;
    replacements.SetHostStr(admin_hostname_);
    GURL admin_url = admin_url_base.ReplaceComponents(replacements);

    SetGlicPagePath("/server-redirect-302");
    add_mock_glic_query_param(admin_url.spec());

    replacements.SetHostStr("gemini.google.com");
    destination_url_ =
        embedded_https_test_server().GetURL("/echo").ReplaceComponents(
            replacements);

    GURL::Replacements pattern_replacements;
    pattern_replacements.SetPathStr("/echo");
    pattern_replacements.SetQueryStr("*");

    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicDebugWebview, {}},
        {features::kGlicCaaGuestError,
         {{"glic-caa-link-url", destination_url_.spec()},
          {"glic-caa-redirect-patterns",
           admin_url.ReplaceComponents(pattern_replacements).spec()}}}};

    redirect_features_.InitWithFeaturesAndParameters(enabled_features,
                                                     /*disabled_features=*/{});
    test::InteractiveGlicTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule(admin_hostname_, "127.0.0.1");
    host_resolver()->AddRule("gemini.google.com", "127.0.0.1");

    test::InteractiveGlicTest::SetUpOnMainThread();
  }
  const GURL& destination_url() { return destination_url_; }

 protected:
  auto WaitForTabOpenedTo(int tab, GURL url) {
    return Steps(
        PollState(kOpenedTabUrlState,
                  [this, tab]() {
                    auto* const model = browser()->tab_strip_model();
                    auto* tab_at_index = model->GetTabAtIndex(tab);
                    if (!tab_at_index) {
                      return GURL();
                    }
                    return tab_at_index->GetContents()->GetVisibleURL();
                  }),
        WaitForState(kOpenedTabUrlState, url),
        StopObservingState(kOpenedTabUrlState));
  }

  base::UserActionTester& user_action_tester() { return user_action_tester_; }

 private:
  GURL destination_url_;
  std::string admin_hostname_;
  base::UserActionTester user_action_tester_;
  base::test::ScopedFeatureList redirect_features_;
};

IN_PROC_BROWSER_TEST_P(GlicApiUiRedirectTest, AccessDeniedAdmin) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  auto https_server_running =
      embedded_https_test_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForElementVisible(
          test::kGlicHostElementId, {"#disabledByAdminPanel:not([hidden])"})),
      CheckTabCount(1),
      InAnyContext(WaitForElementVisible(test::kGlicHostElementId,
                                         {"#disabledByAdminPanel .notice a"})),
      ClickElement(test::kGlicHostElementId,
                   {"#disabledByAdminPanel .notice a"})
          .SetContext(ui::InteractionSequence::ContextMode::kAny)
          .SetMustRemainVisible(false),
      InAnyContext(Do([&]() {
        EXPECT_EQ(user_action_tester().GetActionCount(
                      "Glic.DisabledByAdminPanelLinkClicked"),
                  1);
      })),
      WaitForTabOpenedTo(1, destination_url()), CheckControllerShowing(false));
}

INSTANTIATE_TEST_SUITE_P(All, GlicApiUiRedirectTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest, AccessDeniedAdminWithoutLink) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(Do([&]() {
        browser()->profile()->GetPrefs()->SetInteger(
            ::prefs::kGeminiSettings,
            static_cast<int>(glic::prefs::SettingsPolicyState::kDisabled));
      })),

      InAnyContext(WaitForElementVisible(
          test::kGlicHostElementId, {"#disabledByAdminPanel:not(.show-disabled-"
                                     "by-admin-link) .without-link"})),
      InAnyContext(EnsureNotVisible(test::kGlicHostElementId,
                                    {"#disabledByAdminPanel a"})));
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGlicFreInnerContentsElementId);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kControllerIsShowingState);

class GlicUiUnifiedFreIntegrationTest : public GlicUiInteractiveUiTestBase {
 public:
  GlicUiUnifiedFreIntegrationTest()
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/true)) {
    feature_list_.InitWithFeatures(
        {features::kGlicUnifiedFreScreen, features::kGlicMultiInstance}, {});
  }
  ~GlicUiUnifiedFreIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("glic.test", "127.0.0.1");
    GlicUiInteractiveUiTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    browser()->profile()->GetPrefs()->SetInteger(
        glic::prefs::kGlicCompletedFre,
        static_cast<int>(glic::prefs::FreStatus::kNotStarted));
  }

  const DeepQuery kFreContainer = {"#fre-app-container"};
  const DeepQuery kGlicContainer = {"#glic-app-container"};
  const DeepQuery kGlicGuestPanel = {"#glic-app-container", "#guestPanel"};

  const DeepQuery kMockFreClientNoThanksButton = {"#noThanks"};
  const DeepQuery kMockFreClientContinueButton = {"#continue"};

  auto InstrumentFreWebview() {
    return Steps(InAnyContext(WaitForElementVisible(
                     test::kGlicHostElementId,
                     {"#fre-app-container", "#freGuestFrame"})),
                 InAnyContext(InstrumentInnerWebContents(
                     kGlicFreInnerContentsElementId, test::kGlicHostElementId,
                     0)));  // Index 0 for the FRE webview
  }

  auto ClickFreWebviewElement(const DeepQuery& where) {
    return InAnyContext(ClickElement(kGlicFreInnerContentsElementId, where));
  }

  auto CheckElementHidden(const DeepQuery& query, bool hidden = true) {
    return CheckJsResultAt(test::kGlicHostElementId, query, "(el) => el.hidden",
                           hidden);
  }

 protected:
  net::EmbeddedTestServer glic_server_;
  GURL fre_url_;
  GURL glic_guest_url_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicUiUnifiedFreIntegrationTest, ShowsFreInsteadOfGlic) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),
      CheckState(kGlicUiStateHistory, IsNotCurrently(WebUiState::kReady)),
      CheckElementVisible(kFreContainer, true),
      CheckElementVisible(kGlicContainer, false));
}

IN_PROC_BROWSER_TEST_F(GlicUiUnifiedFreIntegrationTest,
                       AcceptFreTransitionsToGlic) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),

      InstrumentFreWebview(),
      WaitForElementVisible(kGlicFreInnerContentsElementId,
                            kMockFreClientContinueButton),
      ClickFreWebviewElement(kMockFreClientContinueButton),
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicContainer)),
      InAnyContext(CheckElementHidden(kFreContainer, true)),
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicGuestPanel)),
      CheckResult(
          [this]() {
            return static_cast<glic::prefs::FreStatus>(
                browser()->profile()->GetPrefs()->GetInteger(
                    glic::prefs::kGlicCompletedFre));
          },
          glic::prefs::FreStatus::kCompleted),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicUiUnifiedFreIntegrationTest, RejectFreClosesPanel) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),
      WaitForElementVisible(test::kGlicHostElementId, kFreContainer),

      InAnyContext(InstrumentFreWebview()),
      WaitForElementVisible(kGlicFreInnerContentsElementId,
                            kMockFreClientNoThanksButton),
      InAnyContext(ClickElement(kGlicFreInnerContentsElementId,
                                kMockFreClientNoThanksButton)
                       .SetMustRemainVisible(false)),
      WaitForHide(kGlicViewElementId),
      PollState(kControllerIsShowingState,
                [this]() { return GetWindowControllerImpl().IsShowing(); }),
      WaitForState(kControllerIsShowingState, false),
      CheckControllerShowing(false),
      CheckResult(
          [this]() {
            return static_cast<glic::prefs::FreStatus>(
                browser()->profile()->GetPrefs()->GetInteger(
                    glic::prefs::kGlicCompletedFre));
          },
          glic::prefs::FreStatus::kNotStarted));
}

IN_PROC_BROWSER_TEST_F(GlicUiUnifiedFreIntegrationTest,
                       DismissFreWithEscClosesPanel) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, GetHost()),
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),
      WaitForElementVisible(test::kGlicHostElementId, kFreContainer),
      InAnyContext(SendAccelerator(test::kGlicHostElementId, escape_key)
                       .SetMustRemainVisible(false)),
      WaitForHide(kGlicViewElementId), CheckControllerShowing(false));
}

IN_PROC_BROWSER_TEST_F(GlicUiUnifiedFreIntegrationTest,
                       MultiWindowFreToGlicTransition) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  Browser* browser1 = browser();
  Browser* browser2 = nullptr;
  const GURL glic_url = GURL(features::kGlicGuestURL.Get());
  ASSERT_TRUE(glic_url.is_valid());

  RunTestSequence(
      // Open Window 1 and verify FRE screen shows.
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kHostOnly),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),
      InAnyContext(CheckElementVisible(kFreContainer, true)),
      InAnyContext(CheckElementVisible(kGlicContainer, false)),
      // Open Window 2 and verify FRE screen shows.
      Do([&]() {
        browser2 = CreateBrowser(browser1->profile());
        // SetActiveBrowser to the newly opened window
        SetActiveBrowser(browser2);
        EXPECT_NE(browser(), browser1) << "Failed to switch to new browser";
      }),
      Do([&]() {
        ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), glic_url));
      }),
      InAnyContext(WaitForShow(kGlicViewElementId)),
      InAnyContext(WaitForShow(test::kGlicHostElementId)),
      InAnyContext(CheckElementVisible(kFreContainer, true)),
      InAnyContext(CheckElementVisible(kGlicContainer, false)),
      // Accept FRE in Window 1.
      Do([&]() { SetActiveBrowser(browser1); }), InstrumentFreWebview(),
      WaitForElementVisible(kGlicFreInnerContentsElementId,
                            kMockFreClientContinueButton),
      ClickFreWebviewElement(kMockFreClientContinueButton),
      // Window 1 transitions to Glic app.
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicContainer)),
      InAnyContext(CheckElementHidden(kFreContainer, true)),
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicGuestPanel)),
      // Window 2 also transitions to Glic app.
      Do([&]() { SetActiveBrowser(browser2); }),
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicContainer)),
      InAnyContext(CheckElementHidden(kFreContainer, true)),
      InAnyContext(
          WaitForElementVisible(test::kGlicHostElementId, kGlicGuestPanel)));
}

}  // namespace glic
