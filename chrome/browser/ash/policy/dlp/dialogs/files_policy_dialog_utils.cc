// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"

#include "base/notreached.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

namespace policy {

FilesPolicyDialog::EnterpriseConnectorsBlockReason
GetEnterpriseConnectorsBlockReason(
    const enterprise_connectors::FileTransferAnalysisDelegate::
        FileTransferAnalysisResult& result) {
  CHECK(result.IsUnknown() || result.IsBlocked());

  if (result.IsUnknown()) {
    return FilesPolicyDialog::EnterpriseConnectorsBlockReason::kUnknown;
  }

  // Blocked files without a tag may happen for several reasons including
  // files too large to be scanned or encrypted files.
  if (result.tag().empty() && result.final_result().has_value()) {
    DCHECK(result.final_result().value() ==
               enterprise_connectors::FinalContentAnalysisResult::
                   ENCRYPTED_FILES ||
           result.final_result().value() ==
               enterprise_connectors::FinalContentAnalysisResult::LARGE_FILES);

    if (result.final_result().value() ==
        enterprise_connectors::FinalContentAnalysisResult::ENCRYPTED_FILES) {
      return FilesPolicyDialog::EnterpriseConnectorsBlockReason::kEncryptedFile;
    }
    if (result.final_result().value() ==
        enterprise_connectors::FinalContentAnalysisResult::LARGE_FILES) {
      return policy::FilesPolicyDialog::EnterpriseConnectorsBlockReason::
          kLargeFile;
    }

    NOTREACHED()
        << "Enterprise connector result representing a blocked transfer "
           "without a tag but with an unexpected final result value.";

    return FilesPolicyDialog::EnterpriseConnectorsBlockReason::kUnknown;
  }

  DCHECK(result.tag() == enterprise_connectors::kDlpTag ||
         result.tag() == enterprise_connectors::kMalwareTag);

  if (result.tag() == enterprise_connectors::kDlpTag) {
    return policy::FilesPolicyDialog::EnterpriseConnectorsBlockReason::
        kSensitiveData;
  }
  if (result.tag() == enterprise_connectors::kMalwareTag) {
    return FilesPolicyDialog::EnterpriseConnectorsBlockReason::kMalware;
  }

  NOTREACHED() << "Enterprise connector result representing a blocked transfer "
                  "with an unexpected tag.";

  return FilesPolicyDialog::EnterpriseConnectorsBlockReason::kUnknown;
}

}  // namespace policy
