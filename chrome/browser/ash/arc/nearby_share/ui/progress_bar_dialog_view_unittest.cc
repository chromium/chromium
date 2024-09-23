// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/progress_bar_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ProgressBarDialogViewTest : public CompatModeTestBase {
 public:
  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    dialog_view_ = widget_->SetContentsView(
        std::make_unique<ProgressBarDialogView>(/*is_multiple_files=*/false));
    widget_->Show();
    EXPECT_TRUE(widget_->IsVisible());
  }

  void TearDown() override {
    widget_.reset();
    CompatModeTestBase::TearDown();
  }

 protected:
  raw_ptr<ProgressBarDialogView, DanglingUntriaged> dialog_view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ProgressBarDialogViewTest, UpdateAndGetValue) {
  dialog_view_->UpdateProgressBarValue(1.0);
  EXPECT_FLOAT_EQ(dialog_view_->GetProgressBarValue(), 1.0);

  // Values greater than 1.0 should be converted to -1.
  dialog_view_->UpdateProgressBarValue(1.1);
  EXPECT_FLOAT_EQ(dialog_view_->GetProgressBarValue(), -1.0);

  dialog_view_->UpdateProgressBarValue(0);
  EXPECT_FLOAT_EQ(dialog_view_->GetProgressBarValue(), 0);

  // Any invalid values should be converted to -1.
  dialog_view_->UpdateProgressBarValue(-0.1);
  EXPECT_FLOAT_EQ(dialog_view_->GetProgressBarValue(), -1.0);
}

TEST_F(ProgressBarDialogViewTest, UpdateInterpolatedValue) {
  dialog_view_->UpdateProgressBarValue(-0.1);
  EXPECT_FLOAT_EQ(dialog_view_->GetProgressBarValue(), -1.0);

  // With interpolation, we should be greater than 0.
  dialog_view_->UpdateInterpolatedProgressBarValue();
  EXPECT_GT(dialog_view_->GetProgressBarValue(), 0);

  // Next interpolated step value should be greater than previous.
  const double previous_value = dialog_view_->GetProgressBarValue();
  dialog_view_->UpdateInterpolatedProgressBarValue();
  EXPECT_GT(dialog_view_->GetProgressBarValue(), previous_value);
}

}  // namespace
}  // namespace arc
