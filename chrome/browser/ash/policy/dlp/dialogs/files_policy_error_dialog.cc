// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include "ash/style/typography.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace policy {

FilesPolicyErrorDialog::FilesPolicyErrorDialog(
    const std::map<DlpConfidentialFile, Policy>& files,
    DlpFileDestination destination,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent)
    : FilesPolicyDialog(files.size(), destination, action, modal_parent),
      files_(files) {
  SetAcceptCallback(base::BindOnce(&FilesPolicyErrorDialog::Dismiss,
                                   weak_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(&FilesPolicyErrorDialog::OpenHelpPage,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  AddGeneralInformation();
  MaybeAddConfidentialRows();
}

FilesPolicyErrorDialog::~FilesPolicyErrorDialog() = default;

void FilesPolicyErrorDialog::MaybeAddConfidentialRows() {
  if (files_.empty()) {
    return;
  }

  SetupScrollView();
  std::map<Policy, std::vector<DlpConfidentialFile>> policy_to_blocked_files;
  for (const auto& file : files_) {
    if (!base::Contains(policy_to_blocked_files, file.second)) {
      policy_to_blocked_files.emplace(
          file.second, std::vector<DlpConfidentialFile>({file.first}));
    } else {
      policy_to_blocked_files.at(file.second).push_back(file.first);
    }
  }
  for (const auto& reason : policy_to_blocked_files) {
    AddPolicyRow(reason.first);
    for (const auto& file : reason.second) {
      AddConfidentialRow(file.icon, file.title);
    }
  }
}

// TODO(b/279435843): Replace with translation strings.
std::u16string FilesPolicyErrorDialog::GetOkButton() {
  return u"Dismiss";
}

std::u16string FilesPolicyErrorDialog::GetCancelButton() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

// TODO(b/279435843): Replace with translation strings.
std::u16string FilesPolicyErrorDialog::GetTitle() {
  switch (action_) {
    case dlp::FileAction::kDownload:
      return u"Files blocked from downloading";
    case dlp::FileAction::kUpload:
      return u"Files blocked from uploading";
    case dlp::FileAction::kCopy:
      return u"Files blocked from copying";
    case dlp::FileAction::kMove:
      return u"Files blocked from moving";
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return u"Files blocked from opening";
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:  // TODO(crbug.com/1361900)
                                     // Set proper text when file
                                     // action is unknown
      return u"Files blocked from transferring";
  }
}

std::u16string FilesPolicyErrorDialog::GetMessage() {
  // Error dialogs don't have a single message, but rather add one or two policy
  // reasons for blocked files in `AddPolicyRow()`.
  return u"";
}

void FilesPolicyErrorDialog::AddPolicyRow(Policy policy) {
  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(10, 16, 10, 16), 0));

  // TODO(b/279435843): Replace with translation strings.
  views::Label* title_label =
      AddRowTitle(u"Files were blocked because of policy", row);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
}

void FilesPolicyErrorDialog::OpenHelpPage() {
  // TODO(b/283786134): Implementation.
}

void FilesPolicyErrorDialog::Dismiss() {
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

BEGIN_METADATA(FilesPolicyErrorDialog, FilesPolicyDialog)
END_METADATA

}  // namespace policy
