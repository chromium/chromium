// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace policy {

FilesPolicyWarnDialog::FilesPolicyWarnDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& files,
    DlpFileDestination destination,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent)
    : FilesPolicyDialog(files.size(), destination, action, modal_parent),
      files_(files) {
  SetOnDlpRestrictionCheckedCallback(std::move(callback));
  MaybeAddConfidentialRows();
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

BEGIN_METADATA(FilesPolicyWarnDialog, FilesPolicyDialog)
END_METADATA

}  // namespace policy
