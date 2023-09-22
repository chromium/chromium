// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/native_widget_types.h"

namespace policy {

// FilesPolicyErrorDialog is a window modal dialog used to show detailed
// overview of files blocked by data protection policies.
class FilesPolicyErrorDialog : public FilesPolicyDialog {
 public:
  METADATA_HEADER(FilesPolicyErrorDialog);

  FilesPolicyErrorDialog() = delete;
  FilesPolicyErrorDialog(const std::map<DlpConfidentialFile,
                                        FilesPolicyDialog::BlockReason>& files,
                         dlp::FileAction action,
                         gfx::NativeWindow modal_parent);
  FilesPolicyErrorDialog(const FilesPolicyErrorDialog&) = delete;
  FilesPolicyErrorDialog(FilesPolicyErrorDialog&&) = delete;
  FilesPolicyErrorDialog& operator=(const FilesPolicyErrorDialog&) = delete;
  FilesPolicyErrorDialog& operator=(FilesPolicyErrorDialog&&) = delete;
  ~FilesPolicyErrorDialog() override;

 private:
  // PolicyDialogBase overrides:
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetCancelButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Adds a row with blocked reason message based on `policy`. Should only be
  // called after `SetupUpperPanel()`.
  void AddPolicyRow(FilesPolicyDialog::BlockReason policy);

  // Called from the dialog's "Cancel" button.
  // Opens the help page for policy/-ies that blocked the file action.
  void OpenLearnMore();

  // Called from the dialog's "OK" button.
  // Dismisses the dialog.
  void Dismiss();

  // Maps each policy reason to the list of files blocked because of it.
  std::map<FilesPolicyDialog::BlockReason, std::vector<DlpConfidentialFile>>
      files_;
  // Total number of blocked files for all policies.
  size_t file_count_;

  base::WeakPtrFactory<FilesPolicyErrorDialog> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_ERROR_DIALOG_H_
