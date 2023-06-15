// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
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
  for (const auto& file : files_) {
    // TODO(aidazolic): Add errors.
    AddConfidentialRow(file.first.icon, file.first.title);
  }
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
