// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

namespace enterprise_connectors {

// This class encapsulates the process of sending a file to local content
// analysis agents for deep scanning and asynchronously retrieving a verdict.
// This class runs on the UI thread.
class LocalBinaryUploadService : public safe_browsing::BinaryUploadService {
 public:
  LocalBinaryUploadService();
  ~LocalBinaryUploadService() override;

  // Send the given file contents to local partners for deep scanning.
  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
