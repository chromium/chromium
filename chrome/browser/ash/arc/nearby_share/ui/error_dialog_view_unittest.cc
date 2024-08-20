// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/error_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/mojom/dialog_button.mojom.h"

namespace arc {

class ErrorDialogViewTest : public CompatModeTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    CompatModeTestBase::SetUp();
    test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    arc_widget_ = CreateArcWidget(/*app_id=*/"123");
    error_dialog_view_ = new ErrorDialogView(
        arc_widget_->GetContentsView(),
        base::BindOnce(&ErrorDialogViewTest::OnCloseCallback,
                       base::Unretained(this)));
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(error_dialog_view_);
    bubble_widget_->Show();
  }
  void TearDown() override {
    if (!bubble_widget_->IsClosed()) {
      bubble_widget_->CloseNow();
    }
    arc_widget_->CloseNow();
    CompatModeTestBase::TearDown();
  }

  void OnCloseCallback() { on_close_callback_count_++; }

  ErrorDialogView* error_dialog_view() const { return error_dialog_view_; }

  int GetOnCloseCallbackCount() const { return on_close_callback_count_; }

 private:
  int on_close_callback_count_ = 0;
  std::unique_ptr<views::Widget> arc_widget_;
  raw_ptr<views::Widget, DanglingUntriaged> bubble_widget_;
  raw_ptr<ErrorDialogView, DanglingUntriaged> error_dialog_view_;
};

TEST_F(ErrorDialogViewTest, ConstructDestruct) {
  EXPECT_EQ(0, GetOnCloseCallbackCount());
  // Verify there's only 1 button in the dialog.
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk),
            error_dialog_view()->buttons());
  EXPECT_TRUE(
      error_dialog_view()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}

TEST_F(ErrorDialogViewTest, TestCloseButton) {
  // Verify the callback hasn't been called.
  EXPECT_EQ(0, GetOnCloseCallbackCount());

  // Click on the OK button.
  EXPECT_TRUE(error_dialog_view()->Accept());

  // Verify the callback was called.
  EXPECT_EQ(1, GetOnCloseCallbackCount());
}

}  // namespace arc
