// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/system/time/time_view.h"
#include "ash/test/ash_test_base.h"

namespace ash {

class TimeTrayItemViewTest : public AshTestBase {
 public:
  TimeTrayItemViewTest() = default;
  ~TimeTrayItemViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    time_tray_item_view_ = std::make_unique<TimeTrayItemView>(
        GetPrimaryShelf(), TimeView::Type::kTime);
  }

  void TearDown() override {
    time_tray_item_view_.reset();
    AshTestBase::TearDown();
  }

  // Returns true if the time view is in horizontal layout, false if it is in
  // vertical layout.
  bool IsTimeViewInHorizontalLayout() {
    return time_tray_item_view_->time_view_->horizontal_time_label_container_
        ->GetVisible();
  }

 protected:
  std::unique_ptr<TimeTrayItemView> time_tray_item_view_;
};

TEST_F(TimeTrayItemViewTest, ShelfAlignment) {
  // The tray should show time horizontal view when the shelf is bottom.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  time_tray_item_view_->UpdateAlignmentForShelf(GetPrimaryShelf());
  EXPECT_TRUE(IsTimeViewInHorizontalLayout());

  // The tray should show time vertical view when the shelf is left or right.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  time_tray_item_view_->UpdateAlignmentForShelf(GetPrimaryShelf());
  EXPECT_FALSE(IsTimeViewInHorizontalLayout());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  time_tray_item_view_->UpdateAlignmentForShelf(GetPrimaryShelf());
  EXPECT_FALSE(IsTimeViewInHorizontalLayout());

  // The tray should show time horizontal view when switching back to bottom
  // shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  time_tray_item_view_->UpdateAlignmentForShelf(GetPrimaryShelf());
  EXPECT_TRUE(IsTimeViewInHorizontalLayout());
}

}  // namespace ash
