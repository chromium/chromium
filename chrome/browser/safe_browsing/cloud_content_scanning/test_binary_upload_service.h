// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_TEST_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_TEST_BINARY_UPLOAD_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"

namespace safe_browsing {

class TestBinaryUploadService
    : public enterprise_connectors::BinaryUploadService {
 public:
  TestBinaryUploadService();
  ~TestBinaryUploadService() override;

  void MaybeUploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request)
      override;
  void MaybeAcknowledge(
      std::unique_ptr<enterprise_connectors::BinaryUploadAck> ack) override {}
  void MaybeCancelRequests(
      std::unique_ptr<enterprise_connectors::BinaryUploadCancelRequests> cancel)
      override {}
  base::WeakPtr<enterprise_connectors::BinaryUploadService> AsWeakPtr()
      override;
  void SetResponse(enterprise_connectors::ScanRequestUploadResult result,
                   enterprise_connectors::ContentAnalysisResponse response);

  bool was_called() { return was_called_; }
  const enterprise_connectors::ContentAnalysisRequest& last_request() {
    return last_request_;
  }
  void ClearWasCalled();

 private:
  enterprise_connectors::ContentAnalysisRequest last_request_;
  enterprise_connectors::ScanRequestUploadResult saved_result_ =
      enterprise_connectors::ScanRequestUploadResult::kUnknown;
  enterprise_connectors::ContentAnalysisResponse saved_response_;
  bool was_called_ = false;
  base::WeakPtrFactory<TestBinaryUploadService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_TEST_BINARY_UPLOAD_SERVICE_H_
