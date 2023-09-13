// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/ash/policy/dlp/files_policy_warn_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/common/chrome_features.h"
#include "components/enterprise/data_controls/component.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "url/gurl.h"

namespace policy {
namespace {

// Returns the domain of the |destination|'s |url| if it can be
// obtained, or the full value otherwise, converted to u16string. Fails if
// |url| is empty.
std::u16string GetDestinationURL(DlpFileDestination destination) {
  DCHECK(destination.url().has_value());
  DCHECK(destination.url()->is_valid());
  GURL gurl = *destination.url();
  if (gurl.has_host()) {
    return base::UTF8ToUTF16(gurl.host());
  }
  return base::UTF8ToUTF16(gurl.spec());
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
// |url| are empty (the destination is a local file/directory). Returns the
// |component| if both are non-empty.
const std::u16string GetDestination(DlpFileDestination destination) {
  return destination.component().has_value()
             ? GetDestinationComponent(destination)
             : GetDestinationURL(destination);
}
}  // namespace

FilesPolicyWarnDialog::FilesPolicyWarnDialog(
    OnDlpRestrictionCheckedWithJustificationCallback callback,
    const std::vector<DlpConfidentialFile>& files,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent,
    absl::optional<DlpFileDestination> destination,
    FilesPolicyWarnSettings settings)
    : FilesPolicyDialog(files.size(), action, modal_parent),
      files_(files),
      destination_(destination) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(&FilesPolicyWarnDialog::ProceedWarning,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(split.first)));
  SetCancelCallback(base::BindOnce(&FilesPolicyWarnDialog::CancelWarning,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(split.second)));
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  AddGeneralInformation();
  MaybeAddConfidentialRows();

  // TODO(b/299578935): Customize the warning dialog according to
  // `warning_message`, `learn_more_url` and
  // `bypass_requires_justification` values stored in `settings`.

  DlpHistogramEnumeration(dlp::kFileActionWarnReviewedUMA, action);
}

FilesPolicyWarnDialog::~FilesPolicyWarnDialog() = default;

void FilesPolicyWarnDialog::MaybeAddConfidentialRows() {
  if (action_ == dlp::FileAction::kDownload || files_.empty()) {
    return;
  }

  SetupScrollView();
  for (const auto& file : files_) {
    AddConfidentialRow(file.icon, file.title);
  }
}

std::u16string FilesPolicyWarnDialog::GetOkButton() {
  return policy::files_string_util::GetContinueAnywayButton(action_);
}

std::u16string FilesPolicyWarnDialog::GetCancelButton() {
  return l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON);
}

std::u16string FilesPolicyWarnDialog::GetTitle() {
  if (base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    switch (action_) {
      case dlp::FileAction::kDownload:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_DOWNLOAD_REVIEW_TITLE);
      case dlp::FileAction::kUpload:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_UPLOAD_REVIEW_TITLE);
      case dlp::FileAction::kCopy:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_COPY_REVIEW_TITLE);
      case dlp::FileAction::kMove:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_MOVE_REVIEW_TITLE);
      case dlp::FileAction::kOpen:
      case dlp::FileAction::kShare:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_OPEN_REVIEW_TITLE);
      case dlp::FileAction::kTransfer:
      case dlp::FileAction::kUnknown:  // TODO(crbug.com/1361900)
                                       // Set proper text when file
                                       // action is unknown
        return l10n_util::GetStringUTF16(
            IDS_POLICY_DLP_FILES_TRANSFER_REVIEW_TITLE);
    }
  }
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

std::u16string FilesPolicyWarnDialog::GetMessage() {
  if (base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    return base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(IDS_POLICY_DLP_FILES_WARN_MESSAGE,
                                         files_.size()),
        base::NumberToString16(files_.size()),
        /*offset=*/nullptr);
  }
  CHECK(destination_.has_value());
  DlpFileDestination destination_val = destination_.value();
  std::u16string destination_str;
  int message_id;
  switch (action_) {
    case dlp::FileAction::kDownload:
      destination_str = GetDestinationComponent(destination_val);
      // Download action is only allowed for one file.
      return base::ReplaceStringPlaceholders(
          l10n_util::GetPluralStringFUTF16(
              IDS_POLICY_DLP_FILES_DOWNLOAD_WARN_MESSAGE, 1),
          destination_str,
          /*offset=*/nullptr);
    case dlp::FileAction::kUpload:
      destination_str = GetDestinationURL(destination_val);
      message_id = IDS_POLICY_DLP_FILES_UPLOAD_WARN_MESSAGE;
      break;
    case dlp::FileAction::kCopy:
      destination_str = GetDestination(destination_val);
      message_id = IDS_POLICY_DLP_FILES_COPY_WARN_MESSAGE;
      break;
    case dlp::FileAction::kMove:
      destination_str = GetDestination(destination_val);
      message_id = IDS_POLICY_DLP_FILES_MOVE_WARN_MESSAGE;
      break;
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      destination_str = GetDestination(destination_val);
      message_id = IDS_POLICY_DLP_FILES_OPEN_WARN_MESSAGE;
      break;
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
      // kUnknown is used for internal checks - treat as kTransfer.
      destination_str = GetDestination(destination_val);
      message_id = IDS_POLICY_DLP_FILES_TRANSFER_WARN_MESSAGE;
      break;
  }
  return base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, file_count_),
      destination_str,
      /*offset=*/nullptr);
}

void FilesPolicyWarnDialog::ProceedWarning(
    OnDlpRestrictionCheckedWithJustificationCallback callback) {
  std::move(callback).Run(/*user_justification=*/absl::nullopt,
                          /*should_proceed=*/true);
}

void FilesPolicyWarnDialog::CancelWarning(
    OnDlpRestrictionCheckedWithJustificationCallback callback) {
  std::move(callback).Run(/*user_justification=*/absl::nullopt,
                          /*should_proceed=*/false);
}

BEGIN_METADATA(FilesPolicyWarnDialog, FilesPolicyDialog)
END_METADATA

}  // namespace policy
