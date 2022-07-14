// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_

#include <memory>

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {

// This class encapsulates the process of sending a file to local content
// analysis agents for deep scanning and asynchronously retrieving a verdict.
// This class runs on the UI thread.
class LocalBinaryUploadService : public safe_browsing::BinaryUploadService {
 public:
  explicit LocalBinaryUploadService(
      std::unique_ptr<AnalysisSettings> analysis_settings);
  ~LocalBinaryUploadService() override;

  // Send the given file contents to local partners for deep scanning.
  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;

 private:
  void DoLocalContentAnalysis(std::unique_ptr<Request> request,
                              Result result,
                              Request::Data data);

  // Updates response to Chrome based on whether the SDK request is successfully
  // sent.
  void OnSentRequestStatus(
      std::unique_ptr<Request> request,
      absl::optional<content_analysis::sdk::ContentAnalysisResponse> response);

  std::unique_ptr<AnalysisSettings> analysis_settings_;
  std::unique_ptr<content_analysis::sdk::Client> client_;
  base::WeakPtrFactory<LocalBinaryUploadService> weakptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
