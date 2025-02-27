// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_ANALYSIS_REQUEST_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_ANALYSIS_REQUEST_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

namespace enterprise_connectors {

// A `BinaryUploadService::Request` implementation that gets the data to scan
// from a string corresponding to some clipboard data (either text or image
// bytes).
class ClipboardAnalysisRequest
    : public safe_browsing::BinaryUploadService::Request {
 public:
  ClipboardAnalysisRequest(
      CloudOrLocalAnalysisSettings settings,
      std::string text,
      safe_browsing::BinaryUploadService::ContentAnalysisCallback callback);
  ~ClipboardAnalysisRequest() override;

  ClipboardAnalysisRequest(const ClipboardAnalysisRequest&) = delete;
  ClipboardAnalysisRequest& operator=(const ClipboardAnalysisRequest&) = delete;

  // safe_browsing::BinaryUploadService::Request:
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  safe_browsing::BinaryUploadService::Result result_ =
      safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_ANALYSIS_REQUEST_H_
