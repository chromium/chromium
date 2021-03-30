// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_tray_item_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/system/time/time_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace tray {

class TimeTrayItemViewTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  TimeTrayItemViewTest() = default;
  ~TimeTrayItemViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    std::vector<base::Feature> features = {features::kScalableStatusArea,
                                           features::kShowDateInTrayButton};
    if (IsShowDateInTrayButtonEnabled())
      scoped_feature_list_.InitWithFeatures(features, {});
    else
      scoped_feature_list_.InitWithFeatures({}, features);

    model_ = std::make_unique<UnifiedSystemTrayModel>(GetPrimaryShelf());
    time_tray_item_view_ =
        std::make_unique<TimeTrayItemView>(GetPrimaryShelf(), model_.get());
  }

  void TearDown() override {
    time_tray_item_view_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

  bool IsShowDateInTrayButtonEnabled() { return GetParam(); }

  // Returns true if the time view is in horizontal layout, false if it is in
  // vertical layout.
  bool IsTimeViewInHorizontalLayout() {
    // Time view is in horizontal layout if its subview is in use (it transfers
    // the ownership to the views hierarchy and becomes nullptr),
    return !time_tray_item_view_->time_view_->horizontal_view_;
  }

  bool ShouldShowDateInTimeView() {
    return time_tray_item_view_->time_view_->show_date_when_horizontal_;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<TimeTrayItemView> time_tray_item_view_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         TimeTrayItemViewTest,
                         testing::Bool() /* IsShowDateInTrayButtonEnabled() */);

TEST_P(TimeTrayItemViewTest, ShelfAlignment) {
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

TEST_P(TimeTrayItemViewTest, DisplayChanged) {
  UpdateDisplay("800x800");
  EXPECT_FALSE(ShouldShowDateInTimeView());

  // Date should be shown in large screen size (when the feature is enabled).
  UpdateDisplay("1680x800");
  EXPECT_EQ(IsShowDateInTrayButtonEnabled(), ShouldShowDateInTimeView());
}

}  // namespace tray
}  // namespace ash
