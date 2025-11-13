// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/clipboard_analysis_request.h"

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

ClipboardAnalysisRequest::ClipboardAnalysisRequest(
    CloudOrLocalAnalysisSettings settings,
    std::string text,
    safe_browsing::BinaryUploadService::ContentAnalysisCallback callback)
    : Request(std::move(callback), std::move(settings)) {
  DCHECK_GT(text.size(), 0u);
  data_.size = text.size();

  // Only remember strings less than the maximum allowed.
  if (text.size() < safe_browsing::BinaryUploadService::kMaxUploadSizeBytes) {
    data_.contents = std::move(text);
    result_ = ScanRequestUploadResult::SUCCESS;
  }
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_TEXT_UPLOADS);
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::TOTAL_TEXT_UPLOADS);
}

ClipboardAnalysisRequest::~ClipboardAnalysisRequest() {
  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_TEXT_UPLOADS);
}

void ClipboardAnalysisRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

}  // namespace enterprise_connectors
