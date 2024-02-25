// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_UTILS_H_

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

namespace policy::files_dialog_utils {

// Converts a file transfer analysis result representing a block or unknown
// verdict into a block reason.
FilesPolicyDialog::BlockReason GetEnterpriseConnectorsBlockReason(
    const enterprise_connectors::FileTransferAnalysisDelegate::
        FileTransferAnalysisResult& result);

// Retrieve dialog info for the given Enterprise Connectors block `reason` and
// vector of `paths`.
policy::FilesPolicyDialog::Info GetDialogInfoForEnterpriseConnectorsBlockReason(
    policy::FilesPolicyDialog::BlockReason reason,
    const std::vector<base::FilePath>& paths,
    const std::vector<
        std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>>&
        file_transfer_analysis_delegates);

// Appends a learn more link to the given `view`.
void AddLearnMoreLink(const std::u16string& text,
                      const std::u16string& accessible_name,
                      const GURL& url,
                      views::View* view);

}  // namespace policy::files_dialog_utils

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DIALOGS_FILES_POLICY_DIALOG_UTILS_H_
