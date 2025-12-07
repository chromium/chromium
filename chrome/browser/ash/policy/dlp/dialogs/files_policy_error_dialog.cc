// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include <string>

#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace policy {
namespace {

// Returns whether a custom message has been defined for the given reason.
bool HasCustomMessage(
    FilesPolicyDialog::BlockReason reason,
    const std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>&
        dialog_info_map) {
  auto it = dialog_info_map.find(reason);
  if (it == dialog_info_map.end()) {
    return false;
  }
  return it->second.HasCustomMessage();
}

// Returns files blocked for the given `reasons`.
std::vector<DlpConfidentialFile> GetFilesBlockedByReasons(
    const std::vector<FilesPolicyDialog::BlockReason>& reasons,
    const std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>&
        dialog_info_map) {
  std::vector<DlpConfidentialFile> blocked_files;
  for (FilesPolicyDialog::BlockReason reason : reasons) {
    auto it = dialog_info_map.find(reason);
    if (it == dialog_info_map.end()) {
      continue;
    }
    blocked_files.insert(blocked_files.end(), it->second.GetFiles().begin(),
                         it->second.GetFiles().end());
  }
  return blocked_files;
}

// Returns learn more links associated with the given `reasons`.
std::vector<std::pair<GURL, std::u16string>> GetLearnMoreLinks(
    const std::vector<FilesPolicyDialog::BlockReason>& reasons,
    const std::map<FilesPolicyDialog::BlockReason, FilesPolicyDialog::Info>&
        dialog_info_map) {
  std::vector<std::pair<GURL, std::u16string>> links;
  for (FilesPolicyDialog::BlockReason reason : reasons) {
    auto it = dialog_info_map.find(reason);
    if (it == dialog_info_map.end() ||
        !it->second.GetLearnMoreURL().has_value()) {
      continue;
    }
    links.emplace_back(it->second.GetLearnMoreURL().value(),
                       it->second.GetAccessibleLearnMoreLinkName());
  }
  return links;
}

}  // namespace
FilesPolicyErrorDialog::FilesPolicyErrorDialog(
    const std::map<BlockReason, Info>& dialog_info_map,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent)
    : FilesPolicyDialog(dialog_info_map.size(), action, modal_parent) {
  SetAcceptCallback(base::BindOnce(&FilesPolicyErrorDialog::Dismiss,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::mojom::DialogButton::kOk, GetOkButton());
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));

  SetupBlockedFilesSections(dialog_info_map);

  file_count_ = 0;
  for (const auto& [reason, info] : dialog_info_map) {
    file_count_ += info.GetFiles().size();
  }

  AddGeneralInformation();
  MaybeAddConfidentialRows();

  data_controls::DlpHistogramEnumeration(
      data_controls::dlp::kFileActionBlockReviewedUMA, action);
}

FilesPolicyErrorDialog::~FilesPolicyErrorDialog() = default;

FilesPolicyErrorDialog::BlockedFilesSection::BlockedFilesSection(
    int view_id,
    const std::u16string& message,
    const std::vector<DlpConfidentialFile>& files,
    const std::vector<std::pair<GURL, std::u16string>>& learn_more_urls)
    : view_id(view_id),
      message(message),
      files(files),
      learn_more_urls(learn_more_urls) {}

FilesPolicyErrorDialog::BlockedFilesSection::~BlockedFilesSection() = default;

FilesPolicyErrorDialog::BlockedFilesSection::BlockedFilesSection(
    const BlockedFilesSection& other) = default;

FilesPolicyErrorDialog::BlockedFilesSection&
FilesPolicyErrorDialog::BlockedFilesSection::operator=(
    BlockedFilesSection&& other) = default;

void FilesPolicyErrorDialog::MaybeAddConfidentialRows() {
  if (sections_.empty()) {
    return;
  }

  SetupScrollView();

  // Single error dialog.
  if (sections_.size() == 1) {
    const auto& section = sections_.front();
    for (const auto& [url, accessible_name] : section.learn_more_urls) {
      files_dialog_utils::AddLearnMoreLink(
          l10n_util::GetStringUTF16(IDS_LEARN_MORE), accessible_name, url,
          upper_panel_);
    }
    for (const auto& file : section.files) {
      AddConfidentialRow(file.icon, file.title);
    }
    return;
  }

  // Mixed error dialog.
  for (BlockedFilesSection section : sections_) {
    AddBlockedFilesSection(section);
  }
}

std::u16string FilesPolicyErrorDialog::GetOkButton() {
  return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OK_BUTTON);
}

std::u16string FilesPolicyErrorDialog::GetTitle() {
  return policy::files_string_util::GetBlockTitle(action_, file_count_);
}

std::u16string FilesPolicyErrorDialog::GetMessage() {
  // Single error dialogs specify the policy reason before the scrollable list.
  if (sections_.size() == 1) {
    return sections_.front().message;
  }
  // Mixed error dialogs don't have a single message, but use `AddPolicyRow()`
  // to add the policy reason directly in the scrollable file list.
  return u"";
}

void FilesPolicyErrorDialog::SetupBlockedFilesSections(
    const std::map<BlockReason, Info>& dialog_info_map) {
  AppendBlockedFilesSection(FilesPolicyErrorDialog::BlockReason::kDlp,
                            dialog_info_map);

  std::vector<FilesPolicyDialog::BlockReason>
      merged_enterprise_connectors_reasons(
          {FilesPolicyDialog::BlockReason::kEnterpriseConnectors,
           FilesPolicyDialog::BlockReason::
               kEnterpriseConnectorsUnknownScanResult});

  // Files with block reasons
  // - BlockReason::kEnterpriseConnectorsSensitiveData
  // - BlockReason::kEnterpriseConnectorsMalware
  // should be placed together with
  // - BlockReason::kEnterpriseConnectorsUnknownScanResult
  // - BlockReason::kEnterpriseConnectors
  // if there is no custom message specified for them.
  if (HasCustomMessage(
          FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
          dialog_info_map)) {
    AppendBlockedFilesSection(
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData,
        dialog_info_map);
  } else {
    merged_enterprise_connectors_reasons.push_back(
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsSensitiveData);
  }

  if (HasCustomMessage(
          FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
          dialog_info_map)) {
    AppendBlockedFilesSection(
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware,
        dialog_info_map);
  } else {
    merged_enterprise_connectors_reasons.push_back(
        FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware);
  }

  auto merged_enterprise_connectors_files = GetFilesBlockedByReasons(
      merged_enterprise_connectors_reasons, dialog_info_map);
  if (!merged_enterprise_connectors_files.empty()) {
    sections_.emplace_back(
        MapBlockReasonToViewID(
            FilesPolicyDialog::BlockReason::kEnterpriseConnectors),
        files_string_util::GetBlockReasonMessage(
            FilesPolicyDialog::BlockReason::kEnterpriseConnectors,
            merged_enterprise_connectors_files.size()),
        merged_enterprise_connectors_files,
        GetLearnMoreLinks(merged_enterprise_connectors_reasons,
                          dialog_info_map));
  }

  AppendBlockedFilesSection(
      FilesPolicyErrorDialog::BlockReason::kEnterpriseConnectorsScanFailed,
      dialog_info_map);
  AppendBlockedFilesSection(
      FilesPolicyErrorDialog::BlockReason::kEnterpriseConnectorsEncryptedFile,
      dialog_info_map);
  AppendBlockedFilesSection(
      FilesPolicyErrorDialog::BlockReason::kEnterpriseConnectorsLargeFile,
      dialog_info_map);
}

void FilesPolicyErrorDialog::AppendBlockedFilesSection(
    BlockReason reason,
    const std::map<BlockReason, Info>& dialog_info_map) {
  auto it = dialog_info_map.find(reason);
  if (it == dialog_info_map.end() || it->second.GetFiles().empty()) {
    return;
  }
  sections_.emplace_back(MapBlockReasonToViewID(reason),
                         it->second.GetMessage(), it->second.GetFiles(),
                         GetLearnMoreLinks({reason}, dialog_info_map));
}

void FilesPolicyErrorDialog::AddBlockedFilesSection(
    const BlockedFilesSection& section) {
  if (section.files.empty()) {
    return;
  }

  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());

  // Place title_label below into a FlexLayout which handles multi-line labels
  // properly.
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager
      ->SetDefault(views::kMarginsKey, gfx::Insets::TLBR(10, 16, 10, 16))
      .SetOrientation(views::LayoutOrientation::kVertical);
  row->SetLayoutManager(std::move(layout_manager));

  views::Label* title_label = AddRowTitle(section.message, row);
  title_label->SetID(section.view_id);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));

  // Add the learn more link if provided.
  for (const auto& [url, accessible_name] : section.learn_more_urls) {
    views::View* learn_more_row =
        scroll_view_container_->AddChildView(std::make_unique<views::View>());
    learn_more_row->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets::TLBR(0, 16, 10, 16), 0));

    files_dialog_utils::AddLearnMoreLink(
        l10n_util::GetStringUTF16(IDS_LEARN_MORE), accessible_name, url,
        learn_more_row);
  }

  for (const auto& file : section.files) {
    AddConfidentialRow(file.icon, file.title);
  }
}

void FilesPolicyErrorDialog::Dismiss() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

BEGIN_METADATA(FilesPolicyErrorDialog)
END_METADATA

}  // namespace policy
