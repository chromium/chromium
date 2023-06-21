// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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

void FilesPolicyErrorDialog::AddPolicyRow(Policy policy) {
  // TODO(b/280780100): Implementation.
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
