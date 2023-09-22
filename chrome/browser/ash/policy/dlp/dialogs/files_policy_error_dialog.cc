// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include <string>

#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_string_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace policy {

namespace {
// Returns the block reason description.
std::u16string GetPolicyString(FilesPolicyDialog::BlockReason reason,
                               size_t file_count) {
  return policy::files_string_util::GetBlockReasonMessage(reason, file_count,
                                                          u"");
}
}  // namespace

FilesPolicyErrorDialog::FilesPolicyErrorDialog(
    const std::map<DlpConfidentialFile, FilesPolicyDialog::BlockReason>& files,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent)
    : FilesPolicyDialog(files.size(), action, modal_parent) {
  SetAcceptCallback(base::BindOnce(&FilesPolicyErrorDialog::Dismiss,
                                   weak_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(&FilesPolicyErrorDialog::OpenLearnMore,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  for (const auto& [file, reason] : files) {
    if (!base::Contains(files_, reason)) {
      files_.emplace(reason, std::vector<DlpConfidentialFile>{file});
    } else {
      files_.at(reason).push_back(file);
    }
  }
  file_count_ = files.size();

  AddGeneralInformation();
  MaybeAddConfidentialRows();

  DlpHistogramEnumeration(dlp::kFileActionBlockReviewedUMA, action);
}

FilesPolicyErrorDialog::~FilesPolicyErrorDialog() = default;

void FilesPolicyErrorDialog::MaybeAddConfidentialRows() {
  if (files_.empty()) {
    return;
  }

  SetupScrollView();
  for (const auto& reason : files_) {
    if (files_.size() > 1) {
      // Only add the reason if it's a mixed errors dialog.
      AddPolicyRow(reason.first);
    }
    for (const auto& file : reason.second) {
      AddConfidentialRow(file.icon, file.title);
    }
  }
}

std::u16string FilesPolicyErrorDialog::GetOkButton() {
  return l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OK_BUTTON);
}

std::u16string FilesPolicyErrorDialog::GetCancelButton() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

std::u16string FilesPolicyErrorDialog::GetTitle() {
  return policy::files_string_util::GetBlockTitle(action_, file_count_);
}

std::u16string FilesPolicyErrorDialog::GetMessage() {
  // Single error dialogs specify the policy reason before the scrollable list.
  if (files_.size() == 1) {
    return GetPolicyString(files_.begin()->first,
                           files_.begin()->second.size());
  }
  // Mixed error dialogs don't have a single message, but use `AddPolicyRow()`
  // to add the policy reason directly in the scrollable file list.
  return u"";
}

void FilesPolicyErrorDialog::AddPolicyRow(
    FilesPolicyDialog::BlockReason reason) {
  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(10, 16, 10, 16), 0));

  views::Label* title_label =
      AddRowTitle(GetPolicyString(reason, files_.at(reason).size()), row);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
}

void FilesPolicyErrorDialog::OpenLearnMore() {
  // TODO(b/291896216): Open page based on policy.
  dlp::OpenLearnMore();
}

void FilesPolicyErrorDialog::Dismiss() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

BEGIN_METADATA(FilesPolicyErrorDialog, FilesPolicyDialog)
END_METADATA

}  // namespace policy
