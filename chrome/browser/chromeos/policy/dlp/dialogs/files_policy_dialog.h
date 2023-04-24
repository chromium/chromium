// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_

#include <string>

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace policy {

// FilesPolicyDialog is a window modal dialog used to show detailed overview of
// warnings and files blocked by data protection policies.
class FilesPolicyDialog : public PolicyDialogBase {
 public:
  METADATA_HEADER(FilesPolicyDialog);

  FilesPolicyDialog() = delete;
  FilesPolicyDialog(OnDlpRestrictionCheckedCallback callback,
                    const std::vector<DlpConfidentialFile>& files,
                    DlpFileDestination destination,
                    DlpFilesController::FileAction action,
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

  std::vector<DlpConfidentialFile> files_;
  DlpFileDestination destination_;
  DlpFilesController::FileAction action_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
