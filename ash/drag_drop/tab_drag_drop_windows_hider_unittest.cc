// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/tab_drag_drop_windows_hider.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class TabDragDropWindowsHiderTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    dummy_window_ = CreateToplevelTestWindow();
  }

  void TearDown() override {
    dummy_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> dummy_window_;
};

// Test for crbug.com/1330038 .
TEST_F(TabDragDropWindowsHiderTest, WindowVisibilityChangedDuringDrag) {
  std::unique_ptr<aura::Window> source_window = CreateToplevelTestWindow();

  // Create a sub window and hide it.
  std::unique_ptr<aura::Window> sub_window = CreateTestWindow();
  dummy_window_->AddChild(sub_window.get());
  sub_window->Hide();
  auto hider = std::make_unique<TabDragDropWindowsHider>(source_window.get());
  int size = hider->GetWindowVisibilityMapSizeForTesting();

  // Show the sub window. Make sure the window observer list size remains the
  // same.
  sub_window->Show();
  EXPECT_EQ(size, hider->GetWindowVisibilityMapSizeForTesting());
  sub_window.reset();
}
}  // namespace ash
