// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxPageSize = 50 * 1024 * 1024;

}  // namespace

PagePrintAnalysisRequest::PagePrintAnalysisRequest(
    const AnalysisSettings& analysis_settings,
    base::ReadOnlySharedMemoryRegion page,
    safe_browsing::BinaryUploadService::ContentAnalysisCallback callback)
    : safe_browsing::BinaryUploadService::Request(
          std::move(callback),
          analysis_settings.cloud_or_local_settings),
      page_(std::move(page)) {
  DCHECK(page_.IsValid());
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_PRINTS);
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::TOTAL_PRINTS);
}

PagePrintAnalysisRequest::~PagePrintAnalysisRequest() {
  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_PRINTS);
}

void PagePrintAnalysisRequest::GetRequestData(DataCallback callback) {
  Data data;
  data.size = page_.GetSize();
  data.page = page_.Duplicate();

  std::move(callback).Run(
      // Only enforce a max size for cloud scans.
      data.size >= kMaxPageSize && cloud_or_local_settings().is_cloud_analysis()
          ? safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE
          : safe_browsing::BinaryUploadService::Result::SUCCESS,
      std::move(data));
}

}  // namespace enterprise_connectors
