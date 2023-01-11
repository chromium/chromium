// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class SnapGroupTest : public AshTestBase {
 public:
  SnapGroupTest() = default;
  SnapGroupTest(const SnapGroupTest&) = delete;
  SnapGroupTest& operator=(const SnapGroupTest&) = delete;
  ~SnapGroupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kSnapGroup);
    AshTestBase::SetUp();
  }

  void SnapTwoTestWindows(aura::Window* primary_window,
                          aura::Window* secondary_window) {
    UpdateDisplay("800x700");

    WindowState* primary_window_state = WindowState::Get(primary_window);
    const WMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
    primary_window_state->OnWMEvent(&snap_primary);
    EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
              primary_window_state->GetStateType());

    WindowState* secondary_window_state = WindowState::Get(secondary_window);
    const WMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
    secondary_window_state->OnWMEvent(&snap_secondary);
    EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
              secondary_window_state->GetStateType());

    EXPECT_EQ(0.5f, *primary_window_state->snap_ratio());
    EXPECT_EQ(0.5f, *secondary_window_state->snap_ratio());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the corresponding snap group will be created when calling
// `AddSnapGroup` and removed when calling `RemoveSnapGroup`.
TEST_F(SnapGroupTest, AddAndRemoveSnapGroupTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));
  ASSERT_FALSE(snap_group_controller->AddSnapGroup(w1.get(), w3.get()));

  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);
  const auto iter1 = window_to_snap_group_map.find(w1.get());
  ASSERT_TRUE(iter1 != window_to_snap_group_map.end());
  const auto iter2 = window_to_snap_group_map.find(w2.get());
  ASSERT_TRUE(iter2 != window_to_snap_group_map.end());
  auto* snap_group = snap_groups.back().get();
  EXPECT_EQ(iter1->second, snap_group);
  EXPECT_EQ(iter2->second, snap_group);

  ASSERT_TRUE(snap_group_controller->RemoveSnapGroup(snap_group));
  ASSERT_TRUE(snap_groups.empty());
  ASSERT_TRUE(window_to_snap_group_map.empty());
}

// Tests that the corresponding snap group will be removed when one of the
// windows in the snap group gets destroyed.
TEST_F(SnapGroupTest, WindowDestroyTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  // Destroy one window in the snap group and the entire snap group will be
  // removed.
  w1.reset();
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());
}

// Tests that if one window in the snap group is actiaved, the stacking order of
// the other window in the snap group will be updated to be right below the
// activated window i.e. the two windows in the snap group will be placed on
// top.
TEST_F(SnapGroupTest, WindowActivationTest) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AddSnapGroup(w1.get(), w2.get()));

  wm::ActivateWindow(w3.get());

  // Actiave one of the windows in the snap group.
  wm::ActivateWindow(w1.get());

  MruWindowTracker::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(std::vector<aura::Window*>({w1.get(), w3.get(), w2.get()}),
            window_list);

  // `w3` is stacked below `w2` even though the activation order of `w3` is
  // before `w2`.
  // TODO(michelefan): Keep an eye out for changes in the activation logic and
  // update this test if needed in future.
  EXPECT_TRUE(IsStackedBelow(w3.get(), w2.get()));
}

}  // namespace ash