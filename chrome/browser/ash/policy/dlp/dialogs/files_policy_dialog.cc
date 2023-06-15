// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/views/widget/widget.h"

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

FilesPolicyDialogFactory* factory_;

FilesPolicyDialog::FilesPolicyDialog(size_t file_count,
                                     DlpFileDestination destination,
                                     dlp::FileAction action,
                                     gfx::NativeWindow modal_parent)
    : destination_(destination), action_(action), file_count_(file_count) {
  ui::ModalType modal =
      modal_parent ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_SYSTEM;
  SetModalType(modal);

  // TODO(b/283786807): Use type & policy for computing the strings.
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  AddGeneralInformation();
}

FilesPolicyDialog::~FilesPolicyDialog() = default;

views::Widget* FilesPolicyDialog::CreateWarnDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& files,
    DlpFileDestination destination,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent) {
  if (factory_) {
    return factory_->CreateWarnDialog(std::move(callback), files, destination,
                                      action, modal_parent);
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyWarnDialog>(
          std::move(callback), files, destination, action, modal_parent),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

views::Widget* FilesPolicyDialog::CreateErrorDialog(
    const std::map<DlpConfidentialFile, Policy>& files,
    DlpFileDestination destination,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent) {
  if (factory_) {
    return factory_->CreateErrorDialog(std::move(files), destination, action,
                                       modal_parent);
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyErrorDialog>(std::move(files), destination,
                                               action, modal_parent),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

// static
void FilesPolicyDialog::SetFactory(FilesPolicyDialogFactory* factory) {
  delete factory_;
  factory_ = factory;
}

void FilesPolicyDialog::AddGeneralInformation() {
  SetupUpperPanel(GetTitle(), GetMessage());
}

std::u16string FilesPolicyDialog::GetOkButton() {
  switch (action_) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_CONTINUE_BUTTON);
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
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
    case dlp::FileAction::kDownload:
      return l10n_util::GetPluralStringFUTF16(
          // Download action is only allowed for one file.
          IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_TITLE, 1);
    case dlp::FileAction::kUpload:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_WARN_TITLE, file_count_);
    case dlp::FileAction::kCopy:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_COPY_WARN_TITLE, file_count_);
    case dlp::FileAction::kMove:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_MOVE_WARN_TITLE, file_count_);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_OPEN_WARN_TITLE, file_count_);
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:  // TODO(crbug.com/1361900)
                                     // Set proper text when file
                                     // action is unknown
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_TITLE, file_count_);
  }
}

std::u16string FilesPolicyDialog::GetMessage() {
  std::u16string destination_str;
  int message_id;
  switch (action_) {
    case dlp::FileAction::kDownload:
      destination_str = GetDestinationComponent(destination_);
      // Download action is only allowed for one file.
      return base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_MESSAGE, 1),
          destination_str,
          /*offset=*/nullptr);
    case dlp::FileAction::kUpload:
      destination_str = GetDestinationURL(destination_);
      message_id = IDS_POLICY_DLP_FILES_UPLOAD_WARN_MESSAGE;
      break;
    case dlp::FileAction::kCopy:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_COPY_WARN_MESSAGE;
      break;
    case dlp::FileAction::kMove:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_MOVE_WARN_MESSAGE;
      break;
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_OPEN_WARN_MESSAGE;
      break;
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
      // TODO(crbug.com/1361900): Set proper text when file action is unknown.
      destination_str = GetDestination(destination_);
      message_id = IDS_POLICY_DLP_FILES_TRANSFER_WARN_MESSAGE;
      break;
  }
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, file_count_),
      destination_str,
      /*offset=*/nullptr);
}

BEGIN_METADATA(FilesPolicyDialog, PolicyDialogBase)
END_METADATA

}  // namespace policy
