// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
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
                       ExpectedErrorResult expected_result = {});
  MultiStep WaitAction(ExpectedErrorResult expected_result = {});
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
    ExpectedErrorResult expected_result) {
  auto wait_provider = base::BindLambdaForTesting([&task_id]() {
    apc::Actions action = actor::MakeWait();
    action.set_task_id(task_id.value());
    return EncodeActionProto(action);
  });
  return ExecuteAction(std::move(wait_provider), std::move(expected_result));
}

MultiStep GlicActorGeneralUiTest::WaitAction(
    ExpectedErrorResult expected_result) {
  return WaitAction(task_id_, std::move(expected_result));
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
