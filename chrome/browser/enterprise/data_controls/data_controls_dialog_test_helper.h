// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/enterprise/data_controls/data_controls_dialog.h"

namespace data_controls {

// Helper class to interact with a DataControlsDialog that might be shown during
// a test.
class DataControlsDialogTestHelper : public DataControlsDialog::TestObserver {
 public:
  explicit DataControlsDialogTestHelper(
      DataControlsDialog::Type expected_dialog_type);
  ~DataControlsDialogTestHelper();

  // DataControlsDialog::TestObserver:
  void OnConstructed(DataControlsDialog* dialog) override;
  void OnWidgetInitialized(DataControlsDialog* dialog) override;
  void OnDestructed(DataControlsDialog* dialog) override;

  // Returns null if no dialog is currently being shown.
  DataControlsDialog* dialog();

  // Mimics the user pressing either of the available dialog buttons.
  void AcceptDialog();
  void CancelDialog();

  // Runs `dialog_init_loop_`.
  void WaitForDialogToInitialize();

  // Runs `dialog_close_loop_`.
  void WaitForDialogToClose();

 private:
  raw_ptr<DataControlsDialog> dialog_ = nullptr;
  DataControlsDialog::Type expected_dialog_type_;

  // Members used to track the dialog being initialized.
  std::unique_ptr<base::RunLoop> dialog_init_loop_;
  base::OnceClosure dialog_init_callback_;

  // Members used to track the dialog closing.
  std::unique_ptr<base::RunLoop> dialog_close_loop_;
  base::OnceClosure dialog_close_callback_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DATA_CONTROLS_DIALOG_TEST_HELPER_H_
