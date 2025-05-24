// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/desktop_data_controls_dialog_test_helper.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"

namespace data_controls {

DesktopDataControlsDialogTestHelper::DesktopDataControlsDialogTestHelper(
    DataControlsDialog::Type expected_dialog_type)
    : expected_dialog_type_(expected_dialog_type) {
  dialog_init_loop_ = std::make_unique<base::RunLoop>();
  dialog_close_loop_ = std::make_unique<base::RunLoop>();
  dialog_init_callback_ = dialog_init_loop_->QuitClosure();
  dialog_close_callback_ = dialog_close_loop_->QuitClosure();
}

DesktopDataControlsDialogTestHelper::~DesktopDataControlsDialogTestHelper() =
    default;

void DesktopDataControlsDialogTestHelper::OnConstructed(
    DesktopDataControlsDialog* dialog,
    views::DialogDelegate* dialog_delegate) {
  ASSERT_FALSE(dialog_)
      << "Only one DesktopDataControlsDialog should be opened at a time for a "
         "test using this helper class.";
  ASSERT_FALSE(dialog_delegate_)
      << "Only one views::DialogDelegate should be opened at a time for a "
         "test using this helper class.";
  dialog_ = dialog;
  dialog_delegate_ = dialog_delegate;
  ASSERT_EQ(dialog->type(), expected_dialog_type_);
}

void DesktopDataControlsDialogTestHelper::OnWidgetInitialized(
    DesktopDataControlsDialog* dialog,
    views::DialogDelegate* dialog_delegate) {
  ASSERT_TRUE(dialog);
  ASSERT_TRUE(dialog_delegate);

  ASSERT_EQ(dialog, dialog_);
  ASSERT_EQ(dialog_delegate, dialog_delegate_);

  std::move(dialog_init_callback_).Run();
}

void DesktopDataControlsDialogTestHelper::OnDestructed(
    DesktopDataControlsDialog* dialog) {
  ASSERT_TRUE(dialog);

  ASSERT_EQ(dialog, dialog_);
  dialog_ = nullptr;
  dialog_delegate_ = nullptr;

  std::move(dialog_close_callback_).Run();
}

DesktopDataControlsDialog* DesktopDataControlsDialogTestHelper::dialog() {
  return dialog_;
}

views::DialogDelegate* DesktopDataControlsDialogTestHelper::dialog_delegate() {
  return dialog_delegate_;
}

void DesktopDataControlsDialogTestHelper::BypassWarning() {
  // Some platforms crash if the dialog has been accepted/cancelled before fully
  // launching modally, so to avoid that issue accepting/cancelling the dialog
  // is done asynchronously.
  ASSERT_TRUE(dialog_delegate_);
  ASSERT_TRUE(dialog_->type() ==
                  DataControlsDialog::Type::kClipboardPasteWarn ||
              dialog_->type() == DataControlsDialog::Type::kClipboardCopyWarn);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&views::DialogDelegate::CancelDialog,
                                base::Unretained(dialog_delegate_)));
}

void DesktopDataControlsDialogTestHelper::CloseDialogWithoutBypass() {
  // Some platforms crash if the dialog has been accepted/cancelled before fully
  // launching modally, so to avoid that issue accepting/cancelling the dialog
  // is done asynchronously.
  ASSERT_TRUE(dialog_delegate_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&views::DialogDelegate::AcceptDialog,
                                base::Unretained(dialog_delegate_)));
}

void DesktopDataControlsDialogTestHelper::WaitForDialogToInitialize() {
  ASSERT_TRUE(dialog_init_loop_);
  dialog_init_loop_->Run();
}

void DesktopDataControlsDialogTestHelper::WaitForDialogToClose() {
  ASSERT_TRUE(dialog_close_loop_);
  dialog_close_loop_->Run();

  dialog_ = nullptr;
  dialog_delegate_ = nullptr;
}

}  // namespace data_controls
