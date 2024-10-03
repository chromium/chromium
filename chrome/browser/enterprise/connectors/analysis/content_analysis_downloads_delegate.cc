// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace enterprise_connectors {

namespace {

// Deobfuscates the entire file. Placed here to allow the deobfuscation and file
// opening to proceed independently of the delegate's lifecycle.
void DeobfuscateAndOpen(base::FilePath file_path,
                        base::OnceClosure open_file_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&enterprise_obfuscation::DeobfuscateFileInPlace,
                     std::move(file_path)),
      base::BindOnce(
          [](base::OnceClosure open_file_callback,
             base::expected<void, enterprise_obfuscation::Error> result) {
            if (!result.has_value()) {
              // TODO(b/367259664): Add better error handling for deobfuscation.
              DVLOG(1) << "Failed to deobfuscate file.";
            }

            if (open_file_callback) {
              std::move(open_file_callback).Run();
            }
          },
          std::move(open_file_callback)));
}

}  // namespace

ContentAnalysisDownloadsDelegate::ContentAnalysisDownloadsDelegate(
    const std::u16string& filename,
    const std::u16string& custom_message,
    GURL custom_learn_more_url,
    bool bypass_justification_required,
    base::OnceClosure open_file_callback,
    base::OnceClosure discard_file_callback,
    download::DownloadItem* download_item,
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message)
    : custom_rule_message_(custom_rule_message),
      filename_(filename),
      custom_message_(custom_message),
      custom_learn_more_url_(custom_learn_more_url),
      bypass_justification_required_(bypass_justification_required),
      open_file_callback_(std::move(open_file_callback)),
      discard_file_callback_(std::move(discard_file_callback)),
      download_item_(download_item) {
  if (download_item_) {
    download_item_->AddObserver(this);
  }
}

ContentAnalysisDownloadsDelegate::~ContentAnalysisDownloadsDelegate() {
  if (download_item_) {
    download_item_->RemoveObserver(this);
  }
}

void ContentAnalysisDownloadsDelegate::BypassWarnings(
    std::optional<std::u16string> user_justification) {
  if (download_item_) {
    enterprise_connectors::ScanResult* stored_result =
        static_cast<enterprise_connectors::ScanResult*>(
            download_item_->GetUserData(
                enterprise_connectors::ScanResult::kKey));

    if (stored_result) {
      stored_result->user_justification = user_justification;
    } else {
      auto scan_result = std::make_unique<enterprise_connectors::ScanResult>();
      scan_result->user_justification = user_justification;
      download_item_->SetUserData(enterprise_connectors::ScanResult::kKey,
                                  std::move(scan_result));
    }
  }

  // For obfuscated download files, deobfuscate the file before opening.
  enterprise_obfuscation::DownloadObfuscationData* obfuscation_data =
      static_cast<enterprise_obfuscation::DownloadObfuscationData*>(
          download_item_->GetUserData(
              enterprise_obfuscation::DownloadObfuscationData::kUserDataKey));

  if (obfuscation_data && obfuscation_data->is_obfuscated) {
    DeobfuscateAndOpen(download_item_->GetFullPath(),
                       std::move(open_file_callback_));
    ResetCallbacks();
    return;
  }

  Open();
}

void ContentAnalysisDownloadsDelegate::Open() {
  if (open_file_callback_) {
    std::move(open_file_callback_).Run();
  }
  ResetCallbacks();
}

void ContentAnalysisDownloadsDelegate::Cancel(bool warning) {
  if (discard_file_callback_)
    std::move(discard_file_callback_).Run();
  ResetCallbacks();
}

void ContentAnalysisDownloadsDelegate::ResetCallbacks() {
  discard_file_callback_.Reset();
  open_file_callback_.Reset();
}

std::optional<std::u16string>
ContentAnalysisDownloadsDelegate::GetCustomMessage() const {
  // Rule-based custom messages take precedence over policy-based.
  std::u16string custom_rule_message =
      GetCustomRuleString(custom_rule_message_);
  if (!custom_rule_message.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_CUSTOM_MESSAGE, filename_,
        custom_rule_message);
  }

  if (custom_message_.empty())
    return std::nullopt;
  return l10n_util::GetStringFUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_CUSTOM_MESSAGE, filename_,
      custom_message_);
}

std::optional<GURL> ContentAnalysisDownloadsDelegate::GetCustomLearnMoreUrl()
    const {
  // Rule-based custom messages which don't have learn more urls take
  // precedence over policy-based.
  if (custom_learn_more_url_.is_empty() ||
      !custom_rule_message_.message_segments().empty()) {
    return std::nullopt;
  }
  return custom_learn_more_url_;
}

std::optional<std::vector<std::pair<gfx::Range, GURL>>>
ContentAnalysisDownloadsDelegate::GetCustomRuleMessageRanges() const {
  std::vector<size_t> offsets;
  l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_CUSTOM_MESSAGE,
                             {filename_, std::u16string{}}, &offsets);

  std::vector<std::pair<gfx::Range, GURL>> custom_rule_message_ranges =
      GetCustomRuleStyles(custom_rule_message_, offsets.back());
  if (!custom_rule_message_ranges.empty()) {
    return custom_rule_message_ranges;
  }
  return std::nullopt;
}

bool ContentAnalysisDownloadsDelegate::BypassRequiresJustification() const {
  return bypass_justification_required_;
}

std::u16string ContentAnalysisDownloadsDelegate::GetBypassJustificationLabel()
    const {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOAD_BYPASS_JUSTIFICATION_LABEL);
}

std::optional<std::u16string>
ContentAnalysisDownloadsDelegate::OverrideCancelButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_DOWNLOADS_DISCARD_FILE_BUTTON);
}

void ContentAnalysisDownloadsDelegate::OnDownloadDestroyed(
    download::DownloadItem* download) {
  DCHECK_EQ(download, download_item_);
  download->RemoveObserver(this);
  download_item_ = nullptr;
}

}  // namespace enterprise_connectors
