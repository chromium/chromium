// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/app_restore/features.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class MruWindowTrackerTest : public AshTestBase {
 public:
  MruWindowTrackerTest() = default;

  MruWindowTrackerTest(const MruWindowTrackerTest&) = delete;
  MruWindowTrackerTest& operator=(const MruWindowTrackerTest&) = delete;

  ~MruWindowTrackerTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  MruWindowTracker* mru_window_tracker() {
    return Shell::Get()->mru_window_tracker();
  }
};

// Basic test that the activation order is tracked.
TEST_F(MruWindowTrackerTest, Basic) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w2.get());
  wm::ActivateWindow(w1.get());

  MruWindowTracker::WindowList window_list =
      mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  ASSERT_EQ(3u, window_list.size());
  EXPECT_EQ(w1.get(), window_list[0]);
  EXPECT_EQ(w2.get(), window_list[1]);
  EXPECT_EQ(w3.get(), window_list[2]);
}

// Tests that windows being dragged are only in the WindowList once.
TEST_F(MruWindowTrackerTest, DraggedWindowsInListOnlyOnce) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  wm::ActivateWindow(w1.get());

  // Start dragging the window.
  WindowState::Get(w1.get())->CreateDragDetails(gfx::PointF(), HTRIGHT,
                                                ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  // The dragged window should only be in the list once.
  MruWindowTracker::WindowList window_list =
      mru_window_tracker()->BuildWindowListIgnoreModal(kActiveDesk);
  EXPECT_EQ(1, std::count(window_list.begin(), window_list.end(), w1.get()));
}

class MruWindowTrackerOrderTest : public MruWindowTrackerTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  MruWindowTrackerOrderTest() {}
  ~MruWindowTrackerOrderTest() override = default;

  MruWindowTracker::WindowList BuildMruWindowList() const {
    return GetParam()
               ? Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
                     kActiveDesk)
               : Shell::Get()->mru_window_tracker()->BuildMruWindowList(
                     kActiveDesk);
  }
};

// Test basic functionalities of MruWindowTracker.
TEST_P(MruWindowTrackerOrderTest, Basic) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  std::unique_ptr<aura::Window> w3(CreateTestWindow());

  // Make w3 always on top.
  w3->SetProperty(aura::client::kZOrderingKey,
                  ui::ZOrderLevel::kFloatingWindow);
  // They're in different container.
  EXPECT_NE(w3->parent(), w1->parent());

  std::unique_ptr<aura::Window> w4(CreateTestWindow());
  std::unique_ptr<aura::Window> w5(CreateTestWindow());
  std::unique_ptr<aura::Window> w6(CreateTestWindow());

  wm::ActivateWindow(w6.get());
  wm::ActivateWindow(w5.get());
  wm::ActivateWindow(w4.get());
  wm::ActivateWindow(w3.get());
  wm::ActivateWindow(w2.get());
  wm::ActivateWindow(w1.get());

  WindowState::Get(w1.get())->Minimize();
  WindowState::Get(w4.get())->Minimize();
  WindowState::Get(w5.get())->Minimize();

  // By minimizing the first window, we activate w2 which will move it to the
  // front of the MRU queue.
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));

  MruWindowTracker::WindowList window_list = BuildMruWindowList();
  EXPECT_EQ(w2.get(), window_list[0]);
  EXPECT_EQ(w1.get(), window_list[1]);
  EXPECT_EQ(w3.get(), window_list[2]);
  EXPECT_EQ(w4.get(), window_list[3]);
  EXPECT_EQ(w5.get(), window_list[4]);
  EXPECT_EQ(w6.get(), window_list[5]);

  // Exclude 3rd window.
  w3->SetProperty(ash::kExcludeInMruKey, true);
  window_list = BuildMruWindowList();
  EXPECT_EQ(w2.get(), window_list[0]);
  EXPECT_EQ(w1.get(), window_list[1]);
  EXPECT_EQ(w4.get(), window_list[2]);
  EXPECT_EQ(w5.get(), window_list[3]);
  EXPECT_EQ(w6.get(), window_list[4]);

  auto delegate = std::make_unique<views::WidgetDelegateView>();
  delegate->SetModalType(ui::MODAL_TYPE_SYSTEM);
  std::unique_ptr<views::Widget> modal =
      CreateTestWidget(delegate.release(), kShellWindowId_Invalid);
  EXPECT_EQ(modal.get()->GetNativeView()->parent()->GetId(),
            kShellWindowId_SystemModalContainer);

  window_list = BuildMruWindowList();
  auto iter = window_list.begin();
  if (GetParam()) {
    EXPECT_EQ(w2.get(), *iter++);
    EXPECT_EQ(w1.get(), *iter++);
    EXPECT_EQ(w4.get(), *iter++);
    EXPECT_EQ(w5.get(), *iter++);
    EXPECT_EQ(w6.get(), *iter++);
  }
  EXPECT_EQ(iter, window_list.end());
}

// A test class for testing the Full Restore feature.
class MruWindowTrackerFullRestoreTest : public MruWindowTrackerTest {
 public:
  MruWindowTrackerFullRestoreTest() = default;
  MruWindowTrackerFullRestoreTest(const MruWindowTrackerFullRestoreTest&) =
      delete;
  MruWindowTrackerFullRestoreTest& operator=(
      const MruWindowTrackerFullRestoreTest&) = delete;
  ~MruWindowTrackerFullRestoreTest() override = default;

  // MruWindowTrackerTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        full_restore::features::kFullRestore);
    MruWindowTrackerTest::SetUp();
  }

  // Simulates restoring a window using Full Restore by init'ing a window with
  // the `app_restore::kActivationIndexKey`.
  std::unique_ptr<aura::Window> CreateTestFullRestoredWindow(
      int activation_index) {
    auto window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->SetProperty(app_restore::kActivationIndexKey, activation_index);
    window->Init(ui::LAYER_NOT_DRAWN);
    return window;
  }

  void VerifyMruWindowsOrder(
      const MruWindowTracker::WindowList& expected_list) {
    auto actual_list = mru_window_tracker()->GetMruWindowsForTesting();
    ASSERT_EQ(expected_list.size(), actual_list.size());
    for (size_t i = 0; i < expected_list.size(); ++i)
      EXPECT_EQ(expected_list[i], actual_list[i]);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests whether MRU order is properly restored for the Full Restore feature.
TEST_F(MruWindowTrackerFullRestoreTest, RestoreMruOrder) {
  // Simulate restoring Full Restored windows out-of-order. Start with `w5`,
  // which has an activation index of 5. The lower
  // `app_restore::kActivationIndexKey` is, the more recently it was used. Also
  // the most recently used window is at the end of
  // `MruWindowTracker::mru_windows_`.
  auto w5 = CreateTestFullRestoredWindow(/*activation_index=*/5);
  VerifyMruWindowsOrder({w5.get()});

  // Simulate restoring `w2`.
  auto w2 = CreateTestFullRestoredWindow(/*activation_index=*/2);
  VerifyMruWindowsOrder({w5.get(), w2.get()});

  // Simulate restoring `w3`.
  auto w3 = CreateTestFullRestoredWindow(/*activation_index=*/3);
  VerifyMruWindowsOrder({w5.get(), w3.get(), w2.get()});

  // Simulate a user creating a window while Full Restore is ongoing.
  auto user_created_window = CreateTestWindow();
  wm::ActivateWindow(user_created_window.get());
  VerifyMruWindowsOrder(
      {w5.get(), w3.get(), w2.get(), user_created_window.get()});

  // Simulate restoring `w4`.
  auto w4 = CreateTestFullRestoredWindow(/*activation_index=*/4);
  VerifyMruWindowsOrder(
      {w5.get(), w4.get(), w3.get(), w2.get(), user_created_window.get()});

  // Simulate restoring `w1`.
  auto w1 = CreateTestFullRestoredWindow(/*activation_index=*/1);
  VerifyMruWindowsOrder({w5.get(), w4.get(), w3.get(), w2.get(), w1.get(),
                         user_created_window.get()});
}

// Tests that Full Restore'd windows are included in the MRU window list. See
// crbug.com/1229260.
TEST_F(MruWindowTrackerFullRestoreTest, FullRestoredWindowsInMRUWindowList) {
  // Create an `aura::Window` using `CreateTestWindow()` so that the window is
  // parented to something. Then set its
  // `app_restore::kLaunchedFromFullRestoreKey` to simulate it being Full
  // Restore'd.
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->SetProperty(app_restore::kLaunchedFromFullRestoreKey, true);

  // Build the MRU window list. `w1` should be included despite not being
  // activatable.
  MruWindowTracker::WindowList window_list =
      mru_window_tracker()->BuildMruWindowList(kAllDesks);
  EXPECT_EQ(1u, window_list.size());
  EXPECT_EQ(w1.get(), window_list[0]);
}

INSTANTIATE_TEST_SUITE_P(MruWindowTrackerOrder,
                         MruWindowTrackerOrderTest,
                         /*use ignore modal=*/::testing::Bool());

}  // namespace ash
