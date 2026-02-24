// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
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

struct ApcHitTestSummary {
  std::string doc_token;
  std::optional<int64_t> node_key;
  std::string debug_string;
};

std::optional<int64_t> GetNodeKeyForDebug(
    const optimization_guide::proto::ContentNode& node) {
  // APC does not consistently populate a stable DOM node id for all node types.
  // `common_ancestor_dom_node_id` is the best available stable identifier for
  // actionable nodes, and is sufficient for this test's debug assertions.
  if (node.has_content_attributes() &&
      node.content_attributes().has_common_ancestor_dom_node_id()) {
    return node.content_attributes().common_ancestor_dom_node_id();
  }
  return std::nullopt;
}

ApcHitTestSummary SummarizeApcHitTestResult(
    const optimization_guide::TargetNodeInfo& hit) {
  std::ostringstream oss;
  oss << "{doc_token=" << hit.document_identifier.serialized_token();

  std::optional<int64_t> node_key;
  if (hit.node) {
    const auto& node = *hit.node;
    oss << " has_node=1";
    node_key = GetNodeKeyForDebug(node);

    if (node.has_content_attributes()) {
      const auto& attrs = node.content_attributes();
      if (attrs.has_common_ancestor_dom_node_id()) {
        oss << " common_ancestor_dom_node_id="
            << attrs.common_ancestor_dom_node_id();
      }
      if (attrs.has_label()) {
        oss << " label=\"" << attrs.label() << "\"";
      }
      if (attrs.has_attribute_type()) {
        oss << " attribute_type=" << static_cast<int>(attrs.attribute_type());
      }
      if (attrs.has_geometry() && attrs.geometry().has_visible_bounding_box()) {
        const auto& vb = attrs.geometry().visible_bounding_box();
        oss << " visible_bbox=" << vb.x() << "," << vb.y() << " " << vb.width()
            << "x" << vb.height();
      }
    }
  } else {
    oss << " has_node=0";
  }

  oss << "}";
  return {.doc_token = hit.document_identifier.serialized_token(),
          .node_key = node_key,
          .debug_string = oss.str()};
}

void ExpectApcHitTestResolvesDifferently(const apc::AnnotatedPageContent& apc,
                                         const gfx::Point& target_dip,
                                         float dsf,
                                         gfx::Point& target_blink_pixels) {
  // Prove APC hit testing uses BlinkSpace/device pixels by hit testing at both
  // the DIP coordinate and the scaled (DIP*DSF) coordinate.
  target_blink_pixels = gfx::ScaleToRoundedPoint(target_dip, dsf);

  const std::optional<optimization_guide::TargetNodeInfo> hit_unscaled =
      optimization_guide::FindNodeAtPoint(apc, target_dip);
  const std::optional<optimization_guide::TargetNodeInfo> hit_scaled =
      optimization_guide::FindNodeAtPoint(apc, target_blink_pixels);

  ASSERT_TRUE(hit_unscaled.has_value());
  ASSERT_TRUE(hit_scaled.has_value());

  const ApcHitTestSummary unscaled_summary =
      SummarizeApcHitTestResult(*hit_unscaled);
  const ApcHitTestSummary scaled_summary =
      SummarizeApcHitTestResult(*hit_scaled);

  // Both hit tests may return a node (e.g. the root/container can cover most of
  // the viewport), but they should not resolve to the same underlying
  // observed node/document. If the coordinate space is correct, the
  // BlinkSpace/device-pixel point should resolve into the iframe document,
  // while the unscaled DIP coordinate should resolve to a different
  // node/document.
  EXPECT_NE(unscaled_summary.doc_token, scaled_summary.doc_token)
      << "hit_unscaled=" << unscaled_summary.debug_string
      << " hit_scaled=" << scaled_summary.debug_string;

  if (unscaled_summary.node_key && scaled_summary.node_key) {
    EXPECT_NE(*unscaled_summary.node_key, *scaled_summary.node_key)
        << "hit_unscaled=" << unscaled_summary.debug_string
        << " hit_scaled=" << scaled_summary.debug_string;
  }
}

class GlicActorGeneralUiTest : public GlicActorUiTest {
 public:
  MultiStep CheckActorTabDataHasAnnotatedPageContentCache();
  MultiStep OpenDevToolsWindow();
  MultiStep WaitAction(actor::TaskId& task_id,
                       std::optional<base::TimeDelta> duration,
                       tabs::TabHandle& observe_tab_handle,
                       ExpectedErrorResult expected_result = {});
  MultiStep WaitAction(ExpectedErrorResult expected_result = {});
  MultiStep WaitForTinyTargetIframeLoaded(ui::ElementIdentifier tab_id);
  StepBuilder CheckDevicePixelRatioIs2(ui::ElementIdentifier tab_id);
  MultiStep ReadTinyTargetBoundsAndAssertApcHitTest(
      ui::ElementIdentifier tab_id,
      gfx::Rect& button_bounds,
      gfx::Point& target_blink_pixels,
      float dsf);

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

MultiStep GlicActorGeneralUiTest::WaitForTinyTargetIframeLoaded(
    ui::ElementIdentifier tab_id) {
  return WaitForJsResult(tab_id, "() => window.button_bounds !== null");
}

GlicActorUiTest::StepBuilder GlicActorGeneralUiTest::CheckDevicePixelRatioIs2(
    ui::ElementIdentifier tab_id) {
  return CheckJsResult(tab_id, "() => window.devicePixelRatio", 2);
}

MultiStep GlicActorGeneralUiTest::ReadTinyTargetBoundsAndAssertApcHitTest(
    ui::ElementIdentifier tab_id,
    gfx::Rect& button_bounds,
    gfx::Point& target_blink_pixels,
    float dsf) {
  return Steps(WithElement(
      tab_id,
      base::BindLambdaForTesting([this, &button_bounds, &target_blink_pixels,
                                  dsf](ui::TrackedElement* el) {
        content::WebContents* contents =
            AsInstrumentedWebContents(el)->web_contents();
        content::EvalJsResult rect_result =
            content::EvalJs(contents, "(() => window.button_bounds)()");
        const base::DictValue& rect = rect_result.ExtractDict();
        const std::optional<int> x = rect.FindInt("x");
        const std::optional<int> y = rect.FindInt("y");
        const std::optional<int> width = rect.FindInt("width");
        const std::optional<int> height = rect.FindInt("height");
        ASSERT_TRUE(x.has_value());
        ASSERT_TRUE(y.has_value());
        ASSERT_TRUE(width.has_value());
        ASSERT_TRUE(height.has_value());
        button_bounds = gfx::Rect(*x, *y, *width, *height);

        // Keep a deterministic, non-zero DIP origin so unscaled DIP coordinates
        // and scaled BlinkSpace/device-pixel coordinates resolve differently in
        // APC hit testing.
        EXPECT_EQ(button_bounds.origin(), gfx::Point(2, 2));
        // Keep target geometry large enough to avoid tiny-raster instability in
        // headless HighDPI environments while preserving coordinate mismatch.
        EXPECT_EQ(button_bounds.size(), gfx::Size(20, 20));

        const apc::AnnotatedPageContent* cached_apc =
            actor::ActorTabData::From(
                GetActorTask()->GetLastActedTabs().begin()->Get())
                ->GetLastObservedPageContent();
        ASSERT_TRUE(cached_apc);

        ExpectApcHitTestResolvesDifferently(*cached_apc, button_bounds.origin(),
                                            dsf, target_blink_pixels);
      })));
}

MultiStep GlicActorGeneralUiTest::WaitAction(
    actor::TaskId& task_id,
    std::optional<base::TimeDelta> duration,
    tabs::TabHandle& observe_tab_handle,
    ExpectedErrorResult expected_result) {
  auto wait_provider =
      base::BindLambdaForTesting([&task_id, &observe_tab_handle, duration]() {
        apc::Actions action =
            actor::MakeWait(duration, observe_tab_handle, task_id);
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
                         ClickAction::SINGLE, task_id_);
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

struct GlicActorEnabling {
  bool enable_glic_actor = true;
  bool enable_glic_actor_ui = true;
};

class GlicActorWithActorDisabledUiTest
    : public test::InteractiveGlicTest,
      public testing::WithParamInterface<GlicActorEnabling> {
 public:
  GlicActorWithActorDisabledUiTest() {
    const GlicActorEnabling& params = GetParam();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (params.enable_glic_actor) {
      enabled_features.push_back(features::kGlicActor);
    } else {
      disabled_features.push_back(features::kGlicActor);
    }
    if (params.enable_glic_actor_ui) {
      enabled_features.push_back(features::kGlicActorUi);
    } else {
      disabled_features.push_back(features::kGlicActorUi);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~GlicActorWithActorDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicActorWithActorDisabledUiTest, ActorNotAvailable) {
  RunTestSequence(DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.actInFocusedTab); }")));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicActorWithActorDisabledUiTest,
                         testing::Values(GlicActorEnabling{true, false},
                                         GlicActorEnabling{false, true},
                                         GlicActorEnabling{false, false}));

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
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
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

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTest,
                       CreateMultipleTasksInSingleInstanceFails) {
  actor::TaskId first_task_id;
  actor::TaskId second_task_id;
  // clang-format off
  RunTestSequence(
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      CreateTask(first_task_id, ""),

      // Attempting to create a second task should fail and it shouldn't affect
      // the existing task.
      CreateTask(second_task_id, "",
        mojom::CreateTaskErrorReason::kExistingActiveTask),
      Check([&](){return second_task_id.is_null();}),
      CheckActorTaskState(first_task_id, actor::ActorTask::State::kCreated),

      // Stop the actor task.
      StopActorTaskAndWait(first_task_id),

      // Creating a new task now should succeed.
      CreateTask(second_task_id, ""),
      Check([&](){return !second_task_id.is_null();})
    );
  // clang-format on
}

class GlicActorGeneralUiTestWithoutPolicyExemption
    : public GlicActorGeneralUiTest {
 public:
  GlicActorGeneralUiTestWithoutPolicyExemption() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicActor,
                               {{features::kGlicActorPolicyControlExemption
                                     .name,
                                 "false"}}}},
        /*disabled_features=*/{});
  }
  ~GlicActorGeneralUiTestWithoutPolicyExemption() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTestWithoutPolicyExemption,
                       CreateTaskFails) {
  // clang-format off
  RunTestSequence(
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),

      // Since policy exemption isn't enabled creating a task should fail with
      // the policy block reason.
      CreateTask(task_id_, "",
        mojom::CreateTaskErrorReason::kBlockedByPolicy)
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
             {{features::kGlicActorClickDelay.name, "60000ms"},
              {features::kGlicActorPolicyControlExemption.name, "true"}}},
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
              std::to_underlying(state));
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
              apc::Actions action = actor::MakeClick(
                  acting_tab_, gfx::Point(15, 15), ClickAction::LEFT,
                  ClickAction::SINGLE, task_id_);
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

// Tests that when an interrupt and uninterrupt happens during an action, the
// uninterrupt returns to the correct state.
IN_PROC_BROWSER_TEST_F(GlicActorCallbackOrderGeneralUiTest,
                       InterruptAndUninterruptDuringAction) {
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
      WaitForActorTaskState(mojom::ActorTaskState::kIdle),
      UninterruptActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kActing),
      StopActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kStopped),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "IDLE,"
          "ACTING,"
          "PERFORM_ACTIONS_RETURNED,"
          "STOPPED")
    );
  // clang-format on
}

// Tests that if the interrupted state is changed by something other than a call
// to uninterrupt, a subsequent call to uninterrupt will be handled gracefully.
// The client should know that such a call is not meaningful, but we make the
// chrome implementation robust to this regardless.
IN_PROC_BROWSER_TEST_F(GlicActorCallbackOrderGeneralUiTest,
                       UnpairedUninterrupt) {
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
      WaitForActorTaskState(mojom::ActorTaskState::kIdle),

      PauseActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kPaused),
      ResumeActorTask(UpdatedContextOptions(),
                      actor::mojom::ActionResultCode::kOk),
      WaitForActorTaskState(mojom::ActorTaskState::kIdle),

      UninterruptActorTask(),

      StopActorTask(),
      WaitForActorTaskState(mojom::ActorTaskState::kStopped),

      CheckResult([this]() { return GetEventLog(); },
          "IDLE,"
          "ACTING,"
          "IDLE,"
          "PERFORM_ACTIONS_RETURNED,"
          "PAUSED,"
          "IDLE,"
          "STOPPED")
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
                         apc::ClickAction::SINGLE, task_id_);

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

IN_PROC_BROWSER_TEST_F(GlicActorGeneralUiTestHighDPI,
                       CoordinatesApplyDeviceScaleFactor_TinyTarget) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

  const GURL task_url = embedded_test_server()->GetURL(
      "/actor/page_with_tiny_iframe_target.html");

  gfx::Rect button_bounds;
  gfx::Point target_blink_pixels;

  auto click_provider = base::BindLambdaForTesting([&button_bounds, this]() {
    // Coordinates are provided in DIPs (local-root logical pixels; they are
    // only equal to CSS pixels when page zoom is 1.0). Click at a stable point
    // (the top-left corner) so that on HighDPI (2.0x) the DIP coordinate
    // differs from the physical ("BlinkSpace") coordinate:
    //   (2,2) DIP -> (4,4) device px.
    //
    // This is a regression test for browser-side TOCTOU validation. APC hit
    // testing uses visual-viewport-relative BlinkSpace/device pixels, while
    // tool targets are expressed in DIPs. If TOCTOU validation forgets to
    // convert the DIP coordinate into APC geometry coordinates, it can observe
    // a different node than the renderer later hits and fail with
    // kObservedTargetElementChanged. See optimization_guide::FindNodeAtPoint()
    // for the canonical coordinate space contract.
    const gfx::Point coordinate = button_bounds.origin();
    apc::Actions action =
        actor::MakeClick(tab_handle_, coordinate, apc::ClickAction::LEFT,
                         apc::ClickAction::SINGLE, task_id_);

    return EncodeActionProto(action);
  });

  RunTestSequence(
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kNewActorTabId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      CheckDevicePixelRatioIs2(kNewActorTabId),
      WaitForTinyTargetIframeLoaded(kNewActorTabId),
      GetPageContextForActorTab(),
      ReadTinyTargetBoundsAndAssertApcHitTest(
          kNewActorTabId, button_bounds, target_blink_pixels, /*dsf=*/2.0f),
      CheckJsResult(
          kNewActorTabId,
          "() => document.querySelector('iframe').contentWindow.clicked",
          false),
      ExecuteAction(std::move(click_provider)),
      // Expected behavior: browser-side TOCTOU validation scales
      // the DIP point into device pixels before hit testing against
      // APC. This makes the APC "observed target" consistent with
      // the renderer-side live DOM hit test and the click succeeds.
      //
      // If a regression causes the TOCTOU/APC hit test to use
      // unscaled DIPs, ExecuteAction will fail with
      // kObservedTargetElementChanged.
      WaitForJsResult(
          kNewActorTabId,
          "() => document.querySelector('iframe').contentWindow.clicked"));
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

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ScreenshotInMinimizedWindow) {
  EnableScreenshotsInContext();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kIsMinimizedState);

  const GURL task_url = embedded_test_server()->GetURL("/actor/blank.html");
  RunTestSequence(
      // clang-format off
    InitializeWithOpenGlicWindow(),
    StartActorTaskInNewTab(task_url, kNewActorTabId),
    SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                            kActivateSurfaceIncompatibilityNotice),
    // Wait for painting, so we can get the screenshot.
    WaitForFrameSubmitted(kNewActorTabId),

    // Take an action with the actor to make sure that actuation has started.
    ClickAction({1,1}, ClickAction::LEFT, ClickAction::SINGLE),
    // Verify we can get the screenshot.
    GetPageContextForActorTab(),
    Steps(Do(base::BindLambdaForTesting([this](){
      EXPECT_TRUE(annotated_page_content_);
      EXPECT_TRUE(viewport_screenshot_);
      annotated_page_content_.reset();
      viewport_screenshot_.reset();
    }))),

    // Minimize the window and wait until it is minimized.
    Steps(Do([this](){
      browser()->window()->Minimize();
    })),
    PollState(kIsMinimizedState, [this]() {
      return browser()->window()->IsMinimized();
    }),
    WaitForState(kIsMinimizedState, true),

    // Try to get the screenshot when the window is minimized.
    GetPageContextForActorTab(),
    Steps(Do(base::BindLambdaForTesting([this](){
      EXPECT_TRUE(annotated_page_content_);
      EXPECT_TRUE(viewport_screenshot_);
    }))));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ScreenshotInInitiallyMinimizedWindow) {
  EnableScreenshotsInContext();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kIsMinimizedState);

  const GURL task_url = embedded_test_server()->GetURL("/actor/blank.html");
  RunTestSequence(
      // clang-format off
    SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                            kActivateSurfaceIncompatibilityNotice),
    InitializeWithOpenGlicWindow(),
    AddInstrumentedTab(kNewActorTabId, task_url),
    WithElement(kNewActorTabId, [this](ui::TrackedElement* el){
      content::WebContents* tab_contents =
          AsInstrumentedWebContents(el)->web_contents();
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(tab_contents);
      CHECK(tab);
      tab_handle_ = tab->GetHandle();
    }),
    // Wait for painting, so we can get the screenshot. This is needed to avoid
    // flakes on Mac OS.
    WaitForFrameSubmitted(kNewActorTabId),

    // Minimize the window and wait until it is minimized.
    Steps(Do([this](){
      browser()->window()->Minimize();
    })),
    PollState(kIsMinimizedState, [this]() {
      return browser()->window()->IsMinimized();
    }),
    WaitForState(kIsMinimizedState, true),

    CreateTask(task_id_, ""),
    // Try to get the screenshot when the window is minimized.
    GetPageContextForActorTab(),
    Steps(Do(base::BindLambdaForTesting([this](){
      EXPECT_TRUE(annotated_page_content_);
      EXPECT_TRUE(viewport_screenshot_);
    }))));
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(GlicActorUiTest, ScreenshotInMinimizedWindowWithFloaty) {
  EnableScreenshotsInContext();
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kIsMinimizedState);

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  // Ensure a new tab can be created without crashing when the most recently
  // focused browser window is not a normal tabbed browser (e.g. a DevTools
  // window).
  TrackFloatingGlicInstance();
  RunTestSequence(
      // clang-format off
    SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                            kActivateSurfaceIncompatibilityNotice),
    InstrumentTab(kNewActorTabId),
    NavigateWebContents(kNewActorTabId, task_url),
    OpenGlicFloatingWindow(),
    WaitForWebContentsReady(kNewActorTabId),
    WithElement(kNewActorTabId, [this](ui::TrackedElement* el){
      content::WebContents* tab_contents =
          AsInstrumentedWebContents(el)->web_contents();
      tabs::TabInterface* tab =
          tabs::TabInterface::GetFromContents(tab_contents);
      CHECK(tab);
      tab_handle_ = tab->GetHandle();
    }),
    CreateTask(task_id_, ""),

    // Wait for painting, so we can get the screenshot.
    WaitForFrameSubmitted(kNewActorTabId),
    // Verify we can get the screenshot.
    GetPageContextForActorTab(),
    Steps(Do(base::BindLambdaForTesting([this](){
      EXPECT_TRUE(annotated_page_content_);
      EXPECT_TRUE(viewport_screenshot_);
      annotated_page_content_.reset();
      viewport_screenshot_.reset();
    }))),

    // // Minimize the window and wait until it is minimized.
    Steps(Do([this](){
      browser()->window()->Minimize();
    })),
    PollState(kIsMinimizedState, [this]() {
      return browser()->window()->IsMinimized();
    }),
    WaitForState(kIsMinimizedState, true),

    // Try to get the screenshot when the window is minimized.
    GetPageContextForActorTab(),
    Steps(Do(base::BindLambdaForTesting([this](){
      EXPECT_TRUE(annotated_page_content_);
      EXPECT_TRUE(viewport_screenshot_);
    }))));
  // clang-format on
}

}  // namespace

}  // namespace glic::test
