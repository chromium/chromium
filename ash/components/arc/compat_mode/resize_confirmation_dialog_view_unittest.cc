// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/resize_confirmation_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/test/test_widget_observer.h"

namespace arc {
namespace {

class ResizeConfirmationDialogViewTest : public CompatModeTestBase {
 public:
  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->SetBounds(gfx::Rect(800, 800));
    parent_widget_ = CreateArcWidget(/*app_id=*/"123");
    dialog_view_ =
        widget_->SetContentsView(std::make_unique<ResizeConfirmationDialogView>(
            parent_widget_.get(),
            base::BindOnce(&ResizeConfirmationDialogViewTest::OnClicked,
                           base::Unretained(this))));
    widget_->Show();
  }

  void TearDown() override {
    widget_->Close();
    widget_.reset();
    CompatModeTestBase::TearDown();
  }

 protected:
  void ClickDialogButton(bool accept, bool with_checkbox) {
    ResizeConfirmationDialogView::TestApi dialog_view_test(dialog_view_);
    if (with_checkbox) {
      dialog_view_test.SelectDoNotAskCheckbox();
    }

    auto* target_button = accept ? dialog_view_test.accept_button()
                                 : dialog_view_test.cancel_button();

    LeftClickOnView(widget_.get(), target_button);
  }

  views::Widget* widget() { return widget_.get(); }
  bool callback_called() { return callback_called_; }
  bool callback_accepted() { return callback_accepted_; }
  bool callback_do_not_ask_again() { return callback_do_not_ask_again_; }

 private:
  void OnClicked(bool accepted, bool do_not_ask_again) {
    callback_called_ = true;
    callback_accepted_ = accepted;
    callback_do_not_ask_again_ = do_not_ask_again;
  }

  // For callback checks.
  bool callback_called_{false};
  bool callback_accepted_{false};
  bool callback_do_not_ask_again_{false};

  // A LayoutProvider must exist in scope in order to set up views.
  views::LayoutProvider layout_provider;

  // An AshColorProvider must exist in scope in order to set up views.
  ash::AshColorProvider ash_color_provider_;

  raw_ptr<ResizeConfirmationDialogView, DanglingUntriaged> dialog_view_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<views::Widget> parent_widget_;
};

TEST_F(ResizeConfirmationDialogViewTest, ClickAcceptWithCheckbox) {
  ClickDialogButton(/*accept=*/true, /*with_checkbox=*/true);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(callback_accepted());
  EXPECT_TRUE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickCancelWithCheckbox) {
  ClickDialogButton(/*accept=*/false, /*with_checkbox=*/true);
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_accepted());
  EXPECT_TRUE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickAcceptWithoutCheckbox) {
  ClickDialogButton(/*accept=*/true, /*with_checkbox=*/false);
  EXPECT_TRUE(callback_called());
  EXPECT_TRUE(callback_accepted());
  EXPECT_FALSE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, ClickCancelWithoutCheckbox) {
  ClickDialogButton(/*accept=*/false, /*with_checkbox=*/false);
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_accepted());
  EXPECT_FALSE(callback_do_not_ask_again());
}

TEST_F(ResizeConfirmationDialogViewTest, CloseWidget) {
  EXPECT_FALSE(callback_called());
  widget()->CloseNow();
  EXPECT_TRUE(callback_called());
  EXPECT_FALSE(callback_accepted());
  EXPECT_FALSE(callback_do_not_ask_again());
}
}  // namespace
}  // namespace arc
