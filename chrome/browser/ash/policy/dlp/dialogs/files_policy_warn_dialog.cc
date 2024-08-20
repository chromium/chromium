// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"

#include <optional>
#include <string>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "url/gurl.h"

namespace policy {
namespace {
// Maximum number of characters a user can write in the justification text area.
constexpr int kMaxBypassJustificationLength = 280;

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
      NOTREACHED_IN_MIGRATION();
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

const std::u16string GetJustificationLabelText(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_JUSTIFICATION_LABEL);
    case dlp::FileAction::kUpload:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_JUSTIFICATION_LABEL);
    case dlp::FileAction::kCopy:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_COPY_JUSTIFICATION_LABEL);
    case dlp::FileAction::kMove:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_MOVE_JUSTIFICATION_LABEL);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_OPEN_JUSTIFICATION_LABEL);
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_JUSTIFICATION_LABEL);
  }
}

}  // namespace

FilesPolicyWarnDialog::FilesPolicyWarnDialog(
    WarningWithJustificationCallback callback,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent,
    std::optional<DlpFileDestination> destination,
    Info dialog_info)
    : FilesPolicyDialog(dialog_info.GetFiles().size(), action, modal_parent),
      destination_(destination),
      dialog_info_(dialog_info) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(&FilesPolicyWarnDialog::ProceedWarning,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(split.first)));
  SetCancelCallback(base::BindOnce(&FilesPolicyWarnDialog::CancelWarning,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(split.second)));
  SetButtonLabel(ui::mojom::DialogButton::kOk, GetOkButton());
  SetButtonLabel(ui::mojom::DialogButton::kCancel, GetCancelButton());

  AddGeneralInformation();
  if (dialog_info_.GetLearnMoreURL().has_value()) {
    files_dialog_utils::AddLearnMoreLink(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE),
        dialog_info.GetAccessibleLearnMoreLinkName(),
        dialog_info_.GetLearnMoreURL().value(), upper_panel_);
  }
  MaybeAddConfidentialRows();
  MaybeAddJustificationPanel();

  data_controls::DlpHistogramEnumeration(
      data_controls::dlp::kFileActionWarnReviewedUMA, action);
}

FilesPolicyWarnDialog::~FilesPolicyWarnDialog() = default;

size_t FilesPolicyWarnDialog::GetMaxBypassJustificationLengthForTesting()
    const {
  return kMaxBypassJustificationLength;
}

void FilesPolicyWarnDialog::MaybeAddConfidentialRows() {
  if (action_ == dlp::FileAction::kDownload ||
      dialog_info_.GetFiles().empty()) {
    return;
  }

  SetupScrollView();
  for (const auto& file : dialog_info_.GetFiles()) {
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
      case dlp::FileAction::kUnknown:  // TODO(crbug.com/40238129)
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
    case dlp::FileAction::kUnknown:  // TODO(crbug.com/40238129)
                                     // Set proper text when file
                                     // action is unknown
      return l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_TRANSFER_WARN_TITLE, file_count_);
  }
}

std::u16string FilesPolicyWarnDialog::GetMessage() {
  if (base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    return dialog_info_.GetMessage();
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
    WarningWithJustificationCallback callback) {
  std::optional<std::u16string> user_justification;
  if (justification_field_) {
    user_justification = justification_field_->GetText();
  }
  std::move(callback).Run(user_justification,
                          /*should_proceed=*/true);
}

void FilesPolicyWarnDialog::CancelWarning(
    WarningWithJustificationCallback callback) {
  std::move(callback).Run(/*user_justification=*/std::nullopt,
                          /*should_proceed=*/false);
}

void FilesPolicyWarnDialog::MaybeAddJustificationPanel() {
  if (!dialog_info_.DoesBypassRequireJustification()) {
    return;
  }

  // Disable the proceed button until some text is entered.
  DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, false);

  views::View* justification_panel =
      AddChildView(std::make_unique<views::View>());
  justification_panel->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets::TLBR(8, 24, 8, 24),
      /*between_child_spacing=*/8));

  const std::u16string justification_label_text =
      GetJustificationLabelText(action_);

  views::Label* justification_field_label = justification_panel->AddChildView(
      std::make_unique<views::Label>(justification_label_text));
  justification_field_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  justification_field_label->SetAllowCharacterBreak(true);
  justification_field_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosLabel1));
  justification_field_label->SetEnabledColor(
      ash::ColorProvider::Get()->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorPrimary));

  // Setting a themed rounded background does not work for text areas. As a
  // workaround we set it for an external container and set the text area
  // background to transparent.
  views::View* justification_field_container =
      justification_panel->AddChildView(std::make_unique<views::View>());
  justification_field_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  justification_field_container->SetBackground(
      views::CreateThemedRoundedRectBackground(
          ash::kColorAshControlBackgroundColorInactive, 8, 0));

  justification_field_ = justification_field_container->AddChildView(
      std::make_unique<views::Textarea>());
  justification_field_->SetID(
      PolicyDialogBase::kEnterpriseConnectorsJustificationTextareaId);
  justification_field_->GetViewAccessibility().SetName(
      justification_label_text);
  justification_field_->GetViewAccessibility().SetDescription(
      l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_FILES_JUSTIFICATION_TEXTAREA_ACCESSIBLE_DESCRIPTION,
          base::NumberToString16(0),
          base::NumberToString16(kMaxBypassJustificationLength)));
  justification_field_->SetController(this);
  justification_field_->SetBackgroundColor(SK_ColorTRANSPARENT);
  justification_field_->SetPreferredSize(
      gfx::Size(justification_field_->GetPreferredSize().width(), 140));
  justification_field_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(8, 16, 8, 16)));
  justification_field_->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody2));

  justification_field_length_label_ =
      justification_panel->AddChildView(std::make_unique<views::Label>());
  justification_field_length_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  justification_field_length_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_BYPASS_JUSTIFICATION_TEXT_LIMIT_LABEL,
      base::NumberToString16(0),
      base::NumberToString16(kMaxBypassJustificationLength)));
  justification_field_length_label_->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosLabel2));
}

void FilesPolicyWarnDialog::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  if (justification_field_length_label_) {
    justification_field_length_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_BYPASS_JUSTIFICATION_TEXT_LIMIT_LABEL,
        base::NumberToString16(new_contents.size()),
        base::NumberToString16(kMaxBypassJustificationLength)));
    justification_field_->GetViewAccessibility().SetDescription(
        l10n_util::GetStringFUTF16(
            IDS_POLICY_DLP_FILES_JUSTIFICATION_TEXTAREA_ACCESSIBLE_DESCRIPTION,
            base::NumberToString16(new_contents.size()),
            base::NumberToString16(kMaxBypassJustificationLength)));
  }

  if (new_contents.size() == 0 ||
      new_contents.size() > kMaxBypassJustificationLength) {
    DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, false);
  } else {
    DialogDelegate::SetButtonEnabled(ui::mojom::DialogButton::kOk, true);
  }
}

BEGIN_METADATA(FilesPolicyWarnDialog)
END_METADATA

}  // namespace policy
