// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_PENDING_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_PENDING_BINARY_UPLOAD_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_connectors::test {

// Fake `BinaryUploadService` that keeps uploaded requests pending instead of
// sending them, so that tests can inspect them and control when and how they
// complete.
class PendingBinaryUploadService : public BinaryUploadService {
 public:
  PendingBinaryUploadService();
  PendingBinaryUploadService(const PendingBinaryUploadService&) = delete;
  PendingBinaryUploadService& operator=(const PendingBinaryUploadService&) =
      delete;
  ~PendingBinaryUploadService() override;

  // Testing factory to use with
  // `safe_browsing::CloudBinaryUploadServiceFactory`.
  static std::unique_ptr<KeyedService> Create(content::BrowserContext* context);

  // Returns the upload service of `profile`, which must have been created by
  // `Create()`.
  static PendingBinaryUploadService* GetForProfile(Profile* profile);

  // BinaryUploadService:
  void MaybeUploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request) override;
  void MaybeAcknowledge(std::unique_ptr<BinaryUploadAck> ack) override;
  void MaybeCancelRequests(
      std::unique_ptr<BinaryUploadCancelRequests> cancel) override;
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;

  // Requests uploaded to this service, in the order they were uploaded. Tests
  // can complete them with `BinaryUploadRequest::FinishRequest()`, or take
  // ownership of them.
  std::vector<std::unique_ptr<BinaryUploadRequest>>& requests() {
    return requests_;
  }

 private:
  std::vector<std::unique_ptr<BinaryUploadRequest>> requests_;
  base::WeakPtrFactory<PendingBinaryUploadService> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_PENDING_BINARY_UPLOAD_SERVICE_H_
