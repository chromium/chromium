// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_detailed_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class TrayDetailedViewTest : public AshTestBase {
 public:
  TrayDetailedViewTest() {
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    detailed_view_ = std::make_unique<TrayDetailedView>(delegate_.get());
    detailed_view_->CreateScrollableList();
  }

  void CreateZeroStateView(std::unique_ptr<ZeroStateView> view) {
    detailed_view_->CreateZeroStateView(std::move(view));
  }

  void SetZeroStateViewVisibility(bool visibility) {
    detailed_view_->SetZeroStateViewVisibility(visibility);
  }

  TrayDetailedViewTest(const TrayDetailedViewTest&) = delete;
  TrayDetailedViewTest& operator=(const TrayDetailedViewTest&) = delete;

 protected:
  std::unique_ptr<TrayDetailedView> detailed_view_;
  std::unique_ptr<FakeDetailedViewDelegate> delegate_;
};

TEST_F(TrayDetailedViewTest, CreateZeroStateView) {
  auto zero_state_view = std::make_unique<ZeroStateView>(
      IDR_MAHI_GENERAL_ERROR_STATUS_IMAGE,
      IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_TITLE,
      IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_SUBTITLE);
  auto zero_state_view_ptr = zero_state_view.get();
  const int scroller_index = detailed_view_
                                 ->GetIndexOf(static_cast<views::View*>(
                                     detailed_view_->scroll_view_for_testing()))
                                 .value();

  CreateZeroStateView((std::move(zero_state_view)));

  EXPECT_EQ(detailed_view_->zero_state_view_for_testing(), zero_state_view_ptr);
  const int zero_state_view_index =
      detailed_view_->GetIndexOf(detailed_view_->zero_state_view_for_testing())
          .value();
  EXPECT_EQ(zero_state_view_index, scroller_index);
  EXPECT_FALSE(zero_state_view_ptr->GetVisible());
}

TEST_F(TrayDetailedViewTest, SetZeroStateViewVisibility) {
  auto zero_state_view = std::make_unique<ZeroStateView>(
      IDR_MAHI_GENERAL_ERROR_STATUS_IMAGE,
      IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_TITLE,
      IDS_ASH_STATUS_TRAY_CAST_ZERO_STATE_SUBTITLE);

  auto zero_state_view_ptr = zero_state_view.get();

  CreateZeroStateView(std::move(zero_state_view));

  SetZeroStateViewVisibility(true);
  EXPECT_TRUE(zero_state_view_ptr->GetVisible());
  EXPECT_FALSE(detailed_view_->scroll_view_for_testing()->GetVisible());

  SetZeroStateViewVisibility(false);
  EXPECT_FALSE(zero_state_view_ptr->GetVisible());
  EXPECT_TRUE(detailed_view_->scroll_view_for_testing()->GetVisible());

  SetZeroStateViewVisibility(true);
  EXPECT_TRUE(zero_state_view_ptr->GetVisible());
  EXPECT_FALSE(detailed_view_->scroll_view_for_testing()->GetVisible());
}
}  // namespace ash
