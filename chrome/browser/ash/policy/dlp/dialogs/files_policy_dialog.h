// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/dlp/files_policy_warn_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace policy {

class FilesPolicyDialogFactory;

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

// FilesPolicyDialog is a window modal dialog used to show detailed overview of
// warnings and files blocked by data protection policies.
class FilesPolicyDialog : public PolicyDialogBase {
 public:
  METADATA_HEADER(FilesPolicyDialog);

  // Reasons for which a file can be blocked either because of an Enterprise
  // Connectors or DLP policy.
  enum class BlockReason {
    // File was blocked because of Data Leak Prevention policies.
    kDlp,
    // File was blocked but the reason is not known.
    kEnterpriseConnectorsUnknown,
    // File was blocked because it contains sensitive data (e.g., SSNs).
    kEnterpriseConnectorsSensitiveData,
    // File was blocked because it's a malware.
    kEnterpriseConnectorsMalware,
    // File was blocked because it could not be scanned due to encryption.
    kEnterpriseConnectorsEncryptedFile,
    // File was blocked because it could not be uploaded due to its size.
    kEnterpriseConnectorsLargeFile,
  };

  FilesPolicyDialog() = delete;
  FilesPolicyDialog(size_t file_count,
                    dlp::FileAction action,
                    gfx::NativeWindow modal_parent);
  FilesPolicyDialog(const FilesPolicyDialog& other) = delete;
  FilesPolicyDialog& operator=(const FilesPolicyDialog& other) = delete;
  ~FilesPolicyDialog() override;

  // Creates and shows an instance of FilesPolicyWarnDialog. Returns owning
  // Widget.
  static views::Widget* CreateWarnDialog(
      OnDlpRestrictionCheckedWithJustificationCallback callback,
      const std::vector<DlpConfidentialFile>& files,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent,
      absl::optional<DlpFileDestination> destination = absl::nullopt,
      FilesPolicyWarnSettings settings = FilesPolicyWarnSettings());

  // Creates and shows an instance of FilesPolicyErrorDialog. Returns owning
  // Widget.
  static views::Widget* CreateErrorDialog(
      const std::map<DlpConfidentialFile, BlockReason>& files,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent);

  static void SetFactory(FilesPolicyDialogFactory* factory);

 protected:
  // PolicyDialogBase overrides:
  void SetupScrollView() override;
  void AddConfidentialRow(const gfx::ImageSkia& icon,
                          const std::u16string& title) override;

  dlp::FileAction action_;
  // Number of files listed in the dialog.
  size_t file_count_;

 private:
  // PolicyDialogBase overrides:
  views::Label* AddTitle(const std::u16string& title) override;
  views::Label* AddMessage(const std::u16string& message) override;
};

// Interface for creating warn and error FilesPolicyDialogs.
// Used in tests.
class FilesPolicyDialogFactory {
 public:
  virtual ~FilesPolicyDialogFactory() = default;

  virtual views::Widget* CreateWarnDialog(
      OnDlpRestrictionCheckedWithJustificationCallback callback,
      const std::vector<DlpConfidentialFile>& files,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent,
      absl::optional<DlpFileDestination> destination,
      FilesPolicyWarnSettings settings) = 0;

  virtual views::Widget* CreateErrorDialog(
      const std::map<DlpConfidentialFile, FilesPolicyDialog::BlockReason>&
          files,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
