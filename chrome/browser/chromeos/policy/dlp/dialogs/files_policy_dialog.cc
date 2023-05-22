// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace policy {

namespace {
// Returns the domain of the |destination|'s |url_or_path| if it can be
// obtained, or the full value otherwise, converted to u16string. Fails if
// |url_or_path| is empty.
std::u16string GetDestinationURL(DlpFileDestination destination) {
  DCHECK(destination.url_or_path().has_value());
  DCHECK(!destination.url_or_path()->empty());
  std::string url = destination.url_or_path().value();
  GURL gurl(url);
  if (gurl.is_valid() && gurl.has_host()) {
    return base::UTF8ToUTF16(gurl.host());
  }
  return base::UTF8ToUTF16(url);
}

// Returns the u16string formatted name for |destination|'s |component|. Fails
// if |component| is empty.
const std::u16string GetDestinationComponent(DlpFileDestination destination) {
  DCHECK(destination.component().has_value());
  switch (destination.component().value()) {
    case data_controls::Component::kArc:
      return l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL);
    case data_controls::Component::kCrostini:
      return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL);
    case data_controls::Component::kPluginVm:
      return l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_PLUGIN_VM_DIRECTORY_LABEL);
    case data_controls::Component::kUsb:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DESTINATION_REMOVABLE_STORAGE);
    case data_controls::Component::kDrive:
      return l10n_util::GetStringUTF16(IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
    case data_controls::Component::kOneDrive:
      return l10n_util::GetStringUTF16(
          IDS_FILE_BROWSER_DLP_COMPONENT_MICROSOFT_ONEDRIVE);
    case data_controls::Component::kUnknownComponent:
      NOTREACHED();
      return u"";
  }
}

// Returns the u16string formatted |destination|. Fails if both |component| and
// |url_or_path| are empty. Returns the |component| if both are non-empty.
const std::u16string GetDestination(DlpFileDestination destination) {
  return destination.component().has_value()
             ? GetDestinationComponent(destination)
             : GetDestinationURL(destination);
}
}  // namespace

FilesPolicyDialog::FilesPolicyDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& files,
    DlpFileDestination destination,
    DlpFilesController::FileAction action,
    gfx::NativeWindow modal_parent)
    : PolicyDialogBase(std::move(callback)),
      files_(std::move(files)),
      destination_(destination),
      action_(action) {
  // TODO(b/277879595): When no Files app window, open a new one.
  // TODO(b/279397364): Confirm behavior if we cannot open Files App.
  ui::ModalType type =
      modal_parent ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_SYSTEM;
  SetModalType(type);

  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  AddGeneralInformation();
  MaybeAddConfidentialRows();
}

FilesPolicyDialog::~FilesPolicyDialog() = default;

void FilesPolicyDialog::AddGeneralInformation() {
  SetupUpperPanel(GetTitle(), GetMessage());
}

void FilesPolicyDialog::MaybeAddConfidentialRows() {
  if (files_.empty()) {
    return;
  }

  SetupScrollView();
  for (const DlpConfidentialFile& file : files_) {
    AddConfidentialRow(file.icon, file.title);
  }
}

std::u16string FilesPolicyDialog::GetOkButton() {
  switch (action_) {
    case DlpFilesController::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_CONTINUE_BUTTON);
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:
      // TODO(crbug.com/1361900): Set proper text when file action is unknown.
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_CONTINUE_BUTTON);
  }
}

std::u16string FilesPolicyDialog::GetCancelButton() {
  return l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON);
}

std::u16string FilesPolicyDialog::GetTitle() {
  switch (action_) {
    case DlpFilesController::FileAction::kDownload:
      return l10n_util::GetPluralStringFUTF16(
          // Download action is only allowed for one file.
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_TITLE, 1);
    case DlpFilesController::FileAction::kUpload:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_TITLE, files_.size());
    case DlpFilesController::FileAction::kCopy:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_TITLE, files_.size());
    case DlpFilesController::FileAction::kMove:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_TITLE, files_.size());
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_TITLE, files_.size());
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:  // TODO(crbug.com/1361900)
                                                    // Set proper text when file
                                                    // action is unknown
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_TITLE, files_.size());
  }
}

std::u16string FilesPolicyDialog::GetMessage() {
  std::u16string destination_str;
  int message_id;
  switch (action_) {
    case DlpFilesController::FileAction::kDownload:
      destination_str = GetDestinationComponent(destination_);
      // Download action is only allowed for one file.
      return base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_MESSAGE, 1),
          destination_str,
          /*offset=*/nullptr);
    case DlpFilesController::FileAction::kUpload:
      destination_str = GetDestinationURL(destination_);
      message_id = IDS_POLICY_DLP_FILES_UPLOAD_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kCopy:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_COPY_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kMove:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_MOVE_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kOpen:
    case DlpFilesController::FileAction::kShare:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_OPEN_WARN_MESSAGE;
      break;
    case DlpFilesController::FileAction::kTransfer:
    case DlpFilesController::FileAction::kUnknown:
      // TODO(crbug.com/1361900): Set proper text when file action is unknown.
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_TRANSFER_WARN_MESSAGE;
      break;
  }
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, files_.size()),
      destination_str,
      /*offset=*/nullptr);
}

BEGIN_METADATA(FilesPolicyDialog, PolicyDialogBase)
END_METADATA

}  // namespace policy
