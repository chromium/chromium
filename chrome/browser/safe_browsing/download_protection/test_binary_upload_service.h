// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TEST_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TEST_BINARY_UPLOAD_SERVICE_H_

#include <memory>

#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/proto/webprotect.pb.h"

namespace safe_browsing {

class TestBinaryUploadService : public BinaryUploadService {
 public:
  TestBinaryUploadService();
  ~TestBinaryUploadService() override = default;

  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void SetResponse(Result result, DeepScanningClientResponse response);

  bool was_called() { return was_called_; }
  void ClearWasCalled();

 private:
  Result saved_result_ = Result::UNKNOWN;
  DeepScanningClientResponse saved_response_ = DeepScanningClientResponse();
  bool was_called_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_TEST_BINARY_UPLOAD_SERVICE_H_
