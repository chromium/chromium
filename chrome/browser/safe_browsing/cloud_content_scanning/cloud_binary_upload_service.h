// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_

#include <list>
#include <memory>
#include <queue>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class CloudBinaryUploadService
    : public enterprise_connectors::BinaryUploadService {
 public:
  // The maximum number of uploads that can happen in parallel.
  static size_t GetParallelActiveRequestsMax();

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

  // If auth check results are available for the matching
  // `authorization_callbacks`, run and clear the callbacks.
  void MaybeRunAuthorizationCallbacks(
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

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

 protected:
  void FinishRequest(enterprise_connectors::BinaryUploadRequest* request,
                     enterprise_connectors::ScanRequestUploadResult result,
                     enterprise_connectors::ContentAnalysisResponse response);

  void FinishAndCleanupRequest(
      enterprise_connectors::BinaryUploadRequest* request,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::ContentAnalysisResponse response);

  // This may destroy `request`.
  // Virtual for testing.
  virtual void OnGetRequestData(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::BinaryUploadRequest::Data data);

  enterprise_connectors::BinaryUploadRequest* GetRequest(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

 private:
  using TokenAndConnector =
      std::pair<std::string, enterprise_connectors::AnalysisConnector>;
  friend class CloudBinaryUploadServiceTest;

  // Queue the file for deep scanning. This method should be the only caller of
  // UploadForDeepScanning to avoid consuming too many user resources.
  void QueueForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request);

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling `request`'s `callback`. This must be
  // called on the UI thread.
  virtual void UploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request);

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

  // Convenience callback method that calls both OnGetContentAnalysisResponse
  // and OnContentUploaded. Since the multipart uploader does not send separate
  // requests for metadata and content, it only needs one callback that finishes
  // the request and performs the cleanup.
  void OnUploadComplete(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      bool success,
      int http_status,
      const std::string& response_data);

  // Callback that runs when a content analysis verdict is received. Only used
  // explicitly by the resumable uploader.
  void OnGetContentAnalysisResponse(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      bool success,
      int http_status,
      const std::string& response_data);

  // Callback to cleanup the request. Only used explicitly by the resumable
  // uploader once the content is uploaded.
  void OnContentUploaded(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  void OnGetResponse(enterprise_connectors::BinaryUploadRequest::Id request_id,
                     enterprise_connectors::ContentAnalysisResponse response);

  void MaybeFinishRequest(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  void FinishRequestWithIncompleteResponse(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  void FinishIfActive(enterprise_connectors::BinaryUploadRequest::Id request_id,
                      enterprise_connectors::ScanRequestUploadResult result,
                      enterprise_connectors::ContentAnalysisResponse response);

  void MaybeUploadForDeepScanningCallback(
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest> request,
      enterprise_connectors::ScanRequestUploadResult auth_check_result);

  // Callback once the response from the backend is received.
  void ValidateDataUploadRequestConnectorCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::ContentAnalysisResponse response);

  void RecordRequestMetrics(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      enterprise_connectors::ScanRequestUploadResult result);
  void RecordRequestMetrics(
      enterprise_connectors::BinaryUploadRequest::Id request_id,
      enterprise_connectors::ScanRequestUploadResult result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Clears request and associated data from memory and starts the next queued
  // request, if present.
  void CleanupRequest(enterprise_connectors::BinaryUploadRequest* request);

  // Tries to start uploads from `request_queue_` depending on the number of
  // currently active requests. This should be called whenever
  // `active_requests_` shrinks so queued requests are started as soon as
  // possible.
  void PopRequestQueue();

  // Prepares auth and non-auth requests for uploading to the server.
  void PrepareRequestForUpload(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  bool ResponseIsComplete(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const raw_ptr<Profile> profile_;

  enterprise_connectors::BinaryUploadRequest::Id::Generator
      request_id_generator_;

  // enterprise_connectors::BinaryUploadRequest queued for upload.
  std::queue<std::unique_ptr<enterprise_connectors::BinaryUploadRequest>>
      request_queue_;

  // Resources associated with an in-progress request.
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id,
                 std::unique_ptr<enterprise_connectors::BinaryUploadRequest>>
      active_requests_;
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id,
                 base::TimeTicks>
      start_times_;
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id,
                 std::unique_ptr<base::OneShotTimer>>
      active_timers_;
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id,
                 std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>>
      active_uploads_;
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id, std::string>
      active_tokens_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<
      enterprise_connectors::BinaryUploadRequest::Id,
      base::flat_map<std::string,
                     enterprise_connectors::ContentAnalysisResponse::Result>>
      received_connector_results_;

  // Indicates whether this DM token + Connector combination can be used to
  // upload data for enterprise requests. Advanced Protection scans are
  // validated using the user's Advanced Protection enrollment status.
  base::flat_map<TokenAndConnector,
                 enterprise_connectors::ScanRequestUploadResult>
      can_upload_enterprise_data_;

  // Callbacks waiting on IsAuthorized request. These are organized by DM token
  // and Connector.
  base::flat_map<TokenAndConnector,
                 std::unique_ptr<base::OnceCallbackList<void(
                     enterprise_connectors::ScanRequestUploadResult)>>>
      authorization_callbacks_;

  // Indicates if this service is waiting on the backend to validate event
  // reporting. Used to avoid spamming the backend.
  base::flat_set<TokenAndConnector> pending_validate_data_upload_request_;

  // Ensures we validate the browser is registered with the backend every 24
  // hours.
  base::RepeatingTimer timer_;

  // Used to obtain an access token to attach to requests.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<CloudBinaryUploadService> weakptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_H_
