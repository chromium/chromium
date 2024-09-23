// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/style/typography.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace policy {

namespace {

// Returns the accessible name for the learn more link of the given block
// `reason`.
std::u16string GetAccessibleLearnMoreLinkNameForBlockReason(
    policy::FilesPolicyDialog::BlockReason reason) {
  switch (reason) {
    case FilesPolicyDialog::BlockReason::kDlp:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_LEARN_MORE_ABOUT_DATA_CONTROLS_ACCESSIBLE_NAME);
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_LEARN_MORE_ABOUT_SENSITIVE_DATA_PROTECTION_ACCESSIBLE_NAME);
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_LEARN_MORE_ABOUT_MALWARE_PROTECTION_ACCESSIBLE_NAME);
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsScanFailed:
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknownScanResult:
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsEncryptedFile:
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsLargeFile:
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectors:
      // Currently these block reasons cannot have a learn more link.
      return std::u16string();
  }
}

}  // namespace

FilesPolicyDialogFactory* factory_;

// static
PolicyDialogBase::ViewIds FilesPolicyDialog::MapBlockReasonToViewID(
    BlockReason reason) {
  switch (reason) {
    case FilesPolicyDialog::BlockReason::kDlp:
      return PolicyDialogBase::kDlpSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknownScanResult:
      return PolicyDialogBase::kEnterpriseConnectorsUnknownScanResultSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsScanFailed:
      return PolicyDialogBase::kEnterpriseConnectorsScanFailedResultSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData:
      return PolicyDialogBase::kEnterpriseConnectorsSensitiveDataSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware:
      return PolicyDialogBase::kEnterpriseConnectorsMalwareSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsEncryptedFile:
      return PolicyDialogBase::kEnterpriseConnectorsEncryptedFileSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectorsLargeFile:
      return PolicyDialogBase::kEnterpriseConnectorsLargeFileSectionId;
    case FilesPolicyDialog::BlockReason::kEnterpriseConnectors:
      return PolicyDialogBase::kEnterpriseConnectorsSectionId;
  }
}

// static
FilesPolicyDialog::Info FilesPolicyDialog::Info::Warn(
    BlockReason reason,
    const std::vector<base::FilePath>& paths) {
  CHECK(!paths.empty());

  Info settings;
  settings.files_ =
      std::vector<DlpConfidentialFile>(paths.begin(), paths.end());
  // TODO(b/300705572): we probably want to have a default message for every
  // block reason.
  int message_id = IDS_POLICY_DLP_FILES_WARN_MESSAGE;
  settings.message_ = base::ReplaceStringPlaceholders(
      l10n_util::GetPluralStringFUTF16(message_id, paths.size()),
      base::NumberToString16(paths.size()),
      /*offset=*/nullptr);
  // Only DLP has a default learn more URL.
  if (reason == FilesPolicyDialog::BlockReason::kDlp) {
    settings.learn_more_url_ = GURL(dlp::kDlpLearnMoreUrl);
  }

  settings.accessible_learn_more_link_name_ =
      GetAccessibleLearnMoreLinkNameForBlockReason(reason);

  return settings;
}

// static
FilesPolicyDialog::Info FilesPolicyDialog::Info::Error(
    BlockReason reason,
    const std::vector<base::FilePath>& paths) {
  CHECK(!paths.empty());

  size_t file_count = paths.size();

  Info settings;
  settings.files_ =
      std::vector<DlpConfidentialFile>(paths.begin(), paths.end());
  settings.message_ =
      files_string_util::GetBlockReasonMessage(reason, file_count);
  // Only DLP has a default learn more URL.
  if (reason == FilesPolicyDialog::BlockReason::kDlp) {
    settings.learn_more_url_ = GURL(dlp::kDlpLearnMoreUrl);
  }

  settings.accessible_learn_more_link_name_ =
      GetAccessibleLearnMoreLinkNameForBlockReason(reason);

  return settings;
}

FilesPolicyDialog::Info::Info() = default;

FilesPolicyDialog::Info::~Info() = default;

FilesPolicyDialog::Info::Info(const Info& other) = default;

FilesPolicyDialog::Info& FilesPolicyDialog::Info::operator=(Info&& other) =
    default;

bool FilesPolicyDialog::Info::operator==(const Info& other) const {
  return bypass_requires_justification_ ==
             other.bypass_requires_justification_ &&
         message_ == other.message_ &&
         learn_more_url_ == other.learn_more_url_ && files_ == other.files_ &&
         accessible_learn_more_link_name_ ==
             other.accessible_learn_more_link_name_;
}

bool FilesPolicyDialog::Info::operator!=(const Info& other) const {
  return !(*this == other);
}

const std::vector<DlpConfidentialFile>& FilesPolicyDialog::Info::GetFiles()
    const {
  return files_;
}

bool FilesPolicyDialog::Info::DoesBypassRequireJustification() const {
  return bypass_requires_justification_;
}

void FilesPolicyDialog::Info::SetBypassRequiresJustification(bool value) {
  bypass_requires_justification_ = value;
}

std::u16string FilesPolicyDialog::Info::GetMessage() const {
  return message_;
}

void FilesPolicyDialog::Info::SetMessage(
    const std::optional<std::u16string>& message) {
  if (message.has_value() && !message->empty()) {
    message_ = l10n_util::GetStringFUTF16(
        IDS_POLICY_DLP_FROM_YOUR_ADMIN_MESSAGE, message.value());
    is_custom_message_ = true;
  }
}

bool FilesPolicyDialog::Info::HasCustomMessage() const {
  return is_custom_message_;
}

std::optional<GURL> FilesPolicyDialog::Info::GetLearnMoreURL() const {
  return learn_more_url_;
}

void FilesPolicyDialog::Info::SetLearnMoreURL(const std::optional<GURL>& url) {
  if (url.has_value() && url->is_valid()) {
    learn_more_url_ = url.value();
  }
}

std::u16string FilesPolicyDialog::Info::GetAccessibleLearnMoreLinkName() const {
  return accessible_learn_more_link_name_;
}

bool FilesPolicyDialog::Info::HasCustomDetails() const {
  return DoesBypassRequireJustification() || HasCustomMessage() ||
         is_custom_learn_more_url_;
}

FilesPolicyDialog::FilesPolicyDialog(size_t file_count,
                                     dlp::FileAction action,
                                     gfx::NativeWindow modal_parent)
    : action_(action), file_count_(file_count) {
  ui::mojom::ModalType modal = modal_parent ? ui::mojom::ModalType::kWindow
                                            : ui::mojom::ModalType::kSystem;

  set_margins(gfx::Insets::TLBR(24, 0, 20, 0));

  SetModalType(modal);
}

FilesPolicyDialog::~FilesPolicyDialog() = default;

views::Widget* FilesPolicyDialog::CreateWarnDialog(
    WarningWithJustificationCallback callback,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent,
    Info dialog_info,
    std::optional<DlpFileDestination> destination) {
  if (factory_) {
    return factory_->CreateWarnDialog(std::move(callback), action, modal_parent,
                                      destination, std::move(dialog_info));
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyWarnDialog>(std::move(callback), action,
                                              modal_parent, destination,
                                              std::move(dialog_info)),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

views::Widget* FilesPolicyDialog::CreateErrorDialog(
    const std::map<BlockReason, Info>& dialog_info_map,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent) {
  if (factory_) {
    return factory_->CreateErrorDialog(dialog_info_map, action, modal_parent);
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyErrorDialog>(dialog_info_map, action,
                                               modal_parent),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

// static
void FilesPolicyDialog::SetFactory(FilesPolicyDialogFactory* factory) {
  delete factory_;
  factory_ = factory;
}

void FilesPolicyDialog::SetupScrollView() {
  // Call the parent class to setup the element. Do not remove.
  PolicyDialogBase::SetupScrollView();
  views::BoxLayout* layout = scroll_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(8, 8, 8, 24),
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
}

views::Label* FilesPolicyDialog::AddTitle(const std::u16string& title) {
  // Call the parent class to setup the element. Do not remove.
  views::Label* title_label = PolicyDialogBase::AddTitle(title);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosTitle1));
  return title_label;
}

views::Label* FilesPolicyDialog::AddMessage(const std::u16string& message) {
  if (message.empty()) {
    // Some dialogs, like the mixed error dialogs don't have a single message,
    // but add the error description inside the scrollable list, so skip adding
    // the element altogether.
    return nullptr;
  }
  // Call the parent class to setup the element. Do not remove.
  views::Label* message_label = PolicyDialogBase::AddMessage(message);
  message_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
  return message_label;
}

void FilesPolicyDialog::AddConfidentialRow(const gfx::ImageSkia& icon,
                                           const std::u16string& title) {
  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(10, 16, 10, 16), /*between_child_spacing=*/16));

  AddRowIcon(icon, row);

  views::Label* title_label = AddRowTitle(title, row);
  title_label->SetID(PolicyDialogBase::kConfidentialRowTitleViewId);
  title_label->SetMultiLine(false);
  title_label->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
}

BEGIN_METADATA(FilesPolicyDialog)
END_METADATA

}  // namespace policy
