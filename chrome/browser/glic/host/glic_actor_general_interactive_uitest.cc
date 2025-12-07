// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/widget_test.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace glic::test {

namespace {

using ::base::test::EqualsProto;

namespace apc = ::optimization_guide::proto;
using ClickAction = apc::ClickAction;
using MultiStep = GlicActorUiTest::MultiStep;
using apc::AnnotatedPageContent;

class GlicActorGeneralUiTest : public GlicActorUiTest {
 public:
  MultiStep CheckActorTabDataHasAnnotatedPageContentCache();
  MultiStep OpenDevToolsWindow();
  MultiStep WaitAction(actor::TaskId& task_id,
                       std::optional<base::TimeDelta> duration,
                       tabs::TabHandle& observe_tab_handle,
                       ExpectedErrorResult expected_result = {});
  MultiStep WaitAction(ExpectedErrorResult expected_result = {});

  MultiStep CreateActorTab(int initiator_tab,
                           int initiator_window,
                           bool open_in_background,
                           int* out_acting_tab_id) {
    return Steps(ExecuteInGlic(base::BindLambdaForTesting(
        [=, this](content::WebContents* glic_contents) {
          std::string script = content::JsReplace(
              R"JS(
                    (async () => {
                      const result = await client.browser.createActorTab($1, {
                        openInBackground: $2,
                        initiatorTabId: ($3).toString(),
                        initiatorWindowId: ($4).toString()
                      });
                      return Number.parseInt(result.tabId, 10);
                    })()
                  )JS",
              task_id_.value(), open_in_background, initiator_tab,
              initiator_window);
          *out_acting_tab_id =
              content::EvalJs(glic_contents, script).ExtractInt();
        })));
  }

 protected:
  static constexpr base::TimeDelta kWaitTime = base::Milliseconds(1);

  tabs::TabHandle GetActiveTabHandle() {
    return browser()->GetTabStripModel()->GetActiveTab()->GetHandle();
  }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  tabs::TabHandle null_tab_handle_;
};

MultiStep
GlicActorGeneralUiTest::CheckActorTabDataHasAnnotatedPageContentCache() {
  return Steps(Do([&]() {
    // TODO(crbug.com/420669167): Needs to be reconsidered for multi-tab.
    const AnnotatedPageContent* cached_apc =
        actor::ActorTabData::From(
            GetActorTask()->GetLastActedTabs().begin()->Get())
            ->GetLastObservedPageContent();
    EXPECT_TRUE(cached_apc);
    EXPECT_THAT(*annotated_page_content_, EqualsProto(*cached_apc));
  }));
}

MultiStep GlicActorGeneralUiTest::OpenDevToolsWindow() {
  return Steps(Do([this]() {
    content::WebContents* contents = GetGlicContents();
    DevToolsWindowTesting::OpenDevToolsWindowSync(contents,
                                                  /*is_docked=*/false);
  }));
}

MultiStep GlicActorGeneralUiTest::WaitAction(
    actor::TaskId& task_id,
    std::optional<base::TimeDelta> duration,
    tabs::TabHandle& observe_tab_handle,
    ExpectedErrorResult expected_result) {
  auto wait_provider =
      base::BindLambdaForTesting([&task_id, &observe_tab_handle, duration]() {
        apc::Actions action = actor::MakeWait(duration, observe_tab_handle);
        action.set_task_id(task_id.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(wait_provider), std::move(expected_result));
}

MultiStep GlicActorGeneralUiTest::WaitAction(
    ExpectedErrorResult expected_result) {
  return WaitAction(task_id_, kWaitTime, null_tab_handle_,
                    std::move(expected_result));
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CreateTaskAndNavigate) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  base::HistogramTester histogram_tester;
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  WaitForWebContentsReady(kNewActorTabId, task_url));

  // Two samples of 1 tab for CreateTab, Navigate actions.
  histogram_tester.ExpectUniqueSample("Actor.PageContext.TabCount", 1, 2);
  histogram_tester.ExpectTotalCount("Actor.PageContext.APC.Duration", 2);
  histogram_tester.ExpectTotalCount("Actor.PageContext.Screenshot.Duration", 2);
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest,
                       CachesLastObservedPageContentAfterActionFinish) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(InitializeWithOpenGlicWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId),
                  GetPageContextForActorTab(),
                  CheckActorTabDataHasAnnotatedPageContentCache());
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      ExecuteAction(ArbitraryStringProvider(encodedProto),
                    mojom::PerformActionsErrorReason::kInvalidProto));
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, ActionTargetNotFound) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  auto click_provider = base::BindLambdaForTesting([this]() {
    content::RenderFrameHost* frame =
        tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();
    apc::Actions action =
        actor::MakeClick(*frame, kNonExistentContentNodeId, ClickAction::LEFT,
                         ClickAction::SINGLE);
    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      ExecuteAction(std::move(click_provider),
                    actor::mojom::ActionResultCode::kInvalidDomNodeId));
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, GetPageContextWithoutFocus) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      // After waiting, this should get the context for `kNewActorTabId`, not
      // the currently focused settings page. The choice of the settings page is
      // to make the action fail if we try to fetch the page context of the
      // wrong tab.
      WaitAction());
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, StartTaskWithDevtoolsOpen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  // Ensure a new tab can be created without crashing when the most recently
  // focused browser window is not a normal tabbed browser (e.g. a DevTools
  // window).
  TrackFloatingGlicInstance();
  RunTestSequence(OpenGlicFloatingWindow(), OpenDevToolsWindow(),
                  StartActorTaskInNewTab(task_url, kNewActorTabId));
}

// Test that nothing breaks if the first action isn't tab scoped.
// crbug.com/431239173.
IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, FirstActionIsntTabScoped) {
  // Wait is an example of an action that isn't tab scoped.
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    CreateTask(task_id_, ""),
    WaitAction()
      // clang-format on
  );
}

class GlicActorWithActorDisabledUiTest : public test::InteractiveGlicTest {
 public:
  GlicActorWithActorDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicActor);
  }
  ~GlicActorWithActorDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorWithActorDisabledUiTest, ActorNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.actInFocusedTab); }")));
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest,
                       ActuationSucceedsOnBackgroundTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextForActorTab(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      FocusWebContents(kOtherTabId),
      CheckIsWebContentsCaptured(kNewActorTabId, true),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),
      WaitForJsResult(kNewActorTabId, "() => button_clicked"),
      CheckIsActingOnTab(kNewActorTabId, true),
      CheckIsActingOnTab(kOtherTabId, false),
      StopActorTask(),
      CheckIsWebContentsCaptured(kNewActorTabId, false));
  // clang-format on
}

// Basic test to check that the ActionsResult proto returned from PerformActions
// is filled in with the window and tab observation fields.
IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest,
                       PerformActionsResultObservations) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),

      // Add an extra tab to ensure that the window's tab list is filled in
      // correctly.
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextForActorTab(),
      ClickAction(kClickableButtonLabel,
                  ClickAction::LEFT, ClickAction::SINGLE),

      Do([&]() {
        ASSERT_TRUE(last_execution_result());

        // Check that the window observation is filled in correctly.
        ASSERT_EQ(last_execution_result()->windows().size(), 1);
        apc::WindowObservation window =
           last_execution_result()->windows().at(0);
        EXPECT_EQ(window.id(), browser()->session_id().id());
        EXPECT_EQ(window.activated_tab_id(), tab_handle_.raw_value());
        EXPECT_TRUE(window.active());
        ASSERT_GE(browser()->tab_strip_model()->count(), 2);
        EXPECT_EQ(window.tab_ids().size(),
                  browser()->tab_strip_model()->count());
        for (tabs::TabInterface* tab : *browser()->tab_strip_model()) {
          EXPECT_THAT(window.tab_ids(),
                      testing::Contains(
                          tab->GetHandle().raw_value()));
        }
        EXPECT_EQ(window.tab_ids().size(),
                  browser()->tab_strip_model()->count());

        // Check that the acting tab has an observation that's filled in
        // correctly.
        ASSERT_EQ(last_execution_result()->tabs().size(), 1);
        apc::TabObservation tab = last_execution_result()->tabs().at(0);
        EXPECT_TRUE(tab.has_id());
        EXPECT_EQ(tab.id(), tab_handle_.raw_value());
        EXPECT_TRUE(tab.has_annotated_page_content());
        EXPECT_TRUE(tab.annotated_page_content().has_main_frame_data());
        EXPECT_TRUE(tab.annotated_page_content().has_root_node());
        EXPECT_TRUE(tab.has_screenshot());
        EXPECT_GT(tab.screenshot().size(), 0u);
        EXPECT_TRUE(tab.has_screenshot_mime_type());
        EXPECT_EQ(tab.screenshot_mime_type(), "image/jpeg");
      })
  );
  // clang-format on
}

// Ensure Wait's observe_tab field causes a tab to be observed, even if there is
// no tab in the acting set.
IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, WaitObserveTabFirstAction) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab2Id);

  const GURL url1 = embedded_test_server()->GetURL("/actor/simple.html?tab1");
  const GURL url2 = embedded_test_server()->GetURL("/actor/simple.html?tab2");

  tabs::TabHandle tab1;
  tabs::TabHandle tab2;

  // clang-format off
  RunTestSequence(
      // Create a task without taking any actions so as not to add a tab to the
      // task's acting set.
      OpenGlicWindow(GlicWindowMode::kAttached),
      CreateTask(task_id_, ""),

      // Add two tabs to ensure the correct tab is being added to the
      // observation result.
      AddInstrumentedTab(kTab1Id, url1),
      InAnyContext(WithElement(
          kTab1Id,
          [&tab1](ui::TrackedElement* el) {
            content::WebContents* contents =
                AsInstrumentedWebContents(el)->web_contents();
            tab1 = tabs::TabInterface::GetFromContents(contents)->GetHandle();
          })),
      AddInstrumentedTab(kTab2Id, url2),
      InAnyContext(WithElement(
          kTab2Id,
          [&tab2](ui::TrackedElement* el) {
            content::WebContents* contents =
                AsInstrumentedWebContents(el)->web_contents();
            tab2 = tabs::TabInterface::GetFromContents(contents)->GetHandle();
          })),

      // Wait observing tab1. Ensure tab1 has a TabObservation in the result.
      WaitAction(task_id_, kWaitTime, tab1),
      CheckResult([this]() { return last_execution_result()->tabs().size(); },
                  1),
      Check([&, this]() {
        return last_execution_result()->tabs().at(0).id() == tab1.raw_value();
      }),

      // Wait observing tab2. Ensure tab2 has a TabObservation in the result but
      // tab1 does not.
      WaitAction(task_id_, kWaitTime, tab2),
      CheckResult([this]() { return last_execution_result()->tabs().size(); },
                  1),
      Check([&, this]() {
        return last_execution_result()->tabs().at(0).id() == tab2.raw_value();
      }),

      // Click on tab1 to add it to the acting set. Then wait observing tab2.
      // Ensure both tabs are now in the result observation.
      ClickAction(
          {15, 15}, ClickAction::LEFT, ClickAction::SINGLE, task_id_, tab1),
      WaitAction(task_id_, kWaitTime, tab2),
      CheckResult([this]() { return last_execution_result()->tabs().size(); },
                  2),
      Check([&, this]() {
        std::set<int> tab_ids{
          last_execution_result()->tabs().at(0).id(),
          last_execution_result()->tabs().at(1).id()
        };
        return tab_ids.size() == 2ul &&
            tab_ids.contains(tab1.raw_value()) &&
            tab_ids.contains(tab2.raw_value());
      }),

      // A non-observing wait should now return an observation for tab1; since
      // it was previously acted on by the click, it is now part of the acting
      // set.
      WaitAction(),
      CheckResult([this]() { return last_execution_result()->tabs().size(); },
                  1),
      Check([&, this]() {
        return last_execution_result()->tabs().at(0).id() == tab1.raw_value();
      })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CreateActorTabForeground) {
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  int created_tab_id = -1;

  TabStripModel* tab_strip = browser()->tab_strip_model();
  const int initiator_tab_id = GetActiveTabHandle().raw_value();
  const int initiator_window = browser()->session_id().id();
  const bool open_in_background = false;

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      CreateTask(task_id_, ""),
      CreateActorTab(initiator_tab_id,
                     initiator_window,
                     open_in_background,
                     &created_tab_id),
      Do([&]() {
        // Ensure the new tab is the active tab
        EXPECT_EQ(created_tab_id, GetActiveTabHandle().raw_value());

        // Ensure the new tab was created but is in the foreground.
        tabs::TabInterface* tab = tabs::TabHandle(created_tab_id).Get();
        EXPECT_TRUE(tab);
        EXPECT_TRUE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((tab))));
      })
    );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CreateActorTabBackground) {
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  int existing_tab_id = -1;
  int created_tab_id = -1;

  TabStripModel* tab_strip = browser()->tab_strip_model();
  const int initiator_tab_id = GetActiveTabHandle().raw_value();
  const int initiator_window = browser()->session_id().id();
  const bool open_in_background = true;

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      CreateTask(task_id_, ""),
      Do([&]() {
        existing_tab_id = GetActiveTabHandle().raw_value();
      }),
      CreateActorTab(initiator_tab_id,
                     initiator_window,
                     open_in_background,
                     &created_tab_id),
      Do([&]() {
        // Ensure the previous tab remains in foreground.
        EXPECT_EQ(existing_tab_id, GetActiveTabHandle().raw_value());

        // Ensure the new tab was created and is in the background.
        tabs::TabInterface* tab = tabs::TabHandle(created_tab_id).Get();
        EXPECT_TRUE(tab);
        EXPECT_FALSE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((tab))));
      })
    );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CreateActorTabOnNewTabPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

  int acting_tab_id = -1;

  TabStripModel* tab_strip = browser()->tab_strip_model();
  tabs::TabInterface* ntp_tab;

  const int initiator_tab_id = GetActiveTabHandle().raw_value();
  const int initiator_window = browser()->session_id().id();
  const bool open_in_background = true;

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId, GURL(chrome::kChromeUINewTabURL)),
      InAnyContext(WithElement(kActiveTabId, [&, this](ui::TrackedElement* el) {
        content::WebContents* contents =
            AsInstrumentedWebContents(el)->web_contents();
        ntp_tab = tabs::TabInterface::GetFromContents(contents);
        CHECK(ntp_tab);

        // Create another tab to ensure we're using the initiator tab.
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), GURL(url::kAboutBlankURL),
            WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

        // Sanity check - we'll ensure these won't change after binding to the
        // NTP.
        ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
        ASSERT_FALSE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((ntp_tab))));
      })),
      CreateTask(task_id_, ""),
      CreateActorTab(initiator_tab_id,
                     initiator_window,
                     open_in_background,
                     &acting_tab_id),
      Do([&]() {
        // Ensure the tab returned from CreateActorTab binds to the existing
        // NTP.
        EXPECT_EQ(acting_tab_id, ntp_tab->GetHandle().raw_value())
            << "CreateActorTab didn't reuse NTP initiator tab";

        // Ensure no new tab was created.
        EXPECT_EQ(tab_strip->count(), 2);

        // Ensure the NTP isn't focused since `open_in_background` was set.
        EXPECT_FALSE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((ntp_tab))));
      })
    );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest,
                       CreateActorTabOnNewTabPageForeground) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);

  int acting_tab_id = -1;

  TabStripModel* tab_strip = browser()->tab_strip_model();
  tabs::TabInterface* ntp_tab;

  const int initiator_tab_id = GetActiveTabHandle().raw_value();
  const int initiator_window = browser()->session_id().id();
  const bool open_in_background = false;

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId, GURL(chrome::kChromeUINewTabURL)),
      InAnyContext(WithElement(kActiveTabId, [&, this](ui::TrackedElement* el) {
        content::WebContents* contents =
            AsInstrumentedWebContents(el)->web_contents();
        ntp_tab = tabs::TabInterface::GetFromContents(contents);
        CHECK(ntp_tab);

        // Create another tab to ensure we're using the initiator tab.
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), GURL(url::kAboutBlankURL),
            WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

        // Sanity check - we'll ensure these won't change after binding to the
        // NTP.
        ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
        ASSERT_FALSE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((ntp_tab))));
      })),
      CreateTask(task_id_, ""),
      CreateActorTab(initiator_tab_id,
                     initiator_window,
                     open_in_background,
                     &acting_tab_id),
      Do([&]() {
        // Ensure the tab returned from CreateActorTab binds to the existing
        // NTP.
        EXPECT_EQ(acting_tab_id, ntp_tab->GetHandle().raw_value())
            << "CreateActorTab didn't reuse NTP initiator tab";

        // Ensure no new tab was created.
        EXPECT_EQ(tab_strip->count(), 2);

        // Ensure the NTP is focused since `open_in_background` wasn't set.
        EXPECT_TRUE(
            tab_strip->IsTabInForeground(tab_strip->GetIndexOfTab((ntp_tab))));
      })
    );
  // clang-format on
}

// This test suite sets a 60s click delay so that the click tool waits 60
// seconds between mouse down and mouse up. This is used to ensure that once the
// tool is invoked it doesn't return unless canceled.
class GlicActorCallbackOrderGeneralUiTest : public GlicActorGeneralUiTest {
 public:
  GlicActorCallbackOrderGeneralUiTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicActor,
             {{features::kGlicActorClickDelay.name, "60000ms"}}},
            {actor::kGlicPerformActionsReturnsBeforeStateChange, {}},
        },
        /*disabled_features=*/{});
  }

  ~GlicActorCallbackOrderGeneralUiTest() override = default;

  MultiStep RecordActorTaskStateChanges() {
    return Steps(ExecuteInGlic(base::BindLambdaForTesting(
        [&task_id = task_id_](content::WebContents* glic_contents) {
          std::string script = content::JsReplace(R"JS(
              window.event_log= [];
              window.taskStateObs = client.browser.getActorTaskState($1);
              window.taskStateObs.subscribe((new_state) => {
                const state_name = (() => {
                  switch(new_state) {
                    case ActorTaskState.UNKNOWN: return 'UNKNOWN';
                    case ActorTaskState.IDLE: return 'IDLE';
                    case ActorTaskState.ACTING: return 'ACTING';
                    case ActorTaskState.PAUSED: return 'PAUSED';
                    case ActorTaskState.STOPPED: return 'STOPPED';
                    default: return 'UNEXPECTED';
                  }
                })();
                window.event_log.push(state_name);
              });
            )JS",
                                                  task_id.value());
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        })));
  }

  MultiStep WaitForActorTaskState(mojom::ActorTaskState state) {
    return Steps(ExecuteInGlic(base::BindLambdaForTesting(
        [state](content::WebContents* glic_contents) {
          std::string script = content::JsReplace(
              R"JS(
            window.taskStateObs.waitUntil((state) => {
              return state == $1;
            });
          )JS",
              base::to_underlying(state));
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        })));
  }

  MultiStep InvokeToolThatNeverFinishes(ui::ElementIdentifier tab_id) {
    return Steps(
        InAnyContext(WithElement(
            tab_id,
            [this](ui::TrackedElement* el) {
              content::WebContents* contents =
                  AsInstrumentedWebContents(el)->web_contents();
              acting_tab_ =
                  tabs::TabInterface::GetFromContents(contents)->GetHandle();
            })),
        ExecuteInGlic(base::BindLambdaForTesting(
            [&](content::WebContents* glic_contents) {
              apc::Actions action =
                  actor::MakeClick(acting_tab_, gfx::Point(15, 15),
                                   ClickAction::LEFT, ClickAction::SINGLE);
              action.set_task_id(task_id_.value());
              std::string encoded_action = EncodeActionProto(action);
              std::string script = content::JsReplace(
                  R"JS(
                  window.performActionsPromise =
                      client.browser.performActions(
                          Uint8Array.fromBase64($1).buffer);
                  window.performActionsPromise.then(() => {
                        window.event_log.push('PERFORM_ACTIONS_RETURNED');
                    });
                )JS",
                  encoded_action);
              ASSERT_TRUE(
                  content::ExecJs(glic_contents, std::move(script),
                                  content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
            })));
  }

  std::string GetEventLog() {
    content::WebContents* glic_contents = GetGlicContents();
    return content::EvalJs(glic_contents, "window.event_log.join(',')")
        .ExtractString();
  }

 private:
  tabs::TabHandle acting_tab_;
  base::test::ScopedFeatureList feature_list_;
};

// Ensure that an in-progress call to PerformActions returns before the task
// state change callback after a task is canceled.
IN_PROC_BROWSER_TEST_F(GlicActorCallbackOrderGeneralUiTest,
                       PerformActionsReturnsBeforeStateChangeOnStop) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(GURL(url::kAboutBlankURL), kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      RecordActorTaskStateChanges(),
      InvokeToolThatNeverFinishes(kNewActorTabId),
      StopActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kStopped),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "PERFORM_ACTIONS_RETURNED,"
          "STOPPED")
    );
  // clang-format on
}

// Ensure that an in-progress call to PerformActions returns before the task
// state change callback after a task is paused.
IN_PROC_BROWSER_TEST_F(GlicActorCallbackOrderGeneralUiTest,
                       PerformActionsReturnsBeforeStateChangeOnPause) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(GURL(url::kAboutBlankURL), kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      RecordActorTaskStateChanges(),
      InvokeToolThatNeverFinishes(kNewActorTabId),
      PauseActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kPaused),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "PERFORM_ACTIONS_RETURNED,"
          "PAUSED")
    );
  // clang-format on
}

// Ensure that an in-progress call to PerformActions returns before the task
// state change callback after a task is stopped while interrupted.
IN_PROC_BROWSER_TEST_F(
    GlicActorCallbackOrderGeneralUiTest,
    PerformActionsReturnsBeforeStateChangeFromInterruptAndStop) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(GURL(url::kAboutBlankURL), kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      RecordActorTaskStateChanges(),
      InvokeToolThatNeverFinishes(kNewActorTabId),
      WaitForActorTaskState(mojom::ActorTaskState::kActing),
      InterruptActorTask(),
      StopActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kStopped),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "IDLE,"
          "PERFORM_ACTIONS_RETURNED,"
          "STOPPED")
    );
  // clang-format on
}

// Ensure that an in-progress call to PerformActions returns before the task
// state change callback after a task is paused while interrupted.
IN_PROC_BROWSER_TEST_F(
    GlicActorCallbackOrderGeneralUiTest,
    PerformActionsReturnsBeforeStateChangeFromInterruptAndPause) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(GURL(url::kAboutBlankURL), kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      RecordActorTaskStateChanges(),
      InvokeToolThatNeverFinishes(kNewActorTabId),
      WaitForActorTaskState(mojom::ActorTaskState::kActing),
      InterruptActorTask(),
      PauseActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kPaused),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "IDLE,"
          "PERFORM_ACTIONS_RETURNED,"
          "PAUSED")
    );
  // clang-format on
}

class GlicActorGeneralUiTestHighDPI : public GlicActorGeneralUiTest {
 public:
  static constexpr double kDeviceScaleFactor = 2.0;
  GlicActorGeneralUiTestHighDPI() {
    display::Display::SetForceDeviceScaleFactor(kDeviceScaleFactor);
  }
  ~GlicActorGeneralUiTestHighDPI() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTestHighDPI,
                       CoordinatesApplyDeviceScaleFactor) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  constexpr std::string_view kOffscreenButton = "offscreen";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  gfx::Rect button_bounds;

  auto click_provider = base::BindLambdaForTesting([&button_bounds, this]() {
    // Coordinates are provided in DIPs
    gfx::Point coordinate = button_bounds.CenterPoint();
    apc::Actions action =
        actor::MakeClick(tab_handle_, coordinate, apc::ClickAction::LEFT,
                         apc::ClickAction::SINGLE);

    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ExecuteJs(kNewActorTabId,
        content::JsReplace("() => document.getElementById($1).scrollIntoView()",
          kOffscreenButton)),
      GetPageContextForActorTab(),
      GetClientRect(kNewActorTabId, kOffscreenButton, button_bounds),
      CheckJsResult(kNewActorTabId, "() => offscreen_button_clicked", false),
      ExecuteAction(std::move(click_provider)),
      CheckJsResult(kNewActorTabId, "() => offscreen_button_clicked"));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CloseFloatyShowsToast) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                      kToastState);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  TrackFloatingGlicInstance();
  RunTestSequence(
      OpenGlicFloatingWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      WaitForWebContentsReady(kNewActorTabId, task_url), CloseGlicWindow(),
      PollState(
          kToastState,
          [this]() { return prefs()->GetInteger(actor::ui::kToastShown); }),
      WaitForState(kToastState, 1));
}

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest, CloseSidePanelShowsToast) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                      kToastState);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId,
                             /*open_in_foreground=*/false),
      WaitForWebContentsReady(kNewActorTabId, task_url), CloseGlicWindow(),
      PollState(
          kToastState,
          [this]() { return prefs()->GetInteger(actor::ui::kToastShown); }),
      WaitForState(kToastState, 1));
}

// Test for the above behavior when the killswitch is turned off. i.e. that the
// state change callback invokes before performActions resolves.
class GlicActorCallbackOrderKillSwitchGeneralUiTest
    : public GlicActorCallbackOrderGeneralUiTest {
 public:
  GlicActorCallbackOrderKillSwitchGeneralUiTest() {
    feature_list_.InitAndDisableFeature(
        actor::kGlicPerformActionsReturnsBeforeStateChange);
  }

  ~GlicActorCallbackOrderKillSwitchGeneralUiTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorCallbackOrderKillSwitchGeneralUiTest,
                       StateChangeBeforePerformActionsResolves) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(GURL(url::kAboutBlankURL), kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),

      RecordActorTaskStateChanges(),
      InvokeToolThatNeverFinishes(kNewActorTabId),
      StopActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kStopped),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "STOPPED,"
          "PERFORM_ACTIONS_RETURNED")
    );
  // clang-format on
}

}  // namespace

}  // namespace glic::test
