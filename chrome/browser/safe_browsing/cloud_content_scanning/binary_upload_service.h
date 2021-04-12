// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_

#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class Profile;

namespace safe_browsing {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class BinaryUploadService : public KeyedService {
 public:
  // The maximum size of data that can be uploaded via this service.
  constexpr static size_t kMaxUploadSizeBytes = 50 * 1024 * 1024;  // 50 MB

  explicit BinaryUploadService(Profile* profile);

  BinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile);

  // This constructor is useful in tests, if you want to keep a reference to the
  // service's |binary_fcm_service_|.
  BinaryUploadService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      std::unique_ptr<BinaryFCMService> binary_fcm_service);
  ~BinaryUploadService() override;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    // Unknown result.
    UNKNOWN = 0,

    // The request succeeded.
    SUCCESS = 1,

    // The upload failed, for an unspecified reason.
    UPLOAD_FAILURE = 2,

    // The upload succeeded, but a response was not received before timing out.
    TIMEOUT = 3,

    // The file was too large to upload.
    FILE_TOO_LARGE = 4,

    // The BinaryUploadService failed to get an InstanceID token.
    FAILED_TO_GET_TOKEN = 5,

    // The user is unauthorized to make the request.
    UNAUTHORIZED = 6,

    // Some or all parts of the file are encrypted.
    FILE_ENCRYPTED = 7,

    // The file's type is not supported and the file was not uploaded.
    DLP_SCAN_UNSUPPORTED_FILE_TYPE = 8,

    // The server returned a 429 HTTP status indicating too many requests are
    // being sent.
    TOO_MANY_REQUESTS = 9,

    kMaxValue = TOO_MANY_REQUESTS,
  };

  // Callbacks used to pass along the results of scanning. The response protos
  // will only be populated if the result is SUCCESS.
  using ContentAnalysisCallback =
      base::OnceCallback<void(Result,
                              enterprise_connectors::ContentAnalysisResponse)>;

  // A class to encapsulate the a request for upload. This class will provide
  // all the functionality needed to generate a ContentAnalysisRequest, and
  // subclasses will provide different sources of data to upload (e.g. file or
  // string).
  class Request {
   public:
    // |callback| will run on the UI thread.
    Request(ContentAnalysisCallback, GURL url);
    virtual ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    Request(Request&&) = delete;
    Request& operator=(Request&&) = delete;

    // Structure of data returned in the callback to GetRequestData().
    struct Data {
      Data();

      // The data content.
      std::string contents;

      // The SHA256 of the data.
      std::string hash;

      // The size of the data. This can differ from |contents.size()| when the
      // file is too large for deep scanning. This field will contain the true
      // size.
      uint64_t size = 0;
    };

    // Asynchronously returns the file contents to upload.
    // TODO(drubery): This could allocate up to kMaxUploadSizeBytes of memory
    // for a large file upload. We should see how often that causes errors,
    // and possibly implement some sort of streaming interface so we don't use
    // so much memory.
    //
    // |result| is set to SUCCESS if getting the request data succeeded or
    // some value describing the error.
    using DataCallback = base::OnceCallback<void(Result, const Data&)>;
    virtual void GetRequestData(DataCallback callback) = 0;

    // Returns the URL to send the request to.
    GURL GetUrlWithParams() const;

    // Returns the metadata to upload, as a ContentAnalysisRequest.
    const enterprise_connectors::ContentAnalysisRequest&
    content_analysis_request() const {
      return content_analysis_request_;
    }

    void set_tab_url(const GURL& tab_url);
    const GURL& tab_url() const;

    void set_per_profile_request(bool per_profile_request);
    bool per_profile_request() const;

    // Methods for modifying the ContentAnalysisRequest.
    void set_analysis_connector(
        enterprise_connectors::AnalysisConnector connector);
    void set_url(const std::string& url);
    void set_csd(ClientDownloadRequest csd);
    void add_tag(const std::string& tag);
    void set_email(const std::string& email);
    void set_request_token(const std::string& token);
    void set_fcm_token(const std::string& token);
    void set_device_token(const std::string& token);
    void set_filename(const std::string& filename);
    void set_digest(const std::string& digest);
    void clear_dlp_scan_request();
    void set_client_metadata(enterprise_connectors::ClientMetadata metadata);

    // Methods for accessing the ContentAnalysisRequest.
    enterprise_connectors::AnalysisConnector analysis_connector();
    const std::string& device_token() const;
    const std::string& request_token() const;
    const std::string& fcm_notification_token() const;
    const std::string& filename() const;
    const std::string& digest() const;

    // Finish the request, with the given |result| and |response| from the
    // server.
    void FinishRequest(Result result,
                       enterprise_connectors::ContentAnalysisResponse response);

    // Calls SerializeToString on the appropriate proto request.
    void SerializeToString(std::string* destination) const;

   private:
    enterprise_connectors::ContentAnalysisRequest content_analysis_request_;
    ContentAnalysisCallback content_analysis_callback_;

    // The URL to send the data to for scanning.
    GURL url_;

    // The URL of the page that initially triggered the scan.
    GURL tab_url_;

    // Indicates if the request was triggered by a profile-level policy or not.
    bool per_profile_request_ = false;
  };

  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  virtual void MaybeUploadForDeepScanning(std::unique_ptr<Request> request);

  // Indicates whether the DM token/Connector combination is allowed to upload
  // data.
  using AuthorizationCallback = base::OnceCallback<void(bool)>;
  void IsAuthorized(const GURL& url,
                    AuthorizationCallback callback,
                    const std::string& dm_token,
                    enterprise_connectors::AnalysisConnector connector);

  // Run every matching callback in |authorization_callbacks_| and remove them.
  void RunAuthorizationCallbacks(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector);

  // Resets |can_upload_data_|. Called every 24 hour by |timer_|.
  void ResetAuthorizationData(const GURL& url);

  // Performs cleanup needed at shutdown.
  void Shutdown() override;

  // Sets |can_upload_data_| for tests.
  void SetAuthForTesting(const std::string& dm_token, bool authorized);

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

 protected:
  void FinishRequest(Request* request,
                     Result result,
                     enterprise_connectors::ContentAnalysisResponse response);

 private:
  using TokenAndConnector =
      std::pair<std::string, enterprise_connectors::AnalysisConnector>;
  friend class BinaryUploadServiceTest;

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling |request|'s |callback|. This must be
  // called on the UI thread.
  virtual void UploadForDeepScanning(std::unique_ptr<Request> request);

  void OnGetInstanceID(Request* request, const std::string& token);

  void OnGetRequestData(Request* request,
                        Result result,
                        const Request::Data& data);

  void OnUploadComplete(Request* request,
                        bool success,
                        int http_status,
                        const std::string& response_data);

  void OnGetResponse(Request* request,
                     enterprise_connectors::ContentAnalysisResponse response);

  void MaybeFinishRequest(Request* request);

  void OnTimeout(Request* request);

  bool IsActive(Request* request);

  void MaybeUploadForDeepScanningCallback(std::unique_ptr<Request> request,
                                          bool authorized);

  // Callback once the response from the backend is received.
  void ValidateDataUploadRequestConnectorCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  // Callback once a request's instance ID is unregistered.
  void InstanceIDUnregisteredCallback(
      const std::string& dm_token,
      enterprise_connectors::AnalysisConnector connector,
      bool);

  void RecordRequestMetrics(Request* request, Result result);
  void RecordRequestMetrics(
      Request* request,
      Result result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Called at the end of the FinishRequest method.
  void FinishRequestCleanup(Request* request, const std::string& instance_id);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<BinaryFCMService> binary_fcm_service_;

  Profile* const profile_;

  // Resources associated with an in-progress request.
  base::flat_map<Request*, std::unique_ptr<Request>> active_requests_;
  base::flat_map<Request*, base::TimeTicks> start_times_;
  base::flat_map<Request*, std::unique_ptr<base::OneShotTimer>> active_timers_;
  base::flat_map<Request*, std::unique_ptr<MultipartUploadRequest>>
      active_uploads_;
  base::flat_map<Request*, std::string> active_tokens_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<
      Request*,
      base::flat_map<std::string,
                     enterprise_connectors::ContentAnalysisResponse::Result>>
      received_connector_results_;

  // Indicates whether this DM token + Connector combination can be used to
  // upload data for enterprise requests. Advanced Protection scans are
  // validated using the user's Advanced Protection enrollment status.
  base::flat_map<TokenAndConnector, bool> can_upload_enterprise_data_;

  // Callbacks waiting on IsAuthorized request. These are organized by DM token
  // and Connector.
  base::flat_map<TokenAndConnector, std::list<base::OnceCallback<void(bool)>>>
      authorization_callbacks_;

  // Indicates if this service is waiting on the backend to validate event
  // reporting. Used to avoid spamming the backend.
  base::flat_set<TokenAndConnector> pending_validate_data_upload_request_;

  // Ensures we validate the browser is registered with the backend every 24
  // hours.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<BinaryUploadService> weakptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_BINARY_UPLOAD_SERVICE_H_
