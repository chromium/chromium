// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"

namespace policy {

// Dialog type (warning or error).
enum class FilesDialogType {
  kUnknown,  // Not a valid type - no dialog will be created.
  kWarning,  // Warning dialog - user can select to proceed or not.
  kError,    // Error dialog - overview of blocked files.
};

// Type of policy. Used for warning type dialogs.
enum class Policy {
  kDlp,                   // Data Leak Prevention policy.
  kEnterpriseConnectors,  // Enterprise Connectors policy.
};

// FilesPolicyDialog is a window modal dialog used to show detailed overview of
// warnings and files blocked by data protection policies.
class FilesPolicyDialog : public PolicyDialogBase {
 public:
  METADATA_HEADER(FilesPolicyDialog);

  FilesPolicyDialog() = delete;
  // TODO(b/283785160): Remove after migrating to the new UI for files.
  FilesPolicyDialog(OnDlpRestrictionCheckedCallback callback,
                    const std::vector<DlpConfidentialFile>& files,
                    DlpFileDestination destination,
                    dlp::FileAction action,
                    gfx::NativeWindow modal_parent);
  // `callback` and `policy` are required only for kWarning `type`.
  FilesPolicyDialog(FilesDialogType type,
                    absl::optional<Policy> policy,
                    absl::optional<OnDlpRestrictionCheckedCallback> callback,
                    const std::vector<DlpConfidentialFile>& files,
                    DlpFileDestination destination,
                    dlp::FileAction action,
                    gfx::NativeWindow modal_parent);

  FilesPolicyDialog(const FilesPolicyDialog& other) = delete;
  FilesPolicyDialog& operator=(const FilesPolicyDialog& other) = delete;
  ~FilesPolicyDialog() override;

 private:
  // PolicyDialogBase overrides:
  void AddGeneralInformation() override;
  void MaybeAddConfidentialRows() override;
  std::u16string GetOkButton() override;
  std::u16string GetCancelButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Called from the error dialog's "Cancel" button.
  // Opens the help page for policy/-ies that blocked the file action.
  void OpenHelpPage();

  // Called from the error dialog's "OK" button.
  // Dismisses the dialog.
  void Dismiss();

  FilesDialogType type_;
  absl::optional<Policy> policy_;
  std::vector<DlpConfidentialFile> files_;
  DlpFileDestination destination_;
  dlp::FileAction action_;

  base::WeakPtrFactory<FilesPolicyDialog> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
