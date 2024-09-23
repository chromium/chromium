// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/low_disk_space_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"

namespace arc {
class LowDiskSpaceDialogViewTest : public CompatModeTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    CompatModeTestBase::SetUp();
    test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());
    arc_widget_ = CreateArcWidget("123");
    dialog_view_ = new LowDiskSpaceDialogView(
        arc_widget_->GetContentsView(), true, 1024 * 1024 /* 1MB */,
        base::BindOnce(&LowDiskSpaceDialogViewTest::OnCloseCallback,
                       base::Unretained(this)));
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(dialog_view_);
    bubble_widget_->Show();
  }
  void TearDown() override {
    if (!bubble_widget_->IsClosed()) {
      bubble_widget_->CloseNow();
    }
    arc_widget_->CloseNow();
    CompatModeTestBase::TearDown();
  }

  void OnCloseCallback(bool should_open_storage_settings) {
    on_close_callback_count_++;
    should_open_storage_settings_ = should_open_storage_settings;
  }

  LowDiskSpaceDialogView* dialog_view() { return dialog_view_; }

  int GetOnCloseCallbackCount() const { return on_close_callback_count_; }

  bool GetShouldOpenStorageSettings() const {
    return should_open_storage_settings_;
  }

  void SetShouldOpenStorageSettings(bool should_open_storage_settings) {
    should_open_storage_settings_ = should_open_storage_settings;
  }

 private:
  bool should_open_storage_settings_ = false;
  int on_close_callback_count_ = 0;
  std::unique_ptr<views::Widget> arc_widget_;
  raw_ptr<views::Widget, DanglingUntriaged> bubble_widget_;
  raw_ptr<LowDiskSpaceDialogView, DanglingUntriaged> dialog_view_;
};

TEST_F(LowDiskSpaceDialogViewTest, ConstructDestruct) {
  EXPECT_EQ(0, GetOnCloseCallbackCount());
  // Verify there's only 1 button in the dialog.
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            dialog_view()->buttons());
  EXPECT_TRUE(
      dialog_view()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(
      dialog_view()->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));
}

TEST_F(LowDiskSpaceDialogViewTest, TestOkButton) {
  // Verify the callback hasn't been called.
  EXPECT_EQ(0, GetOnCloseCallbackCount());
  SetShouldOpenStorageSettings(true);

  // Click on the OK button.
  EXPECT_TRUE(dialog_view()->Accept());

  // Verify the callback was called.
  EXPECT_EQ(1, GetOnCloseCallbackCount());
  EXPECT_FALSE(GetShouldOpenStorageSettings());
}

TEST_F(LowDiskSpaceDialogViewTest, TestStorageManagementButton) {
  // Verify the callback hasn't been called.
  EXPECT_EQ(0, GetOnCloseCallbackCount());
  SetShouldOpenStorageSettings(false);

  // Click on the Cancel button (aka Storage Management) button.
  EXPECT_TRUE(dialog_view()->Cancel());
  // Verify the callback was called.
  EXPECT_EQ(1, GetOnCloseCallbackCount());
  EXPECT_TRUE(GetShouldOpenStorageSettings());
}

}  // namespace arc
