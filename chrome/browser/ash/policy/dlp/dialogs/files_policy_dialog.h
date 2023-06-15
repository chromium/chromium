// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace policy {

// Dialog type (warning or error).
enum class FilesDialogType {
  kUnknown,  // Not a valid type - no dialog will be created.
  kWarning,  // Warning dialog - user can select to proceed or not.
  kError,    // Error dialog - overview of blocked files.
};

// Type of policy.
enum class Policy {
  kDlp,                   // Data Leak Prevention policy.
  kEnterpriseConnectors,  // Enterprise Connectors policy.
};

// Interface for creating warn and error FilesPolicyDialogs.
// Used in tests.
class FilesPolicyDialogFactory {
 public:
  virtual ~FilesPolicyDialogFactory() = default;

  virtual views::Widget* CreateWarnDialog(
      OnDlpRestrictionCheckedCallback callback,
      const std::vector<DlpConfidentialFile>& files,
      DlpFileDestination destination,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent) = 0;

  virtual views::Widget* CreateErrorDialog(
      const std::map<DlpConfidentialFile, Policy>& files,
      DlpFileDestination destination,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent) = 0;
};

// FilesPolicyDialog is a window modal dialog used to show detailed overview of
// warnings and files blocked by data protection policies.
class FilesPolicyDialog : public PolicyDialogBase {
 public:
  METADATA_HEADER(FilesPolicyDialog);

  FilesPolicyDialog() = delete;
  FilesPolicyDialog(size_t file_count,
                    DlpFileDestination destination,
                    dlp::FileAction action,
                    gfx::NativeWindow modal_parent);
  FilesPolicyDialog(const FilesPolicyDialog& other) = delete;
  FilesPolicyDialog& operator=(const FilesPolicyDialog& other) = delete;
  ~FilesPolicyDialog() override;

  // Creates and shows an instance of FilesPolicyWarnDialog. Returns owning
  // Widget.
  static views::Widget* CreateWarnDialog(
      OnDlpRestrictionCheckedCallback callback,
      const std::vector<DlpConfidentialFile>& files,
      DlpFileDestination destination,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent);

  // Creates and shows an instance of FilesPolicyErrorDialog. Returns owning
  // Widget.
  static views::Widget* CreateErrorDialog(
      const std::map<DlpConfidentialFile, Policy>& files,
      DlpFileDestination destination,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent);

  static void SetFactory(FilesPolicyDialogFactory* factory);

 protected:
  DlpFileDestination destination_;
  dlp::FileAction action_;

 private:
  // PolicyDialogBase overrides:
  void AddGeneralInformation() override;
  std::u16string GetOkButton() override;
  std::u16string GetCancelButton() override;
  std::u16string GetTitle() override;
  std::u16string GetMessage() override;

  // Number of files listed in the dialog.
  size_t file_count_;

  base::WeakPtrFactory<FilesPolicyDialog> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
