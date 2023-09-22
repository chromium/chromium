// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"

#include "base/notreached.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

namespace policy {

FilesPolicyDialog::BlockReason GetEnterpriseConnectorsBlockReason(
    const enterprise_connectors::FileTransferAnalysisDelegate::
        FileTransferAnalysisResult& result) {
  CHECK(result.IsUnknown() || result.IsBlocked());

  if (result.IsUnknown()) {
    return FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknown;
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
      return FilesPolicyDialog::BlockReason::kEnterpriseConnectorsEncryptedFile;
    }
    if (result.final_result().value() ==
        enterprise_connectors::FinalContentAnalysisResult::LARGE_FILES) {
      return policy::FilesPolicyDialog::BlockReason::
          kEnterpriseConnectorsLargeFile;
    }

    NOTREACHED()
        << "Enterprise connector result representing a blocked transfer "
           "without a tag but with an unexpected final result value.";

    return FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknown;
  }

  DCHECK(result.tag() == enterprise_connectors::kDlpTag ||
         result.tag() == enterprise_connectors::kMalwareTag);

  if (result.tag() == enterprise_connectors::kDlpTag) {
    return policy::FilesPolicyDialog::BlockReason::
        kEnterpriseConnectorsSensitiveData;
  }
  if (result.tag() == enterprise_connectors::kMalwareTag) {
    return FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware;
  }

  NOTREACHED() << "Enterprise connector result representing a blocked transfer "
                  "with an unexpected tag.";

  return FilesPolicyDialog::BlockReason::kEnterpriseConnectorsUnknown;
}

}  // namespace policy
