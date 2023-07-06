// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include <string>

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

namespace {
// Returns the block reason description for `policy`.
std::u16string GetPolicyString(Policy policy) {
  switch (policy) {
    case Policy::kDlp:
      return u"Files were blocked because of policy";
    case Policy::kEnterpriseConnectors:
      return u"Files were blocked because of content";
  }
}
}  // namespace

FilesPolicyErrorDialog::FilesPolicyErrorDialog(
    const std::map<DlpConfidentialFile, Policy>& files,
    DlpFileDestination destination,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent)
    : FilesPolicyDialog(files.size(), destination, action, modal_parent) {
  SetAcceptCallback(base::BindOnce(&FilesPolicyErrorDialog::Dismiss,
                                   weak_factory_.GetWeakPtr()));
  SetCancelCallback(base::BindOnce(&FilesPolicyErrorDialog::OpenHelpPage,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, GetCancelButton());

  for (const auto& file : files) {
    if (!base::Contains(files_, file.second)) {
      files_.emplace(file.second, std::vector<DlpConfidentialFile>{file.first});
    } else {
      files_.at(file.second).push_back(file.first);
    }
  }
  CHECK(files_.size() == 1 || files_.size() == 2);

  AddGeneralInformation();
  MaybeAddConfidentialRows();
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
  // Single error dialogs specify the policy reason before the scrollable list.
  if (files_.size() == 1) {
    return GetPolicyString(files_.begin()->first);
  }
  // Mixed error dialogs don't have a single message, but use `AddPolicyRow()`
  // to add the policy reason directly in the scrollable file list.
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
  views::Label* title_label = AddRowTitle(GetPolicyString(policy), row);
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
