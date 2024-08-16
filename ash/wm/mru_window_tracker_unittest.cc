// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/ranges/algorithm.h"
#include "components/app_restore/window_properties.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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

  MruWindowTracker* mru_window_tracker() {
    return Shell::Get()->mru_window_tracker();
  }

  // Simulates restoring a window through window restore with the
  // `app_restore::kActivationIndexKey`.
  std::unique_ptr<aura::Window> CreateTestWindowRestoredWindow(
      int activation_index) {
    auto window = CreateTestWindow();
    window->SetProperty(app_restore::kActivationIndexKey, activation_index);
    WindowRestoreController::Get()->StackWindow(window.get());
    return window;
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
  EXPECT_EQ(1, base::ranges::count(window_list, w1.get()));
}

// Tests whether MRU order is properly restored for the window restore features.
TEST_F(MruWindowTrackerTest, RestoreMruOrder) {
  // Simulate restoring window restored windows out-of-order. Start with `w5`,
  // which has an activation index of 5. The lower
  // `app_restore::kActivationIndexKey` is, the more recently it was used. Also
  // the most recently used window is at the end of
  // `MruWindowTracker::mru_windows_`.
  auto w5 = CreateTestWindowRestoredWindow(/*activation_index=*/5);
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get()));

  // Simulate restoring `w2`.
  auto w2 = CreateTestWindowRestoredWindow(/*activation_index=*/2);
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get(), w2.get()));

  // Simulate restoring `w3`.
  auto w3 = CreateTestWindowRestoredWindow(/*activation_index=*/3);
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get(), w3.get(), w2.get()));

  // Simulate a user creating a window while Full Restore is ongoing.
  auto user_created_window = CreateTestWindow();
  wm::ActivateWindow(user_created_window.get());
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get(), w3.get(), w2.get(),
                                   user_created_window.get()));

  // Simulate restoring `w4`.
  auto w4 = CreateTestWindowRestoredWindow(/*activation_index=*/4);
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get(), w4.get(), w3.get(), w2.get(),
                                   user_created_window.get()));

  // Simulate restoring `w1`.
  auto w1 = CreateTestWindowRestoredWindow(/*activation_index=*/1);
  EXPECT_THAT(mru_window_tracker()->GetMruWindowsForTesting(),
              testing::ElementsAre(w5.get(), w4.get(), w3.get(), w2.get(),
                                   w1.get(), user_created_window.get()));
}

// Tests that window restore'd windows are included in the MRU window list. See
// https://crbug.com/1229260.
TEST_F(MruWindowTrackerTest, WindowRestoredWindowsInMruWindowList) {
  // Create an `aura::Window` using `CreateTestWindow()` so that the window is
  // parented to something. Then set its
  // `app_restore::kLaunchedFromAppRestoreKey` to simulate it being window
  // restore'd.
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->SetProperty(app_restore::kLaunchedFromAppRestoreKey, true);

  // Build the MRU window list. `w1` should be included despite not being
  // activatable.
  EXPECT_THAT(mru_window_tracker()->BuildMruWindowList(kAllDesks),
              testing::ElementsAre(w1.get()));
}

class MruWindowTrackerOrderTest : public MruWindowTrackerTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  MruWindowTrackerOrderTest() {}
  MruWindowTrackerOrderTest(const MruWindowTrackerOrderTest&) = delete;
  MruWindowTrackerOrderTest& operator=(const MruWindowTrackerOrderTest&) =
      delete;
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
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  std::unique_ptr<views::Widget> modal =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       delegate.release(), kShellWindowId_Invalid);
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

INSTANTIATE_TEST_SUITE_P(MruWindowTrackerOrder,
                         MruWindowTrackerOrderTest,
                         /*use ignore modal=*/::testing::Bool());

}  // namespace ash
