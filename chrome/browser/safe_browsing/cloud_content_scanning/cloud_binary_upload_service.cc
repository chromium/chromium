// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/management_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {
namespace {

// The command line flag to control the max amount of concurrent active
// requests.
// TODO(crbug.com/1191061): Tweak this number to an "optimal" value.
constexpr char kMaxParallelActiveRequests[] = "wp-max-parallel-active-requests";
constexpr int kDefaultMaxParallelActiveRequests = 5;

constexpr base::TimeDelta kAuthTimeout = base::Seconds(10);
constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbConsumerUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/consumer";

constexpr int kInitialBackoffSeconds = 3;
constexpr int kBackoffFactor = 2;
constexpr int kMaxRetryAttempt = 2;

bool* IgnoreFCMDelaysStorage() {
  static bool ignore = false;
  return &ignore;
}

bool IsConsumerScanRequest(const CloudBinaryUploadService::Request& request) {
  for (const std::string& tag : request.content_analysis_request().tags()) {
    if (tag == "dlp")
      return false;
  }
  return request.device_token().empty();
}

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(bool is_app) {
  if (is_app) {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_app", R"(
        semantics {
          sender: "Advanced Protection Program"
          description:
            "For users part of Google's Advanced Protection Program, when a "
            "file is downloaded, Chrome will upload that file to Safe Browsing "
            "for detailed scanning."
          trigger:
            "The browser will upload the file to Google when the user "
            "downloads a file, and the browser is enrolled into the "
            "Advanced Protection Program."
          data:
            "The downloaded file."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            AdvancedProtectionAllowed {
              AdvancedProtectionAllowed: false
            }
          }
        }
        )");
  } else {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_connector", R"(
        semantics {
          sender: "Chrome Enterprise Connectors"
          description:
            "For users with content analysis Chrome Enterprise Connectors "
            "enabled, Chrome will upload the data corresponding to the "
            "Connector for scanning."
          trigger:
            "If the OnFileAttachedEnterpriseConnector, "
            "OnFileDownloadedEnterpriseConnector or "
            "OnBulkDataEntryEnterpriseConnector policy is set, a request is made to "
            "scan a file attached to Chrome, a file downloaded by Chrome or "
            "data pasted in Chrome respectively."
          data:
            "The uploaded or downloaded file, or pasted data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting: "This is disabled by default an can only be enabled by "
            "policy."
          chrome_policy {
            OnFileAttachedEnterpriseConnector {
              OnFileAttachedEnterpriseConnector: "[]"
            }
            OnFileDownloadedEnterpriseConnector {
              OnFileDownloadedEnterpriseConnector: "[]"
            }
            OnBulkDataEntryEnterpriseConnector {
              OnBulkDataEntryEnterpriseConnector: "[]"
            }
          }
        }
        )");
  }
}

bool CanUseAccessToken(const BinaryUploadService::Request& request,
                       Profile* profile) {
  DCHECK(profile);
  // Consumer requests never need to use the access token.
  if (IsConsumerScanRequest(request))
    return false;

  // Allow the access token to be used on unmanaged devices, but not on
  // managed devices that aren't affiliated.
  if (!policy::IsDeviceCloudManaged())
    return true;

  return chrome::enterprise_util::IsProfileAffiliated(profile);
}

}  // namespace

// static
size_t CloudBinaryUploadService::GetParallelActiveRequestsMax() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kMaxParallelActiveRequests)) {
    int parsed_max;
    if (base::StringToInt(
            command_line->GetSwitchValueASCII(kMaxParallelActiveRequests),
            &parsed_max) &&
        parsed_max > 0) {
      return parsed_max;
    } else {
      LOG(ERROR) << "wp-max-parallel-active-requests had invalid value";
    }
  }

  return kDefaultMaxParallelActiveRequests;
}

CloudBinaryUploadService::CloudBinaryUploadService(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      binary_fcm_service_(BinaryFCMService::Create(profile)),
      profile_(profile),
      weakptr_factory_(this) {}

CloudBinaryUploadService::CloudBinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    std::unique_ptr<BinaryFCMService> binary_fcm_service)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(std::move(binary_fcm_service)),
      profile_(profile),
      weakptr_factory_(this) {}

CloudBinaryUploadService::~CloudBinaryUploadService() = default;

void CloudBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<CloudBinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsConsumerScanRequest(*request)) {
    DCHECK(!request->IsAuthRequest());
    const bool is_advanced_protection =
        safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
            profile_)
            ->IsUnderAdvancedProtection();
    const bool is_enhanced_protection =
        profile_ && IsEnhancedProtectionEnabled(*profile_->GetPrefs());

    const bool is_deep_scan_authorized =
        is_advanced_protection || is_enhanced_protection;
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       /*authorized=*/is_deep_scan_authorized);
    return;
  }

  // Make copies of the connector and DM token since |request| is about to move.
  auto connector = request->analysis_connector();
  std::string dm_token = request->device_token();
  TokenAndConnector token_and_connector = {dm_token, connector};

  if (dm_token.empty()) {
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       /*authorized*/ false);
    return;
  }

  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    // Get data from `request` before calling `IsAuthorized` since it is about
    // to move.
    GURL url = request->GetUrlWithParams();
    bool per_profile_request = request->per_profile_request();
    IsAuthorized(
        std::move(url), per_profile_request,
        base::BindOnce(
            &CloudBinaryUploadService::MaybeUploadForDeepScanningCallback,
            weakptr_factory_.GetWeakPtr(), std::move(request)),
        dm_token, connector);
    return;
  }

  MaybeUploadForDeepScanningCallback(
      std::move(request), can_upload_enterprise_data_[token_and_connector]);
}

void CloudBinaryUploadService::MaybeAcknowledge(std::unique_ptr<Ack> ack) {
  // Nothing to do for cloud upload service.
}

void CloudBinaryUploadService::MaybeCancelRequests(
    std::unique_ptr<CancelRequests> cancel) {
  // Nothing to do for cloud upload service.
  // TODO(1374944): Might consider canceling requests in `request_queue_`.
}

void CloudBinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<CloudBinaryUploadService::Request> request,
    bool authorized) {
  // Ignore the request if the browser cannot upload data.
  if (!authorized) {
    // TODO(crbug/1028133): Add extra logic to handle UX for non-authorized
    // users.
    request->FinishRequest(Result::UNAUTHORIZED,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }
  QueueForDeepScanning(std::move(request));
}

void CloudBinaryUploadService::QueueForDeepScanning(
    std::unique_ptr<CloudBinaryUploadService::Request> request) {
  if (active_requests_.size() >= GetParallelActiveRequestsMax())
    request_queue_.push(std::move(request));
  else
    UploadForDeepScanning(std::move(request));
}

void CloudBinaryUploadService::RemoveFCMRetryDelaysForTesting() {
  *IgnoreFCMDelaysStorage() = true;
}

void CloudBinaryUploadService::RetryFCMConnection(
    Request* request,
    int retry_count,
    base::TimeDelta next_backoff) {
  if (!IsActive(request)) {
    return;
  }

  if (!binary_fcm_service_ || !binary_fcm_service_->Connected()) {
    if (retry_count >= kMaxRetryAttempt) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&CloudBinaryUploadService::FinishRequest,
                         weakptr_factory_.GetWeakPtr(), request,
                         Result::FAILED_TO_GET_TOKEN,
                         enterprise_connectors::ContentAnalysisResponse()));
    } else {
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&CloudBinaryUploadService::RetryFCMConnection,
                         weakptr_factory_.GetWeakPtr(), request,
                         retry_count + 1, next_backoff * kBackoffFactor),
          next_backoff);
    }
    return;
  }

  OnFCMConnected(request);
}

void CloudBinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool is_auth_request = request->IsAuthRequest();
  Request* raw_request = request.get();
  raw_request->StartRequest();
  active_requests_[raw_request] = std::move(request);
  start_times_[raw_request] = base::TimeTicks::Now();

  std::string token = raw_request->SetRandomRequestToken();
  active_tokens_[raw_request] = token;

  if ((!binary_fcm_service_ || !binary_fcm_service_->Connected()) &&
      !is_auth_request &&
      raw_request->analysis_connector() !=
          enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY) {
    base::TimeDelta first_backoff;
    if (*IgnoreFCMDelaysStorage()) {
      first_backoff = base::Seconds(0);
    } else {
      first_backoff = base::Seconds(kInitialBackoffSeconds);
    }
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CloudBinaryUploadService::RetryFCMConnection,
                       weakptr_factory_.GetWeakPtr(), raw_request,
                       /*retry_count*/ 0, first_backoff * kBackoffFactor),
        first_backoff);
    return;
  }
  OnFCMConnected(raw_request);
}

void CloudBinaryUploadService::OnFCMConnected(Request* request) {
  if (!IsActive(request)) {
    return;
  }

  bool is_auth_request = request->IsAuthRequest();
  // Auth requests and paste requests are never going to need waiting for an
  // async response, so don't bother getting a token from `binary_fcm_service_`.
  if (is_auth_request) {
    request->GetRequestData(
        base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                       weakptr_factory_.GetWeakPtr(), request));
  } else if (request->analysis_connector() ==
             enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY) {
    MaybeGetAccessToken(request);
  } else {
    binary_fcm_service_->SetCallbackForToken(
        request->request_token(),
        base::BindRepeating(&CloudBinaryUploadService::OnGetResponse,
                            weakptr_factory_.GetWeakPtr(), request));
    binary_fcm_service_->GetInstanceID(
        base::BindOnce(&CloudBinaryUploadService::OnGetInstanceID,
                       weakptr_factory_.GetWeakPtr(), request));
  }

  // `request` might have been destroyed by:
  // - `OnGetRequestData` or
  // - `OnGetInstanceID`.
  if (!IsActive(request)) {
    return;
  }

  active_timers_[request] = std::make_unique<base::OneShotTimer>();
  active_timers_[request]->Start(
      FROM_HERE, is_auth_request ? kAuthTimeout : kScanningTimeout,
      base::BindOnce(&CloudBinaryUploadService::OnTimeout,
                     weakptr_factory_.GetWeakPtr(), request));
}

void CloudBinaryUploadService::OnGetInstanceID(Request* request,
                                               const std::string& instance_id) {
  if (!IsActive(request))
    return;

  if (instance_id == BinaryFCMService::kInvalidId) {
    FinishRequest(request, Result::FAILED_TO_GET_TOKEN,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  base::UmaHistogramCustomTimes(
      "SafeBrowsingBinaryUploadRequest.TimeToGetFCMToken",
      base::TimeTicks::Now() - start_times_[request], base::Milliseconds(1),
      base::Minutes(6), 50);

  request->set_fcm_token(instance_id);
  MaybeGetAccessToken(request);
}

void CloudBinaryUploadService::MaybeGetAccessToken(Request* request) {
  if (!IsActive(request)) {
    return;
  }

  if (CanUseAccessToken(*request, profile_)) {
    if (!token_fetcher_) {
      token_fetcher_ = std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile_));
    }
    token_fetcher_->Start(
        base::BindOnce(&CloudBinaryUploadService::OnGetAccessToken,
                       weakptr_factory_.GetWeakPtr(), request));
    return;
  }

  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request));
}

void CloudBinaryUploadService::OnGetAccessToken(
    Request* request,
    const std::string& access_token) {
  if (!IsActive(request))
    return;

  request->set_access_token(access_token);
  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request));
}

void CloudBinaryUploadService::OnGetRequestData(Request* request,
                                                Result result,
                                                Request::Data data) {
  if (!IsActive(request))
    return;

  if (result != Result::SUCCESS) {
    FinishRequest(request, result,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (!request->IsAuthRequest() && data.size == 0) {
    // A size of 0 implies an edge case like an empty file being uploaded. In
    // such a case, the file doesn't need to scan so the request can simply
    // finish early.
    FinishRequest(request, Result::SUCCESS,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  std::string metadata;
  request->SerializeToString(&metadata);
  base::Base64Encode(metadata, &metadata);

  GURL url = request->GetUrlWithParams();
  if (!url.is_valid())
    url = GetUploadUrl(IsConsumerScanRequest(*request));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetTrafficAnnotationTag(IsConsumerScanRequest(*request));
  auto callback = base::BindOnce(&CloudBinaryUploadService::OnUploadComplete,
                                 weakptr_factory_.GetWeakPtr(), request);
  std::unique_ptr<MultipartUploadRequest> upload_request;
  if (request->IsAuthRequest() || !data.contents.empty()) {
    upload_request = MultipartUploadRequest::CreateStringRequest(
        url_loader_factory_, std::move(url), metadata, data.contents,
        std::move(traffic_annotation), std::move(callback));
  } else if (!data.path.empty()) {
    upload_request = MultipartUploadRequest::CreateFileRequest(
        url_loader_factory_, std::move(url), metadata, data.path, data.size,
        std::move(traffic_annotation), std::move(callback));
  } else if (data.page.IsValid()) {
    upload_request = MultipartUploadRequest::CreatePageRequest(
        url_loader_factory_, std::move(url), metadata, std::move(data.page),
        std::move(traffic_annotation), std::move(callback));
  } else {
    NOTREACHED();
    FinishRequest(request, Result::UNKNOWN,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }
  upload_request->set_access_token(request->access_token());

  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), request->content_analysis_request());

  // |request| might have been deleted by the call to Start() in tests, so don't
  // dereference it afterwards.
  upload_request->Start();
  active_uploads_[request] = std::move(upload_request);
}

void CloudBinaryUploadService::OnUploadComplete(
    Request* request,
    bool success,
    int http_status,
    const std::string& response_data) {
  if (!IsActive(request))
    return;

  if (http_status == net::HTTP_TOO_MANY_REQUESTS) {
    FinishRequest(request, Result::TOO_MANY_REQUESTS,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (!success) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  enterprise_connectors::ContentAnalysisResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  active_uploads_.erase(request);

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request, response);
}

void CloudBinaryUploadService::OnGetResponse(
    Request* request,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (!IsActive(request))
    return;

  for (const auto& result : response.results()) {
    if (result.has_tag() && !result.tag().empty()) {
      VLOG(1) << "Request " << request->request_token()
              << " finished scanning tag <" << result.tag() << ">";
      received_connector_results_[request][result.tag()] = result;
    }
  }

  MaybeFinishRequest(request);
}

void CloudBinaryUploadService::MaybeFinishRequest(Request* request) {
  for (const std::string& tag : request->content_analysis_request().tags()) {
    const auto& results = received_connector_results_[request];
    if (base::ranges::none_of(results, [&tag](const auto& tag_and_result) {
          return tag_and_result.first == tag;
        })) {
      VLOG(1) << "Request " << request->request_token() << " is waiting for <"
              << tag << "> scanning to complete.";
      return;
    }
  }

  // It's OK to move here since the map entry is about to be removed.
  enterprise_connectors::ContentAnalysisResponse response;
  response.set_request_token(request->request_token());
  for (auto& tag_and_result : received_connector_results_[request])
    *response.add_results() = std::move(tag_and_result.second);
  FinishRequest(request, Result::SUCCESS, std::move(response));
}

void CloudBinaryUploadService::OnTimeout(Request* request) {
  if (IsActive(request))
    FinishRequest(request, Result::TIMEOUT,
                  enterprise_connectors::ContentAnalysisResponse());
}

void CloudBinaryUploadService::FinishRequest(
    Request* request,
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordRequestMetrics(request, result, response);

  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), request->content_analysis_request());
  WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request], ResultToString(result), response);

  std::string instance_id = request->fcm_notification_token();
  request->FinishRequest(result, response);
  FinishRequestCleanup(request, instance_id);

  // Now that a request has been cleaned up, we can try to allocate resources
  // for queued uploads.
  PopRequestQueue();
}

void CloudBinaryUploadService::FinishRequestCleanup(
    Request* request,
    const std::string& instance_id) {
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();
  active_requests_.erase(request);
  active_timers_.erase(request);
  active_uploads_.erase(request);
  received_connector_results_.erase(request);

  auto token_it = active_tokens_.find(request);
  if (binary_fcm_service_ && token_it != active_tokens_.end())
    binary_fcm_service_->ClearCallbackForToken(token_it->second);

  if (binary_fcm_service_ && instance_id != BinaryFCMService::kInvalidId) {
    // The BinaryFCMService will handle all recoverable errors. In case of
    // unrecoverable error, there's nothing we can do here.
    binary_fcm_service_->UnregisterInstanceID(
        instance_id,
        base::BindOnce(
            &CloudBinaryUploadService::InstanceIDUnregisteredCallback,
            weakptr_factory_.GetWeakPtr(), dm_token, connector));
  } else {
    // `binary_fcm_service_` can be null in tests and `instance_id` can be
    // invalid if getting an FCM token failed, timed out or for an auth request.
    // InstanceIDUnregisteredCallback should be called anyway so the requests
    // waiting on authentication can complete.
    InstanceIDUnregisteredCallback(dm_token, connector, true);
  }

  // Re-obtain `token_it` as auth requests calls to
  // InstanceIDUnregisteredCallback can result in new requests that invalidate
  // the iterator.
  token_it = active_tokens_.find(request);
  if (token_it != active_tokens_.end()) {
    active_tokens_.erase(token_it);
  }
}

void CloudBinaryUploadService::InstanceIDUnregisteredCallback(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector,
    bool) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  // Calling RunAuthorizationCallbacks after the instance ID of the initial
  // authentication is unregistered avoids registration/unregistration conflicts
  // with normal requests.
  if (authorization_callbacks_.contains(token_and_connector) &&
      !authorization_callbacks_[token_and_connector].empty() &&
      can_upload_enterprise_data_.contains(token_and_connector)) {
    RunAuthorizationCallbacks(dm_token, connector);
  }
}

void CloudBinaryUploadService::RecordRequestMetrics(Request* request,
                                                    Result result) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                base::TimeTicks::Now() - start_times_[request],
                                base::Milliseconds(1), base::Minutes(6), 50);
}

void CloudBinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  RecordRequestMetrics(request, result);
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "malware") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.MalwareResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
    if (response_result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsingBinaryUploadRequest.DlpResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

bool CloudBinaryUploadService::IsActive(Request* request) {
  return (active_requests_.find(request) != active_requests_.end());
}

class ValidateDataUploadRequest : public CloudBinaryUploadService::Request {
 public:
  ValidateDataUploadRequest(
      CloudBinaryUploadService::ContentAnalysisCallback callback,
      enterprise_connectors::CloudAnalysisSettings settings)
      : CloudBinaryUploadService::Request(
            std::move(callback),
            enterprise_connectors::CloudOrLocalAnalysisSettings(
                std::move(settings))) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // CloudBinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

  bool IsAuthRequest() const override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(CloudBinaryUploadService::Result::SUCCESS,
                          CloudBinaryUploadService::Request::Data());
}

bool ValidateDataUploadRequest::IsAuthRequest() const {
  return true;
}

void CloudBinaryUploadService::IsAuthorized(
    const GURL& url,
    bool per_profile_request,
    AuthorizationCallback callback,
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(
        FROM_HERE, base::Hours(24),
        base::BindRepeating(&CloudBinaryUploadService::ResetAuthorizationData,
                            weakptr_factory_.GetWeakPtr(), url));
  }

  TokenAndConnector token_and_connector = {dm_token, connector};
  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    // Send a request to check if the browser can upload data.
    authorization_callbacks_[token_and_connector].push_back(
        std::move(callback));
    if (!pending_validate_data_upload_request_.contains(token_and_connector)) {
      pending_validate_data_upload_request_.insert(token_and_connector);
      enterprise_connectors::CloudAnalysisSettings settings;
      settings.analysis_url = url;
      settings.dm_token = dm_token;
      auto request = std::make_unique<ValidateDataUploadRequest>(
          base::BindOnce(&CloudBinaryUploadService::
                             ValidateDataUploadRequestConnectorCallback,
                         weakptr_factory_.GetWeakPtr(), dm_token, connector),
          std::move(settings));
      request->set_device_token(dm_token);
      request->set_analysis_connector(connector);
      request->set_per_profile_request(per_profile_request);
      QueueForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_[token_and_connector]);
}

void CloudBinaryUploadService::ValidateDataUploadRequestConnectorCallback(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector,
    CloudBinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  pending_validate_data_upload_request_.erase(token_and_connector);
  can_upload_enterprise_data_[token_and_connector] =
      (result == CloudBinaryUploadService::Result::SUCCESS);
}

void CloudBinaryUploadService::RunAuthorizationCallbacks(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  DCHECK(can_upload_enterprise_data_.contains(token_and_connector));

  auto it = authorization_callbacks_[token_and_connector].begin();
  while (it != authorization_callbacks_[token_and_connector].end()) {
    std::move(*it).Run(can_upload_enterprise_data_[token_and_connector]);
    it = authorization_callbacks_[token_and_connector].erase(it);
  }
}

void CloudBinaryUploadService::ResetAuthorizationData(const GURL& url) {
  // Clearing |can_upload_enterprise_data_| will make the next
  // call to IsAuthorized send out a request to validate data uploads.
  auto it = can_upload_enterprise_data_.begin();
  while (it != can_upload_enterprise_data_.end()) {
    std::string dm_token = it->first.first;
    enterprise_connectors::AnalysisConnector connector = it->first.second;
    it = can_upload_enterprise_data_.erase(it);
    IsAuthorized(url, /*per_profile_request*/ false, base::DoNothing(),
                 dm_token, connector);
  }
}

void CloudBinaryUploadService::Shutdown() {
  if (binary_fcm_service_)
    binary_fcm_service_->Shutdown();
}

void CloudBinaryUploadService::SetAuthForTesting(const std::string& dm_token,
                                                 bool authorized) {
  for (enterprise_connectors::AnalysisConnector connector : {
         enterprise_connectors::AnalysisConnector::
             ANALYSIS_CONNECTOR_UNSPECIFIED,
             enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
             enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
             enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
             enterprise_connectors::AnalysisConnector::PRINT,
#if BUILDFLAG(IS_CHROMEOS_ASH)
             enterprise_connectors::AnalysisConnector::FILE_TRANSFER,
#endif
       }) {
    TokenAndConnector token_and_connector = {dm_token, connector};
    can_upload_enterprise_data_[token_and_connector] = authorized;
  }
}

// static
GURL CloudBinaryUploadService::GetUploadUrl(bool is_consumer_scan_eligible) {
  if (is_consumer_scan_eligible) {
    return GURL(kSbConsumerUploadUrl);
  } else {
    return GURL(kSbEnterpriseUploadUrl);
  }
}

void CloudBinaryUploadService::PopRequestQueue() {
  while (active_requests_.size() < GetParallelActiveRequestsMax() &&
         !request_queue_.empty()) {
    std::unique_ptr<Request> request = std::move(request_queue_.front());
    request_queue_.pop();
    UploadForDeepScanning(std::move(request));
  }
}

}  // namespace safe_browsing
