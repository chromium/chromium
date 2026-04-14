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

  // Indicates whether the DM token/Connector combination is allowed to upload
  // data.
  using AuthorizationCallback =
      base::OnceCallback<void(enterprise_connectors::ScanRequestUploadResult)>;
  void IsAuthorized(const GURL& url,
                    bool per_profile_request,
                    AuthorizationCallback callback,
                    const std::string& dm_token,
                    enterprise_connectors::AnalysisConnector connector);

  // Resets `can_upload_data_`. Called every 24 hour by `timer_`.
  void ResetAuthorizationData(const GURL& url);

  // Sets `can_upload_data_` for tests.
  void SetAuthForTesting(
      const std::string& dm_token,
      enterprise_connectors::ScanRequestUploadResult auth_check_result);

  // Sets `token_fetcher_` for tests.
  void SetTokenFetcherForTesting(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher);

 protected:
  // This may destroy `request`.
  // Virtual for testing.
  virtual void OnGetRequestData(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::BinaryUploadRequest::Data data);

 private:
  using TokenAndConnector =
      std::pair<std::string, enterprise_connectors::AnalysisConnector>;
  friend class CloudBinaryUploadServiceTest;

  // Queue the file for deep scanning. This method should be the only caller of
  // UploadForDeepScanning to avoid consuming too many user resources.
  void QueueForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request);

  // CloudBinaryUploadServiceBase:
  void UploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request)
      override;

  // Get the access token only if the user matches the management and
  // affiliation requirements.
  void MaybeGetAccessToken(
      enterprise_connectors::BinaryUploadRequest::Id request_id);
  void OnGetAccessToken(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      const std::string& access_token);

  // Set the local IP addresses in the request. This is performed in a separate
  // callback to avoid blocking the UI thread and is only used for enterprise
  // requests.
  void OnIpAddressesFetched(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      std::vector<std::string> ip_addresses);

  // Convenience wrapper around the
  // FileAnalysisRequestBase::RegisterOnGotHashCallback to ensure the request is
  // posted as a task to the current thread.
  void RegisterOnGotHashCallback(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      enterprise_connectors::OnGotHashCallback on_got_hash_callback);

  void FinishRequestWithIncompleteResponse(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  void FinishIfActive(enterprise_connectors::BinaryUploadRequest::Id request_id,
                      enterprise_connectors::ScanRequestUploadResult result,
                      enterprise_connectors::ContentAnalysisResponse response);

  void MaybeUploadForDeepScanningCallback(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request,
      enterprise_connectors::ScanRequestUploadResult auth_check_result);

  enterprise_connectors::ScanRequestUploadResult GetConsumerAuthResult(
      const enterprise_connectors::BinaryUploadRequest& request);

  std::optional<enterprise_connectors::ScanRequestUploadResult>
  MaybeGetEnterpriseAuthResult(
      const enterprise_connectors::BinaryUploadRequest& request);

  // Callback once the response from the backend is received.
  void ValidateDataUploadRequestConnectorCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::ContentAnalysisResponse response);

  // Prepares auth and non-auth requests for uploading to the server.
  void PrepareRequestForUpload(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  bool ShouldTerminateRequestEarly(
      enterprise_connectors::BinaryUploadRequest* request,
      enterprise_connectors::ScanRequestUploadResult get_data_result,
      size_t data_size);

  const raw_ptr<Profile> profile_;

  enterprise_connectors::BinaryUploadRequest::Id::Generator
      request_id_generator_;

  // Indicates if this service is waiting on the backend to validate event
  // reporting. Used to avoid spamming the backend.
  base::flat_set<TokenAndConnector> pending_validate_data_upload_request_;

  // Ensures we validate the browser is registered with the backend every 24
  // hours.
  base::RepeatingTimer timer_;

  // Used to obtain an access token to attach to requests.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<CloudBinaryUploadService> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
