// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_metadata.h"

namespace safe_browsing {

DeepScanningMetadata::DownloadScopedObservation::DownloadScopedObservation(
    DeepScanningMetadata* metadata,
    download::DownloadItem::Observer* observer)
    : metadata_(metadata) {
  observation_ = std::make_unique<base::ScopedObservation<
      download::DownloadItem, download::DownloadItem::Observer>>(observer);
}

DeepScanningMetadata::DownloadScopedObservation::~DownloadScopedObservation() {
  // Remove observation from the list of observers.
  if (observation_ && metadata_) {
    metadata_->RemoveObservation(observation_->GetObserver());
  }
}

void DeepScanningMetadata::DownloadScopedObservation::Stop() {
  // Stop observing the `DownloadItem`.
  if (observation_ && observation_->IsObserving()) {
    observation_->Reset();
  }
}

void DeepScanningMetadata::DownloadScopedObservation::Observe(
    download::DownloadItem* source) {
  if (observation_) {
    observation_->Observe(source);
  }
}

DownloadCheckResult DeepScanningMetadata::MaybeOverrideScanResult(
    DownloadCheckResultReason reason,
    DownloadCheckResult deep_scan_result) {
  DownloadCheckResult result = deep_scan_result;

  switch (deep_scan_result) {
    // These results are more dangerous or equivalent to any |reason|, so they
    // take precedence.
    case DownloadCheckResult::DANGEROUS_HOST:
    case DownloadCheckResult::DANGEROUS:
    case DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE:
      break;

    // These deep scanning results don't override any dangerous reasons.
    case DownloadCheckResult::UNKNOWN:
    case DownloadCheckResult::SENSITIVE_CONTENT_WARNING:
    case DownloadCheckResult::DEEP_SCANNED_SAFE:
    case DownloadCheckResult::DEEP_SCANNED_FAILED:
    case DownloadCheckResult::SAFE:
    case DownloadCheckResult::PROMPT_FOR_SCANNING:
    case DownloadCheckResult::PROMPT_FOR_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::POTENTIALLY_UNWANTED:
    case DownloadCheckResult::UNCOMMON:
    case DownloadCheckResult::IMMEDIATE_DEEP_SCAN:
      if (reason == REASON_DOWNLOAD_DANGEROUS) {
        result = DownloadCheckResult::DANGEROUS;
      } else if (reason == REASON_DOWNLOAD_DANGEROUS_HOST) {
        result = DownloadCheckResult::DANGEROUS_HOST;
      } else if (reason == REASON_DOWNLOAD_POTENTIALLY_UNWANTED) {
        result = DownloadCheckResult::POTENTIALLY_UNWANTED;
      } else if (reason == REASON_DOWNLOAD_UNCOMMON) {
        result = DownloadCheckResult::UNCOMMON;
      } else if (reason == REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE) {
        result = DownloadCheckResult::DANGEROUS_ACCOUNT_COMPROMISE;
      }
      break;

    // These other results have precedence over dangerous ones because they
    // indicate the scan is not done, that the file is blocked for another
    // reason, or that the file is allowed by policy.
    case DownloadCheckResult::ASYNC_SCANNING:
    case DownloadCheckResult::ASYNC_LOCAL_PASSWORD_SCANNING:
    case DownloadCheckResult::BLOCKED_PASSWORD_PROTECTED:
    case DownloadCheckResult::BLOCKED_TOO_LARGE:
    case DownloadCheckResult::SENSITIVE_CONTENT_BLOCK:
    case DownloadCheckResult::FORCE_SAVE_TO_GDRIVE:
    case DownloadCheckResult::ALLOWLISTED_BY_POLICY:
    case DownloadCheckResult::BLOCKED_SCAN_FAILED:
      break;
  }

  return result;
}

}  // namespace safe_browsing
