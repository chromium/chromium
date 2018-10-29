// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_shelf_item_delegate.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"

namespace ash {
namespace {

class AppListShelfItemDelegateTest : public AshTestBase {
 public:
  AppListShelfItemDelegateTest()
      : delegate_(std::make_unique<AppListShelfItemDelegate>()) {}
  ~AppListShelfItemDelegateTest() override = default;

  std::unique_ptr<aura::Window> CreateTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  }

  std::unique_ptr<aura::Window> CreatePopupTestWindow() {
    return AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400),
                                         aura::client::WINDOW_TYPE_POPUP);
  }

  AppListShelfItemDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<AppListShelfItemDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListShelfItemDelegateTest);
};

TEST_F(AppListShelfItemDelegateTest, OnlyMinimizeCycleListWindows) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  std::unique_ptr<aura::Window> w2(CreatePopupTestWindow());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  std::unique_ptr<ui::Event> test_event = std::make_unique<ui::KeyEvent>(
      ui::EventType::ET_MOUSE_PRESSED, ui::VKEY_UNKNOWN, ui::EF_NONE);
  delegate()->ItemSelected(
      std::move(test_event), GetPrimaryDisplay().id(),
      ShelfLaunchSource::LAUNCH_FROM_UNKNOWN,
      base::BindOnce(
          [](ash::ShelfAction, base::Optional<ash::MenuItemList>) {}));
  ASSERT_TRUE(wm::GetWindowState(w1.get())->IsMinimized());
  ASSERT_FALSE(wm::GetWindowState(w2.get())->IsMinimized());
}

}  // namespace
}  // namespace ash
