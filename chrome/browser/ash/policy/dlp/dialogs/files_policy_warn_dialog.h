// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_WARN_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_WARN_DIALOG_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace policy {

// FilesPolicyWarnDialog is a window modal dialog used to show detailed overview
// of file warnings caused by by data protection policies.
class FilesPolicyWarnDialog : public FilesPolicyDialog,
                              public views::TextfieldController {
  METADATA_HEADER(FilesPolicyWarnDialog, FilesPolicyDialog)

 public:
  FilesPolicyWarnDialog() = delete;
  FilesPolicyWarnDialog(WarningWithJustificationCallback callback,
                        dlp::FileAction action,
                        gfx::NativeWindow modal_parent,
                        std::optional<DlpFileDestination> destination,
                        Info dialog_info);
  FilesPolicyWarnDialog(const FilesPolicyWarnDialog&) = delete;
  FilesPolicyWarnDialog(FilesPolicyWarnDialog&&) = delete;
  FilesPolicyWarnDialog& operator=(const FilesPolicyWarnDialog&) = delete;
  FilesPolicyWarnDialog& operator=(FilesPolicyWarnDialog&&) = delete;
  ~FilesPolicyWarnDialog() override;

  // Returns the maximum number of characters that can be inserted in the
  // justification textarea.
  size_t GetMaxBypassJustificationLengthForTesting() const;

 private:
  // PolicyDialogBase overrides:
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Returns the Cancel button label.
  std::u16string GetCancelButton();

  // Called when the user proceeds the warning.
  void ProceedWarning(WarningWithJustificationCallback callback);
  // Called when the user cancels the warning.
  void CancelWarning(WarningWithJustificationCallback callback);

  // Sets up view elements to handle user justification if required.
  void MaybeAddJustificationPanel();

  // views::TextfieldController overrides:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  std::vector<DlpConfidentialFile> files_;

  // TODO(b/290329012): Remove.
  std::optional<DlpFileDestination> destination_;

  // Holds the information that allow to populate the dialog UI such as the list
  // of warned files and the message shown.
  Info dialog_info_;

  raw_ptr<views::Textarea> justification_field_ = nullptr;
  raw_ptr<views::Label> justification_field_length_label_ = nullptr;

  base::WeakPtrFactory<FilesPolicyWarnDialog> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_WARN_DIALOG_H_
