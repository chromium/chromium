// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <memory>
#include <utility>

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/exo_app_type_resolver.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/exo/buffer.h"
#include "components/exo/client_controlled_accelerators.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-shared.h"
#include "services/accessibility/android/test/fake_accessibility_helper_instance.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

using ::ash::AccessibilityManager;

namespace {

class MockAutomationEventRouter
    : public extensions::AutomationEventRouterInterface {
 public:
  MockAutomationEventRouter() = default;
  virtual ~MockAutomationEventRouter() = default;

  // extensions::AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override {}

  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      const ui::AXLocationChange& details) override {}

  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) override {}

  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override {
    last_dispatched_action_data_ = data;
    last_dispatched_action_result_ = result;
  }

  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& rect) override {
    last_dispatched_action_data_ = data;
    last_dispatched_text_location_ = rect;
  }

  std::optional<ui::AXActionData> last_dispatched_action_data_;
  std::optional<bool> last_dispatched_action_result_;
  std::optional<gfx::Rect> last_dispatched_text_location_;
};

}  // namespace

class ArcAccessibilityHelperBridgeBrowserTest : public InProcessBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    fake_accessibility_helper_instance_ =
        std::make_unique<FakeAccessibilityHelperInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->SetInstance(fake_accessibility_helper_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->accessibility_helper());

    AccessibilityManager::Get()->SetProfileForTest(browser()->profile());

    wm_helper_ = std::make_unique<exo::WMHelper>();
    wm_helper_->RegisterAppPropertyResolver(
        std::make_unique<ExoAppTypeResolver>());
  }

  void TearDownOnMainThread() override {
    wm_helper_.reset();

    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->CloseInstance(fake_accessibility_helper_instance_.get());
    fake_accessibility_helper_instance_.reset();
  }

 protected:
  // Create and initialize a window for this test, i.e. an Arc++-specific
  // version of ExoTestHelper::CreateWindow.
  std::unique_ptr<exo::ClientControlledShellSurface> MakeTestArcWindow(
      std::string name) {
    return exo::test::ShellSurfaceBuilder({640, 480})
        .SetApplicationId(name)
        .BuildClientControlledShellSurface();
  }

  std::unique_ptr<exo::ShellSurface> MakeNonArcTestWindow() {
    return exo::test::ShellSurfaceBuilder({640, 480}).BuildShellSurface();
  }

  std::unique_ptr<FakeAccessibilityHelperInstance>
      fake_accessibility_helper_instance_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       PreferenceChange) {
  ASSERT_EQ(ax::android::mojom::AccessibilityFilterType::OFF,
            fake_accessibility_helper_instance_->filter_type());
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  auto shell_surface1 = MakeTestArcWindow("org.chromium.arc.1");
  auto shell_surface2 = MakeTestArcWindow("org.chromium.arc.2");

  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();
  activation_client->ActivateWindow(
      shell_surface1->GetWidget()->GetNativeWindow());
  ASSERT_EQ(shell_surface1->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  ASSERT_FALSE(shell_surface1->GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));
  ASSERT_FALSE(shell_surface2->GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Confirm that filter type is updated with preference change.
  EXPECT_EQ(ax::android::mojom::AccessibilityFilterType::ALL,
            fake_accessibility_helper_instance_->filter_type());

  // Use ChromeVox by default. Touch exploration pass through is still false.
  EXPECT_FALSE(shell_surface1->GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());

  // Enable TalkBack. Touch exploration pass through of shell_surface1
  // (current active window) would become true.
  bridge->SetNativeChromeVoxArcSupport(
      false,
      base::BindOnce(
          [](extensions::api::accessibility_private::SetNativeChromeVoxResponse
                 response) {}));

  EXPECT_TRUE(shell_surface1->GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  // Activate shell_surface2 and confirm that it still be false.
  activation_client->ActivateWindow(
      shell_surface2->GetWidget()->GetNativeWindow());
  ASSERT_EQ(shell_surface2->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  EXPECT_FALSE(shell_surface2->GetWidget()->GetNativeWindow()->GetProperty(
      aura::client::kAccessibilityTouchExplorationPassThrough));

  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       RequestTreeSyncOnWindowIdChange) {
  auto shell_surface1 = MakeTestArcWindow("org.chromium.arc.1");
  aura::Window* window1 = shell_surface1->GetWidget()->GetNativeWindow();
  aura::Window child_window1 = aura::Window(nullptr);
  child_window1.Init(ui::LAYER_NOT_DRAWN);
  window1->AddChild(&child_window1);

  auto shell_surface2 = MakeTestArcWindow("org.chromium.arc.2");
  aura::Window* window2 = shell_surface2->GetWidget()->GetNativeWindow();
  aura::Window child_window2 = aura::Window(nullptr);
  child_window2.Init(ui::LAYER_NOT_DRAWN);
  window2->AddChild(&child_window2);

  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();
  activation_client->ActivateWindow(window1);

  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  exo::SetShellClientAccessibilityId(&child_window1, 10);

  EXPECT_TRUE(
      fake_accessibility_helper_instance_->last_requested_tree_window_key()
          ->is_window_id());
  EXPECT_EQ(
      10U, fake_accessibility_helper_instance_->last_requested_tree_window_key()
               ->get_window_id());

  exo::SetShellClientAccessibilityId(&child_window2, 20);

  EXPECT_EQ(
      20U, fake_accessibility_helper_instance_->last_requested_tree_window_key()
               ->get_window_id());

  exo::SetShellClientAccessibilityId(&child_window2, 21);

  EXPECT_EQ(
      21U, fake_accessibility_helper_instance_->last_requested_tree_window_key()
               ->get_window_id());
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       ExploreByTouchMode) {
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  // Check that explore by touch doesn't get disabled as long as ChromeVox
  // remains enabled.
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());

  AccessibilityManager::Get()->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(fake_accessibility_helper_instance_->explore_by_touch_enabled());
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       FocusHighlight) {
  AccessibilityManager::Get()->SetFocusHighlightEnabled(true);

  auto shell_surface = MakeTestArcWindow("org.chromium.arc.1");
  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();
  activation_client->ActivateWindow(
      shell_surface->GetWidget()->GetNativeWindow());

  const gfx::Rect node_rect1 = gfx::Rect(50, 50, 50, 50);

  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->event_type = ax::android::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event->task_id = 1;
  event->node_data.push_back(
      ax::android::mojom::AccessibilityNodeInfoData::New());
  auto& node = event->node_data.back();
  node->bounds_in_screen = node_rect1;

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());
  bridge->OnAccessibilityEvent(event.Clone());

  ash::AccessibilityFocusRingControllerImpl* ring_controller =
      ash::Shell::Get()->accessibility_focus_ring_controller();
  const std::vector<std::unique_ptr<ash::AccessibilityFocusRingLayer>>&
      layers1 =
          ring_controller->GetFocusRingGroupForTesting("HighlightController")
              ->focus_layers_for_testing();
  EXPECT_EQ(1u, layers1.size());
  // Focus ring bounds has some extra margin, so only check some attributes.
  const gfx::Rect drawn_rect1 = layers1[0]->layer()->bounds();
  EXPECT_EQ(node_rect1.CenterPoint(), drawn_rect1.CenterPoint());
  EXPECT_TRUE(drawn_rect1.Contains(node_rect1))
      << "drawn_rect " << drawn_rect1.ToString()
      << " should contain the given rect " << node_rect1.ToString();

  // Next, set the filter type to ALL by enabling Select-to-Speak.
  // We still expect that the focus highlight is updated on a focus event.
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);

  const gfx::Rect node_rect2 = gfx::Rect(100, 100, 100, 100);

  // Create a full event data.
  event->source_id = 10;
  event->window_data =
      std::vector<ax::android::mojom::AccessibilityWindowInfoDataPtr>();
  event->window_data->push_back(
      ax::android::mojom::AccessibilityWindowInfoData::New());
  auto& window = event->window_data->back();
  window->window_id = 100;
  window->root_node_id = 10;
  node->id = 10;
  node->bounds_in_screen = node_rect2;

  bridge->OnAccessibilityEvent(event.Clone());

  const std::vector<std::unique_ptr<ash::AccessibilityFocusRingLayer>>&
      layers2 =
          ring_controller->GetFocusRingGroupForTesting("HighlightController")
              ->focus_layers_for_testing();
  EXPECT_EQ(1u, layers2.size());
  // Focus ring bounds has some extra margin, so only check some attributes.
  const gfx::Rect drawn_rect2 = layers2[0]->layer()->bounds();
  EXPECT_EQ(node_rect2.CenterPoint(), drawn_rect2.CenterPoint());
  EXPECT_TRUE(drawn_rect2.Contains(node_rect2))
      << "drawn_rect " << drawn_rect2.ToString()
      << " should contain the given rect " << node_rect2.ToString();
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest, PerformAction) {
  auto shell_surface = MakeTestArcWindow("org.chromium.arc.1");
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());
  auto& tree_map = bridge->trees_for_test();
  ASSERT_EQ(1u, tree_map.size());
  ax::android::AXTreeSourceAndroid* tree_source =
      tree_map.begin()->second.get();
  MockAutomationEventRouter router;
  tree_source->set_automation_event_router_for_test(&router);
  tree_source->set_window_id_for_test(5);

  ui::AXActionData action_data;
  action_data.target_node_id = 10;
  action_data.target_tree_id = tree_source->ax_tree_id();
  action_data.action = ax::mojom::Action::kDoDefault;
  bridge->OnAction(action_data);

  ax::android::mojom::AccessibilityActionData* requested_action =
      fake_accessibility_helper_instance_->last_requested_action();
  EXPECT_EQ(10, requested_action->node_id);
  EXPECT_EQ(ax::android::mojom::AccessibilityActionType::CLICK,
            requested_action->action_type);
  EXPECT_EQ(5, requested_action->window_id);

  ui::AXActionData dispatched_action =
      router.last_dispatched_action_data_.value();
  EXPECT_EQ(10, dispatched_action.target_node_id);
  EXPECT_EQ(tree_source->ax_tree_id(), dispatched_action.target_tree_id);
  EXPECT_EQ(ax::mojom::Action::kDoDefault, dispatched_action.action);

  EXPECT_TRUE(router.last_dispatched_action_result_.has_value());
  EXPECT_TRUE(router.last_dispatched_action_result_.value());

  // Clear event router to prevent invalid access.
  tree_source->set_automation_event_router_for_test(nullptr);
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       PerformActionWithParams) {
  auto shell_surface = MakeTestArcWindow("org.chromium.arc.1");
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());
  auto& tree_map = bridge->trees_for_test();
  ASSERT_EQ(1u, tree_map.size());
  ax::android::AXTreeSourceAndroid* tree_source =
      tree_map.begin()->second.get();
  MockAutomationEventRouter router;
  tree_source->set_automation_event_router_for_test(&router);
  tree_source->set_window_id_for_test(5);

  ui::AXActionData action_data;
  action_data.target_node_id = 10;
  action_data.target_tree_id = tree_source->ax_tree_id();
  action_data.action = ax::mojom::Action::kScrollToPositionAtRowColumn;
  action_data.row_column = {1, 3};
  bridge->OnAction(action_data);

  ax::android::mojom::AccessibilityActionData* requested_action =
      fake_accessibility_helper_instance_->last_requested_action();
  EXPECT_EQ(10, requested_action->node_id);
  EXPECT_EQ(ax::android::mojom::AccessibilityActionType::SCROLL_TO_POSITION,
            requested_action->action_type);
  EXPECT_EQ(5, requested_action->window_id);
  EXPECT_EQ(1, requested_action->int_parameters->at(
                   ax::android::mojom::ActionIntArgumentType::ROW_INT));
  EXPECT_EQ(3, requested_action->int_parameters->at(
                   ax::android::mojom::ActionIntArgumentType::COLUMN_INT));

  ui::AXActionData dispatched_action =
      router.last_dispatched_action_data_.value();
  EXPECT_EQ(10, dispatched_action.target_node_id);
  EXPECT_EQ(tree_source->ax_tree_id(), dispatched_action.target_tree_id);
  EXPECT_EQ(ax::mojom::Action::kScrollToPositionAtRowColumn,
            dispatched_action.action);

  EXPECT_TRUE(router.last_dispatched_action_result_.has_value());
  EXPECT_TRUE(router.last_dispatched_action_result_.value());

  // Clear event router to prevent invalid access.
  tree_source->set_automation_event_router_for_test(nullptr);
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       GetTextLocation) {
  auto shell_surface = MakeTestArcWindow("org.chromium.arc.1");
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);

  ArcAccessibilityHelperBridge* bridge =
      ArcAccessibilityHelperBridge::GetForBrowserContext(browser()->profile());
  auto& tree_map = bridge->trees_for_test();
  ASSERT_EQ(1u, tree_map.size());
  ax::android::AXTreeSourceAndroid* tree_source =
      tree_map.begin()->second.get();
  MockAutomationEventRouter router;
  tree_source->set_automation_event_router_for_test(&router);
  tree_source->set_window_id_for_test(5);

  ui::AXActionData action_data;
  action_data.target_node_id = 10;
  action_data.target_tree_id = tree_source->ax_tree_id();
  action_data.action = ax::mojom::Action::kGetTextLocation;
  action_data.start_index = 3;
  action_data.end_index = 6;
  bridge->OnAction(action_data);

  ax::android::mojom::AccessibilityActionData* requested_action =
      fake_accessibility_helper_instance_->last_requested_action();
  fake_accessibility_helper_instance_->refresh_with_extra_data_callback().Run(
      gfx::Rect(10, 20, 30, 40));

  EXPECT_EQ(10, requested_action->node_id);
  EXPECT_EQ(ax::android::mojom::AccessibilityActionType::GET_TEXT_LOCATION,
            requested_action->action_type);
  EXPECT_EQ(5, requested_action->window_id);
  EXPECT_EQ(3, requested_action->start_index);
  EXPECT_EQ(6, requested_action->end_index);

  EXPECT_TRUE(router.last_dispatched_text_location_.has_value());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40),
            router.last_dispatched_text_location_.value());

  // General action path should not be used.
  EXPECT_FALSE(router.last_dispatched_action_result_.has_value());

  // Clear event router to prevent invalid access.
  tree_source->set_automation_event_router_for_test(nullptr);
}

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest, Histogram) {
  base::HistogramTester histogram_tester;

  // Prepare ARC and non-ARC windows
  auto arc_shell_surface = MakeTestArcWindow("org.chromium.arc.1");
  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();

  auto non_arc_shell_surface = MakeNonArcTestWindow();

  // Turn on and off a feature while an ARC window is focused and then it will
  // be counted.
  activation_client->ActivateWindow(
      arc_shell_surface->GetWidget()->GetNativeWindow());
  histogram_tester.ExpectBucketCount("Arc.Accessibility.WindowCount", 1, 0);
  ash::MagnificationManager::Get()->SetMagnifierEnabled(true);
  histogram_tester.ExpectBucketCount("Arc.Accessibility.WindowCount", 1, 1);
  EXPECT_EQ(ax::android::mojom::AccessibilityFilterType::ALL,
            fake_accessibility_helper_instance_->filter_type());
  histogram_tester.ExpectTotalCount(
      "Arc.Accessibility.ActiveTime.FullscreenMagnifier", 0);
  ash::MagnificationManager::Get()->SetMagnifierEnabled(false);
  histogram_tester.ExpectTotalCount(
      "Arc.Accessibility.ActiveTime.FullscreenMagnifier", 1);

  // Focus on an ARC window and focus on a non-ARC window while a feature is on
  // and then it will be counted.
  activation_client->ActivateWindow(
      non_arc_shell_surface->GetWidget()->GetNativeWindow());
  ash::MagnificationManager::Get()->SetMagnifierEnabled(true);
  histogram_tester.ExpectBucketCount("Arc.Accessibility.WindowCount", 1, 2);
  activation_client->ActivateWindow(
      arc_shell_surface->GetWidget()->GetNativeWindow());
  histogram_tester.ExpectTotalCount(
      "Arc.Accessibility.ActiveTime.FullscreenMagnifier", 1);
  activation_client->ActivateWindow(
      non_arc_shell_surface->GetWidget()->GetNativeWindow());
  histogram_tester.ExpectTotalCount(
      "Arc.Accessibility.ActiveTime.FullscreenMagnifier", 2);

  // Close the focused ARC window while a feature is on and then it will be
  // counted.
  activation_client->ActivateWindow(
      arc_shell_surface->GetWidget()->GetNativeWindow());
  arc_shell_surface.reset();
  histogram_tester.ExpectTotalCount(
      "Arc.Accessibility.ActiveTime.FullscreenMagnifier", 3);

  histogram_tester.ExpectBucketCount("Arc.Accessibility.WindowCount", 1, 2);
}

}  // namespace arc
