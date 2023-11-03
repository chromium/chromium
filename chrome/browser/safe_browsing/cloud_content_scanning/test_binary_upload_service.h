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

namespace safe_browsing {

class TestBinaryUploadService : public BinaryUploadService {
 public:
  TestBinaryUploadService();
  ~TestBinaryUploadService() override;

  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override {}
  void MaybeCancelRequests(std::unique_ptr<CancelRequests> cancel) override {}
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;
  void SetResponse(Result result,
                   enterprise_connectors::ContentAnalysisResponse response);

  bool was_called() { return was_called_; }
  const enterprise_connectors::ContentAnalysisRequest& last_request() {
    return last_request_;
  }
  void ClearWasCalled();

 private:
  enterprise_connectors::ContentAnalysisRequest last_request_;
  Result saved_result_ = Result::UNKNOWN;
  enterprise_connectors::ContentAnalysisResponse saved_response_;
  bool was_called_ = false;
  base::WeakPtrFactory<TestBinaryUploadService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_TEST_BINARY_UPLOAD_SERVICE_H_
