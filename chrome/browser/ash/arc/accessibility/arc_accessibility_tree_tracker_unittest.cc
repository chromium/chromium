// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_tree_tracker.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_helper_instance_remote_proxy.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface_util.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "services/accessibility/android/test/android_accessibility_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"

namespace arc {

class ArcAccessibilityTreeTrackerTest : public ChromeViewsTestBase {
 public:
  class TestArcAccessibilityTreeTracker : public ArcAccessibilityTreeTracker {
   public:
    TestArcAccessibilityTreeTracker(
        Profile* const profile,
        const AccessibilityHelperInstanceRemoteProxy&
            accessibility_helper_instance,
        ArcBridgeService* const arc_bridge_service)
        : ArcAccessibilityTreeTracker(nullptr,
                                      profile,
                                      accessibility_helper_instance,
                                      arc_bridge_service) {}

    raw_ptr<aura::Window, DanglingUntriaged> focused_window_ = nullptr;
    std::optional<bool> last_dispatched_talkback_state_;

    void TrackWindow(aura::Window* window) {
      ArcAccessibilityTreeTracker::TrackWindow(window);
    }

   private:
    aura::Window* GetFocusedArcWindow() const override {
      return focused_window_;
    }

    void DispatchCustomSpokenFeedbackToggled(bool enabled) override {
      last_dispatched_talkback_state_ = enabled;
    }
  };

  ArcAccessibilityTreeTrackerTest()
      : helper_instance_(&bridge_service_),
        tree_tracker_(&testing_profile_, helper_instance_, &bridge_service_) {}

  TestArcAccessibilityTreeTracker& accessibility_tree_tracker() {
    return tree_tracker_;
  }

  std::unique_ptr<aura::Window> CreateWindow(
      chromeos::AppType app_type = chromeos::AppType::ARC_APP) {
    auto window = std::make_unique<aura::Window>(nullptr);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->SetProperty(chromeos::kAppTypeKey, app_type);
    return window;
  }

 private:
  TestingProfile testing_profile_;
  ArcBridgeService bridge_service_;
  const AccessibilityHelperInstanceRemoteProxy helper_instance_;
  TestArcAccessibilityTreeTracker tree_tracker_;
};

TEST_F(ArcAccessibilityTreeTrackerTest, TaskAndAXTreeLifecycle) {
  auto& tree_tracker = accessibility_tree_tracker();
  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::ALL);

  std::unique_ptr<aura::Window> test_window = CreateWindow();
  tree_tracker.TrackWindow(test_window.get());

  const auto& key_to_tree = tree_tracker.trees_for_test();
  ASSERT_EQ(0U, key_to_tree.size());

  auto event1 = ax::android::mojom::AccessibilityEventData::New();
  event1->source_id = 1;
  event1->task_id = 1;

  // There's no window matches to the event.
  ax::android::AXTreeSourceAndroid* tree =
      tree_tracker.OnAccessibilityEvent(event1.Clone().get());
  ASSERT_EQ(nullptr, tree);
  ASSERT_EQ(0U, key_to_tree.size());

  // Let's set task id to the window.
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");
  tree = tree_tracker.OnAccessibilityEvent(event1.Clone().get());
  ASSERT_NE(nullptr, tree);
  ASSERT_EQ(1U, key_to_tree.size());

  // Event from a different task.
  auto event2 = ax::android::mojom::AccessibilityEventData::New();
  event2->source_id = 2;
  event2->task_id = 2;

  // There's only task 1 window
  tree = tree_tracker.OnAccessibilityEvent(event2.Clone().get());
  ASSERT_EQ(nullptr, tree);
  ASSERT_EQ(1U, key_to_tree.size());

  // Now add a window for task 2.
  std::unique_ptr<aura::Window> test_window2 = CreateWindow();
  tree_tracker.TrackWindow(test_window2.get());

  exo::SetShellApplicationId(test_window2.get(), "org.chromium.arc.2");
  tree = tree_tracker.OnAccessibilityEvent(event2.Clone().get());
  ASSERT_NE(nullptr, tree);
  ASSERT_EQ(2U, key_to_tree.size());

  // Same task id, different source node.
  event2->source_id = 3;

  // No new tasks tree mappings should have occurred.
  tree = tree_tracker.OnAccessibilityEvent(event2.Clone().get());
  ASSERT_NE(nullptr, tree);
  ASSERT_EQ(2U, key_to_tree.size());

  tree_tracker.OnWindowDestroying(test_window.get());
  ASSERT_EQ(1U, key_to_tree.size());

  tree_tracker.OnWindowDestroying(test_window2.get());
  ASSERT_EQ(0U, key_to_tree.size());
}

TEST_F(ArcAccessibilityTreeTrackerTest, ReEnableTree) {
  auto& tree_tracker = accessibility_tree_tracker();
  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::ALL);

  std::unique_ptr<aura::Window> test_window = CreateWindow();
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");
  tree_tracker.TrackWindow(test_window.get());

  std::unique_ptr<aura::Window> child_window =
      CreateWindow(chromeos::AppType::NON_APP);
  exo::SetShellClientAccessibilityId(child_window.get(), 10);
  test_window->AddChild(child_window.get());

  const auto& key_to_tree = tree_tracker.trees_for_test();
  ASSERT_EQ(1U, key_to_tree.size());

  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->source_id = 1;
  event->task_id = kNoTaskId;
  event->window_id = 10;

  // On the event, tree is tracked.
  ax::android::AXTreeSourceAndroid* tree =
      tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_NE(nullptr, tree);

  // Disables accessibility, and no tree is tracked.
  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::OFF);

  ASSERT_EQ(0U, key_to_tree.size());

  // Enables accessibility again, and tree is tracked.
  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::ALL);
  tree_tracker.TrackWindow(test_window.get());
  tree = tree_tracker.OnAccessibilityEvent(event.Clone().get());

  ASSERT_EQ(1U, key_to_tree.size());
  ASSERT_NE(nullptr, tree);
}

TEST_F(ArcAccessibilityTreeTrackerTest, WindowIdTaskIdMapping) {
  auto& tree_tracker = accessibility_tree_tracker();
  std::unique_ptr<aura::Window> test_window = CreateWindow();

  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::ALL);

  const auto& key_to_tree = tree_tracker.trees_for_test();
  ASSERT_EQ(0U, key_to_tree.size());

  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->source_id = 1;
  event->task_id = kNoTaskId;
  event->window_id = 10;

  // There's no tracking window.
  tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_EQ(0U, key_to_tree.size());

  // Set task ID 1 to the window.
  // Also, set a11y window id to a child window.
  tree_tracker.TrackWindow(test_window.get());
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");

  std::unique_ptr<aura::Window> child_window1 =
      CreateWindow(chromeos::AppType::NON_APP);
  exo::SetShellClientAccessibilityId(child_window1.get(), 10);
  test_window->AddChild(child_window1.get());

  ax::android::AXTreeSourceAndroid* tree1 =
      tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_NE(nullptr, tree1);
  ASSERT_EQ(1U, key_to_tree.size());

  // Add another child window with different id. (and call AddChild first.)
  std::unique_ptr<aura::Window> child_window2 =
      CreateWindow(chromeos::AppType::NON_APP);
  test_window->AddChild(child_window2.get());
  exo::SetShellClientAccessibilityId(child_window2.get(), 11);

  event->window_id = 11;

  ax::android::AXTreeSourceAndroid* tree2 =
      tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_NE(nullptr, tree2);
  ASSERT_EQ(1U, key_to_tree.size());
  ASSERT_EQ(tree1, tree2);  // The same tree.

  // Put the window id back to 10, but exo window id is not updated yet.
  // This emulates the case where a mojo events arrives before exo property.
  event->window_id = 10;

  ax::android::AXTreeSourceAndroid* tree3 =
      tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_NE(nullptr, tree3);
  ASSERT_EQ(1U, key_to_tree.size());
  ASSERT_EQ(tree1, tree3);

  // Another task.
  auto event2 = ax::android::mojom::AccessibilityEventData::New();
  event2->source_id = 10;
  event2->task_id = kNoTaskId;
  event2->window_id = 20;

  std::unique_ptr<aura::Window> another_window = CreateWindow();
  exo::SetShellApplicationId(another_window.get(), "org.chromium.arc.2");
  std::unique_ptr<aura::Window> another_child_window =
      CreateWindow(chromeos::AppType::NON_APP);
  exo::SetShellClientAccessibilityId(another_child_window.get(), 20);
  another_window->AddChild(another_child_window.get());

  tree_tracker.TrackWindow(another_window.get());

  ax::android::AXTreeSourceAndroid* tree4 =
      tree_tracker.OnAccessibilityEvent(event2.Clone().get());
  ASSERT_NE(nullptr, tree4);
  ASSERT_EQ(2U, key_to_tree.size());
  ASSERT_NE(tree1, tree4);
}

TEST_F(ArcAccessibilityTreeTrackerTest, TrackArcGhostWindow) {
  auto& tree_tracker = accessibility_tree_tracker();

  tree_tracker.OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType::ALL);

  // Simulate a ghost window. Apply NON_APP type and session ID.
  std::unique_ptr<aura::Window> test_window =
      CreateWindow(chromeos::AppType::NON_APP);
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.session.1");
  tree_tracker.TrackWindow(test_window.get());

  const auto& key_to_tree = tree_tracker.trees_for_test();
  ASSERT_EQ(0U, key_to_tree.size());

  auto event = ax::android::mojom::AccessibilityEventData::New();
  event->source_id = 1;
  event->task_id = kNoTaskId;
  event->window_id = 10;

  // The window properties are not updated yet. a11y event is ignored.
  tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_EQ(0U, key_to_tree.size());

  // A ghost window is replaced with an actual ARC window.
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");
  test_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

  std::unique_ptr<aura::Window> child_window =
      CreateWindow(chromeos::AppType::NON_APP);
  exo::SetShellClientAccessibilityId(child_window.get(), 10);
  test_window->AddChild(child_window.get());

  tree_tracker.OnAccessibilityEvent(event.Clone().get());
  ASSERT_EQ(1U, key_to_tree.size());
}

TEST_F(ArcAccessibilityTreeTrackerTest, FilterTypeChange) {
  using ax::android::mojom::AccessibilityFilterType;

  auto& tree_tracker = accessibility_tree_tracker();

  std::unique_ptr<aura::Window> test_window = CreateWindow();
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");
  exo::SetShellClientAccessibilityId(test_window.get(), 10);

  const auto& key_to_tree = tree_tracker.trees_for_test();
  ASSERT_EQ(0U, key_to_tree.size());

  tree_tracker.OnEnabledFeatureChanged(AccessibilityFilterType::ALL);
  tree_tracker.TrackWindow(test_window.get());
  ASSERT_EQ(1U, key_to_tree.size());

  // Changing from ALL to OFF should result in existing trees being destroyed.
  tree_tracker.OnEnabledFeatureChanged(AccessibilityFilterType::OFF);
  ASSERT_EQ(0U, key_to_tree.size());

  // Changing from OFF to FOCUS should not result in any changes.
  tree_tracker.OnEnabledFeatureChanged(AccessibilityFilterType::FOCUS);
  ASSERT_EQ(0U, key_to_tree.size());

  // Changing from FOCUS to ALL should not result in a existing tree recognized.
  tree_tracker.OnEnabledFeatureChanged(AccessibilityFilterType::ALL);
  tree_tracker.TrackWindow(test_window.get());
  ASSERT_EQ(1U, key_to_tree.size());

  // Changing from ALL to FOCUS should not result in any changes.
  tree_tracker.OnEnabledFeatureChanged(AccessibilityFilterType::FOCUS);
  ASSERT_EQ(0U, key_to_tree.size());
}

TEST_F(ArcAccessibilityTreeTrackerTest, ToggleTalkBack) {
  auto& tree_tracker = accessibility_tree_tracker();

  std::unique_ptr<aura::Window> test_window = CreateWindow();
  tree_tracker.focused_window_ = test_window.get();
  exo::SetShellApplicationId(test_window.get(), "org.chromium.arc.1");

  std::optional<bool>& last_state =
      tree_tracker.last_dispatched_talkback_state_;

  ASSERT_FALSE(last_state.has_value());

  // Enable TalkBack.
  last_state.reset();
  std::unique_ptr<aura::WindowTracker> window_tracker =
      std::make_unique<aura::WindowTracker>();
  window_tracker->Add(test_window.get());
  tree_tracker.OnToggleNativeChromeVoxArcSupport(false);

  ASSERT_TRUE(last_state.value());

  std::unique_ptr<aura::Window> non_arc_window =
      CreateWindow(chromeos::AppType::NON_APP);

  // Switch to non-ARC window.
  last_state.reset();
  tree_tracker.OnWindowFocused(non_arc_window.get(), test_window.get());
  ASSERT_FALSE(last_state.value());

  // Switch back to ARC.
  last_state.reset();
  tree_tracker.OnWindowFocused(test_window.get(), non_arc_window.get());
  ASSERT_TRUE(last_state.value());

  // Disable TalkBack.
  last_state.reset();
  window_tracker = std::make_unique<aura::WindowTracker>();
  window_tracker->Add(test_window.get());
  tree_tracker.OnToggleNativeChromeVoxArcSupport(true);
  ASSERT_FALSE(last_state.value());
}

}  // namespace arc
