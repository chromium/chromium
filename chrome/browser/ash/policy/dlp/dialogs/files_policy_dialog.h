// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
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
  METADATA_HEADER(FilesPolicyDialog, PolicyDialogBase)

 public:
  // Reasons for which a file can be blocked either because of an Enterprise
  // Connectors or DLP policy.
  // Please keep in sync with the `available_reasons` array below!
  // TODO(b/302653030): consider whether unifying BlockReason and Policy.
  enum class BlockReason {
    // File was blocked because of Data Leak Prevention policies.
    kDlp,
    // File was blocked because added to an Enterprise Connectors scanned
    // directory after the scan begun, and thus the file was not scanned.
    kEnterpriseConnectorsUnknownScanResult,
    // File was blocked because the scan failed.
    kEnterpriseConnectorsScanFailed,
    // File was blocked because it contains sensitive data (e.g., SSNs).
    kEnterpriseConnectorsSensitiveData,
    // File was blocked because it's a malware.
    kEnterpriseConnectorsMalware,
    // File was blocked because it could not be scanned due to encryption.
    kEnterpriseConnectorsEncryptedFile,
    // File was blocked because it could not be uploaded due to its size.
    kEnterpriseConnectorsLargeFile,
    // File was blocked because of Enterprise Connectors policies. This can be
    // used to mark files that do not require a more specific description of the
    // reason for which they were blocked.
    kEnterpriseConnectors,
  };

  // All the available reasons.
  // Please keep the array in sync with the `BlockReason` enum above!
  static constexpr std::array<BlockReason, 8> available_reasons{
      BlockReason::kDlp,
      BlockReason::kEnterpriseConnectorsUnknownScanResult,
      BlockReason::kEnterpriseConnectorsScanFailed,
      BlockReason::kEnterpriseConnectorsSensitiveData,
      BlockReason::kEnterpriseConnectorsMalware,
      BlockReason::kEnterpriseConnectorsEncryptedFile,
      BlockReason::kEnterpriseConnectorsLargeFile,
      BlockReason::kEnterpriseConnectors};

  // Returns the ID of the view that contains all details related to the given
  // `reason` in a mixed error dialog.
  static PolicyDialogBase::ViewIds MapBlockReasonToViewID(BlockReason reason);

  // Class holding information to build a dialog such as a message to the user,
  // a list of files involved, an optional learn more link, and for warning
  // scenarios whether the user is required to provide a justification to bypass
  // the warning. These info map to a section in the dialog.
  class Info {
   public:
    // Creates default dialog settings for warning scenarios.
    static Info Warn(BlockReason reason,
                     const std::vector<base::FilePath>& paths);

    // Creates default dialog settings for error scenarios.
    static Info Error(BlockReason reason,
                      const std::vector<base::FilePath>& paths);

    ~Info();
    Info(const Info& other);
    Info& operator=(Info&& other);

    bool operator==(const Info& other) const;
    bool operator!=(const Info& other) const;

    const std::vector<DlpConfidentialFile>& GetFiles() const;

    // For warning scenarios only, returns whether bypassing the warning
    // requires a user justification.
    bool DoesBypassRequireJustification() const;

    // Sets whether bypassing a warning requires a user justification.
    void SetBypassRequiresJustification(bool value);

    // Returns the message that should be shown in the dialog.
    std::u16string GetMessage() const;

    // Overrides the default message.
    void SetMessage(const std::optional<std::u16string>& message);

    // Returns whether a custom message was set.
    bool HasCustomMessage() const;

    // Returns the learn more URL that should be shown in the dialog, if any.
    std::optional<GURL> GetLearnMoreURL() const;

    // Overrides the default learn more URL.
    void SetLearnMoreURL(const std::optional<GURL>& url);

    // Returns an accessible learn more link name, if available. An empty string
    // otherwise.
    std::u16string GetAccessibleLearnMoreLinkName() const;

    // Returns whether at least one of the default values (e.g., message, learn
    // more URL, etc...) has been overridden with a custom value.
    bool HasCustomDetails() const;

   private:
    Info();

    // The files that should be displayed.
    std::vector<DlpConfidentialFile> files_;

    // Whether the user is required to write a justification to bypass the
    // warning. This is only relevant for warning scenarios.
    bool bypass_requires_justification_ = false;

    // Default or admin defined message.
    std::u16string message_;

    // Whether `message_` is a custom message.
    bool is_custom_message_ = false;

    // Whether `learn_more_url_` is a custom url.
    bool is_custom_learn_more_url_ = false;

    // Learn more link name providing more info for users using a ChromeVox
    // reader.
    std::u16string accessible_learn_more_link_name_;

    // Default, admin defined learn more URL, or none of them.
    std::optional<GURL> learn_more_url_;
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
      WarningWithJustificationCallback callback,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent,
      Info dialog_info,
      std::optional<DlpFileDestination> destination = std::nullopt);

  // Creates and shows an instance of FilesPolicyErrorDialog. Returns owning
  // Widget.
  static views::Widget* CreateErrorDialog(
      const std::map<BlockReason, Info>& dialog_info_map,
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
      WarningWithJustificationCallback callback,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent,
      std::optional<DlpFileDestination> destination,
      FilesPolicyDialog::Info settings) = 0;

  virtual views::Widget* CreateErrorDialog(
      const std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>&
          dialog_info_map,
      dlp::FileAction action,
      gfx::NativeWindow modal_parent) = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_H_
