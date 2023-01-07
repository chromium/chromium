// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_ANALYSIS_REQUEST_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_ANALYSIS_REQUEST_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

namespace enterprise_connectors {

// A BinaryUploadService::Request implementation that gets the content of a
// to-be-printed web page through a base::ReadOnlySharedMemoryRegion.
class PagePrintAnalysisRequest
    : public safe_browsing::BinaryUploadService::Request {
 public:
  PagePrintAnalysisRequest(
      const AnalysisSettings& analysis_settings,
      base::ReadOnlySharedMemoryRegion page,
      safe_browsing::BinaryUploadService::ContentAnalysisCallback callback);
  ~PagePrintAnalysisRequest() override;

  void GetRequestData(DataCallback callback) override;

 private:
  base::ReadOnlySharedMemoryRegion page_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_ANALYSIS_REQUEST_H_
