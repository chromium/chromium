// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_

#include <memory>
#include <optional>
#include <queue>

#include "base/callback_list.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class CloudBinaryUploadService
    : public enterprise_connectors::BinaryUploadService,
      public enterprise_connectors::CloudBinaryUploadServiceBase {
 public:

  explicit CloudBinaryUploadService(Profile* profile);

  // This constructor is useful in tests.
  CloudBinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile);
  ~CloudBinaryUploadService() override;

  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  void MaybeUploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request)
      override;
  void MaybeAcknowledge(
      std::unique_ptr<enterprise_connectors::BinaryUploadAck> ack) override;
  void MaybeCancelRequests(
      std::unique_ptr<enterprise_connectors::BinaryUploadCancelRequests> cancel)
      override;
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;

  // Sets `can_upload_data_` for tests.
  void SetAuthForTesting(
      const std::string& dm_token,
      enterprise_connectors::ScanRequestUploadResult auth_check_result);

  // Sets `token_fetcher_` for tests.
  void SetTokenFetcherForTesting(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher);

 private:
  friend class CloudBinaryUploadServiceTest;

  // CloudBinaryUploadServiceBase:
  void MaybeGetAccessToken(
      enterprise_connectors::BinaryUploadRequest::Id request_id) override;
  enterprise_connectors::BinaryUploadRequest::BrowserPolicyConnectorGetter
  BrowserPolicyConnectorGetter() override;
  bool IsAdvancedProtection() override;
  bool IsEnhancedProtection() override;
#if BUILDFLAG(IS_CHROMEOS)
  bool IsManagedGuestSession() override;
#endif

  void OnGetAccessToken(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      const std::string& access_token);

  const raw_ptr<Profile> profile_;

  // Used to obtain an access token to attach to requests.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<CloudBinaryUploadService> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
