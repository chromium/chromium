// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
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
      : InteractiveGlicTestT(params.GetFieldTrialParams()) {
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
          window.appController.simulateNoConnection = %s;
          window.appController.%s();
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsNotCurrently(WebUiState::kOffline)),
      CheckElementVisible(kOfflinePanel, false));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest,
                       DoesNotHidePanelWhenReadyButOffline) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)),
      ChangeConnectionState(false), CheckElementVisible(kContentsPanel, true),
      CheckState(kGlicUiStateHistory, IsCurrently(WebUiState::kReady)));
}

IN_PROC_BROWSER_TEST_P(GlicUiConnectedUiTest, CanAttachWithBrowserWindow) {
  if (IsDetachedOnlyModeEnabled()) {
    GTEST_SKIP();
  }
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached,
                                 GlicInstrumentMode::kHostAndContents),
                  CheckMockElementChecked({"#canAttachCheckbox"}, true));
}

// DISABLED: Not reliable yet.
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
      : GlicUiInteractiveUiTestBase(TestParams(/*connected=*/false)) {}
  ~GlicUiDisconnectedUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, DisconnectedPanelShown) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kOffline)),
      CheckElementVisible(kOfflinePanel, true));
}

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, LoadsWhenBackOnline) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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

// See https://crbug.com/418639389 - these probably need to be broken into unit
// tests with only integration tests for the messaging being passed back and
// forth; slow test runners can cause any time limit to be overrun.
IN_PROC_BROWSER_TEST_F(GlicUiQuickLoadingSequenceWithPreloadTest,
                       DISABLED_Test) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
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
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kHoldLoading)),
      CheckElementVisible(kLoadingPanel, true));
}

// Test that the escape key can be used to dismiss the floaty window in various
// loading and error states.

IN_PROC_BROWSER_TEST_F(GlicUiDisconnectedUiTest, EscapeKeyDismisses) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kOffline)),
      CheckEscapeKeyDismisses(kOfflinePanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelWaitingTest, EscapeKeyDismisses) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory,
                   IsCurrently(WebUiState::kFinishLoading)),
      CheckEscapeKeyDismisses(kLoadingPanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiLoadingPanelHoldingTest, EscapeKeyDismisses) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kHoldLoading)),
      CheckEscapeKeyDismisses(kLoadingPanel));
}

IN_PROC_BROWSER_TEST_F(GlicUiFullLoadingSequenceTest, EscapeKeyDismisses) {
  RunTestSequence(
      ObserveState(kGlicUiStateHistory, &host()),
      OpenGlicWindow(GlicWindowMode::kAttached, GlicInstrumentMode::kHostOnly),
      WaitForState(kGlicUiStateHistory, IsCurrently(WebUiState::kError)),
      CheckEscapeKeyDismisses(kErrorPanel));
}

}  // namespace glic
