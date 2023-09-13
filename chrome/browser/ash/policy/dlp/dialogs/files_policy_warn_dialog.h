// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_WARN_DIALOG_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_warn_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/native_widget_types.h"

namespace policy {

// FilesPolicyWarnDialog is a window modal dialog used to show detailed overview
// of file warnings caused by by data protection policies.
class FilesPolicyWarnDialog : public FilesPolicyDialog {
 public:
  METADATA_HEADER(FilesPolicyWarnDialog);

  FilesPolicyWarnDialog() = delete;
  FilesPolicyWarnDialog(
      OnDlpRestrictionCheckedWithJustificationCallback callback,
      const std::vector<DlpConfidentialFile>& files,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent,
      absl::optional<DlpFileDestination> destination,
      FilesPolicyWarnSettings settings);
  FilesPolicyWarnDialog(const FilesPolicyWarnDialog&) = delete;
  FilesPolicyWarnDialog(FilesPolicyWarnDialog&&) = delete;
  FilesPolicyWarnDialog& operator=(const FilesPolicyWarnDialog&) = delete;
  FilesPolicyWarnDialog& operator=(FilesPolicyWarnDialog&&) = delete;
  ~FilesPolicyWarnDialog() override;

 private:
  // PolicyDialogBase overrides:
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetCancelButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Called when the user proceeds the warning.
  void ProceedWarning(
      OnDlpRestrictionCheckedWithJustificationCallback callback);
  // Called when the user cancels the warning.
  void CancelWarning(OnDlpRestrictionCheckedWithJustificationCallback callback);

  std::vector<DlpConfidentialFile> files_;
  // TODO(b/290329012): Remove.
  absl::optional<DlpFileDestination> destination_;

  base::WeakPtrFactory<FilesPolicyWarnDialog> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_BLOCK_DIALOG_H_
