// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/data_controls_dialog_test_helper.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"

namespace data_controls {

DataControlsDialogTestHelper::DataControlsDialogTestHelper(
    DataControlsDialog::Type expected_dialog_type)
    : expected_dialog_type_(expected_dialog_type) {
  dialog_init_loop_ = std::make_unique<base::RunLoop>();
  dialog_close_loop_ = std::make_unique<base::RunLoop>();
  dialog_init_callback_ = dialog_init_loop_->QuitClosure();
  dialog_close_callback_ = dialog_close_loop_->QuitClosure();
}

DataControlsDialogTestHelper::~DataControlsDialogTestHelper() = default;

void DataControlsDialogTestHelper::OnConstructed(DataControlsDialog* dialog) {
  ASSERT_FALSE(dialog_) << "Only one DataControlsDialog should be opened at a "
                           "time for a test using this helper class.";
  dialog_ = dialog;
  ASSERT_EQ(dialog->type(), expected_dialog_type_);
}

void DataControlsDialogTestHelper::OnWidgetInitialized(
    DataControlsDialog* dialog) {
  ASSERT_TRUE(dialog);
  ASSERT_EQ(dialog, dialog_);

  std::move(dialog_init_callback_).Run();
}

void DataControlsDialogTestHelper::OnDestructed(DataControlsDialog* dialog) {
  ASSERT_TRUE(dialog);
  ASSERT_EQ(dialog, dialog_);
  dialog_ = nullptr;

  std::move(dialog_close_callback_).Run();
}

DataControlsDialog* DataControlsDialogTestHelper::dialog() {
  return dialog_;
}

void DataControlsDialogTestHelper::AcceptDialog() {
  // Some platforms crash if the dialog has been accepted/cancelled before fully
  // launching modally, so to avoid that issue accepting/cancelling the dialog
  // is done asynchronously.
  ASSERT_TRUE(dialog_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DataControlsDialog::AcceptDialog,
                                base::Unretained(dialog_)));
}

void DataControlsDialogTestHelper::CancelDialog() {
  // Some platforms crash if the dialog has been accepted/cancelled before fully
  // launching modally, so to avoid that issue accepting/cancelling the dialog
  // is done asynchronously.
  ASSERT_TRUE(dialog_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&DataControlsDialog::CancelDialog,
                                base::Unretained(dialog_)));
}

void DataControlsDialogTestHelper::WaitForDialogToInitialize() {
  ASSERT_TRUE(dialog_init_loop_);
  dialog_init_loop_->Run();
}

void DataControlsDialogTestHelper::WaitForDialogToClose() {
  ASSERT_TRUE(dialog_close_loop_);
  dialog_close_loop_->Run();
}

}  // namespace data_controls
