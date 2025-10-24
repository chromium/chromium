// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/common/webui_url_constants.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/gfx/geometry/point.h"

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
  MultiStep OpenDevToolsWindow(ui::ElementIdentifier contents_to_inspect);
  MultiStep WaitAction(actor::TaskId& task_id,
                       std::optional<base::TimeDelta> duration,
                       tabs::TabHandle& observe_tab_handle,
                       ExpectedErrorResult expected_result = {});
  MultiStep WaitAction(ExpectedErrorResult expected_result = {});

 protected:
  static constexpr base::TimeDelta kWaitTime = base::Milliseconds(1);

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

MultiStep GlicActorGeneralUiTest::OpenDevToolsWindow(
    ui::ElementIdentifier contents_to_inspect) {
  return InAnyContext(
      WithElement(contents_to_inspect, [](ui::TrackedElement* el) {
        content::WebContents* contents =
            AsInstrumentedWebContents(el)->web_contents();
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
        if (duration.has_value()) {
        }
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
                  GetPageContextFromFocusedTab(),
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
  RunTestSequence(InitializeWithOpenGlicWindow(),
                  OpenDevToolsWindow(kGlicContentsElementId),
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
      GetPageContextFromFocusedTab(),
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
      // Add an extra tab to ensure that the window's tab list is filled in
      // correctly.
      AddInstrumentedTab(kOtherTabId, GURL(chrome::kChromeUISettingsURL)),
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),

      GetPageContextFromFocusedTab(),
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

      // Create a task without taking any actions so as not to add a tab to the
      // task's acting set.
      OpenGlicWindow(GlicWindowMode::kAttached),
      CreateTask(task_id_, ""),

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

// TODO(b/450618828): In order for actions over a popup to pass TOCTOU
// validation the APC hit test must return the same node as the node in the
// popup. However, currently APC doesn't include any information about popups so
// this doesn't yet work. Once APC includes popup data and  the TOCTOU hit test
// understands how to hit test it this flag can be re-enabled.
class GlicActorGeneralUiTestDisableToctou : public GlicActorGeneralUiTest {
 public:
  GlicActorGeneralUiTestDisableToctou() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGlicActorToctouValidation);
  }
  ~GlicActorGeneralUiTestDisableToctou() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure tools can send input to a popup widget like a <select> drop down.
// TODO(b/447164093): Mac uses native OS select popups which cannot be acted on
// by Chrome. Once this bug is resolved Mac will use built-in selects during an
// ActorTask and this test can be enabled.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ActOnPopupWidget DISABLED_ActOnPopupWidget
#else
#define MAYBE_ActOnPopupWidget ActOnPopupWidget
#endif
IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTestDisableToctou,
                       MAYBE_ActOnPopupWidget) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  constexpr std::string_view kPlainSelect = "plainSelect";

  const std::string kGetValueScript = content::JsReplace(
      "() => document.getElementById($1).value", kPlainSelect);

  gfx::Rect select_bounds;
  gfx::Point click_point;

  auto click_provider = base::BindLambdaForTesting([&select_bounds, this]() {
    gfx::Point coordinate = select_bounds.CenterPoint();
    apc::Actions action =
        actor::MakeClick(tab_handle_, coordinate, apc::ClickAction::LEFT,
                         apc::ClickAction::SINGLE);

    action.set_task_id(task_id_.value());
    return EncodeActionProto(action);
  });

  // clang-format off
  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      GetPageContextFromFocusedTab(),
      GetClientRect(kNewActorTabId, kPlainSelect, select_bounds),

      // The select box starts with the "alpha" option selected
      CheckJsResult(kNewActorTabId, kGetValueScript, "alpha"),

      // Open a popup <select> control by clicking on it
      Do([&]() {click_point = select_bounds.CenterPoint();}),
      ClickAction(&click_point, ClickAction::LEFT, ClickAction::SINGLE),

      // Move the clickpoint down which should be over the popup and click on a
      // new option.
      Do([&]() {
        click_point = select_bounds.CenterPoint() + gfx::Vector2d(0, 40);
      }),
      ClickAction(&click_point, ClickAction::LEFT, ClickAction::SINGLE),

      // The selected option should have changed.
      CheckJsResult(kNewActorTabId, kGetValueScript, "beta")
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
      GetPageContextFromFocusedTab(),
      GetClientRect(kNewActorTabId, kOffscreenButton, button_bounds),
      CheckJsResult(kNewActorTabId, "() => offscreen_button_clicked", false),
      ExecuteAction(std::move(click_provider)),
      CheckJsResult(kNewActorTabId, "() => offscreen_button_clicked"));
  // clang-format on
}

}  // namespace

}  // namespace glic::test
