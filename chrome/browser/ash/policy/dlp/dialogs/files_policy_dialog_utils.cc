// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/typography.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/link.h"

namespace policy::files_dialog_utils {

FilesPolicyDialog::BlockReason GetEnterpriseConnectorsBlockReason(
    const enterprise_connectors::FileTransferAnalysisDelegate::
        FileTransferAnalysisResult& result) {
  CHECK(result.IsUnknown() || result.IsBlocked());

  if (result.IsUnknown()) {
    return FilesPolicyDialog::BlockReason::
        kEnterpriseConnectorsUnknownScanResult;
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

    if (result.final_result().value() ==
        enterprise_connectors::FinalContentAnalysisResult::FAIL_CLOSED) {
      return policy::FilesPolicyDialog::BlockReason::
          kEnterpriseConnectorsScanFailed;
    }

    NOTREACHED_IN_MIGRATION()
        << "Enterprise connector result representing a blocked transfer "
           "without a tag but with an unexpected final result value.";

    return FilesPolicyDialog::BlockReason::kEnterpriseConnectors;
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

  NOTREACHED_IN_MIGRATION()
      << "Enterprise connector result representing a blocked transfer "
         "with an unexpected tag.";

  return FilesPolicyDialog::BlockReason::kEnterpriseConnectors;
}

policy::FilesPolicyDialog::Info GetDialogInfoForEnterpriseConnectorsBlockReason(
    policy::FilesPolicyDialog::BlockReason reason,
    const std::vector<base::FilePath>& paths,
    const std::vector<
        std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>>&
        file_transfer_analysis_delegates) {
  auto dialog_settings = policy::FilesPolicyDialog::Info::Error(reason, paths);

  // Find the first valid delegate, since every delegate contains the same copy
  // of custom messaging settings.
  auto delegate = base::ranges::find_if(
      file_transfer_analysis_delegates,
      [](const std::unique_ptr<
          enterprise_connectors::FileTransferAnalysisDelegate>& delegate) {
        return delegate != nullptr;
      });

  if (delegate == file_transfer_analysis_delegates.end()) {
    return dialog_settings;
  }

  std::string tag;
  // Currently, only sensitive data and malware block reasons support custom
  // admin defined dialog settings.
  if (reason == policy::FilesPolicyDialog::BlockReason::
                    kEnterpriseConnectorsSensitiveData) {
    tag = enterprise_connectors::kDlpTag;
  } else if (reason == policy::FilesPolicyDialog::BlockReason::
                           kEnterpriseConnectorsMalware) {
    tag = enterprise_connectors::kMalwareTag;
  } else {
    return dialog_settings;
  }

  // Override default values with admin defined ones.
  dialog_settings.SetMessage(delegate->get()->GetCustomMessage(tag));
  dialog_settings.SetLearnMoreURL(delegate->get()->GetCustomLearnMoreUrl(tag));

  return dialog_settings;
}

void AddLearnMoreLink(const std::u16string& text,
                      const std::u16string& accessible_name,
                      const GURL& url,
                      views::View* view) {
  views::Link* learn_more_link =
      view->AddChildView(std::make_unique<views::Link>(text));
  learn_more_link->SetCallback(base::BindRepeating(&dlp::OpenLearnMore, url));
  learn_more_link->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
  learn_more_link->SetEnabledColor(
      ash::ColorProvider::Get()->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorURL));
  learn_more_link->GetViewAccessibility().SetName(accessible_name);
}

}  // namespace policy::files_dialog_utils
