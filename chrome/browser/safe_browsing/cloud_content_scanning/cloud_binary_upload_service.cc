// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service.h"

#include <algorithm>
#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace safe_browsing {
namespace {

// The default maximum number of concurrent active requests. This is used to
// limit the number of requests that are actively being uploaded. This is set to
// default of 15 because it was determined to be a good value through
// experiments. See http://crbug.com/329293309.
constexpr int kDefaultMaxParallelActiveRequests = 15;

constexpr base::TimeDelta kAuthTimeout = base::Seconds(10);
constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

const char kSbEnterpriseUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/scan";

const char kSbConsumerUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/consumer";

net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag(bool is_app) {
  if (is_app) {
    return net::DefineNetworkTrafficAnnotation(
        "safe_browsing_binary_upload_app", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "For users opted in to Enhanced Safe Browsing or Google's Advanced "
            "Protection Program, when a file is downloaded, Chrome may upload "
            "that file to Safe Browsing for detailed scanning."
          trigger:
            "The browser will upload the file to Google when the user "
            "downloads a suspicious file and the user is opted in to Enhanced "
            "Safe Browsing or Google's Advanced Protection Program."
          data:
            "The downloaded file and metadata about how the user came to "
            "download that file (including URLs)."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/safe_browsing/cloud_content_scanning/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: FILE_DATA
          }
          last_reviewed: "2023-07-28"
        }
        policy {
          cookies_allowed: NO
          setting: "This is disabled by default an can only be enabled by "
            "opting in to Enhanced Safe Browsing or the Advanced Protection "
            "Program."
          chrome_policy {
            SafeBrowsingDeepScanningEnabled: {
              SafeBrowsingDeepScanningEnabled: false
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
            "OnFileDownloadedEnterpriseConnector, "
            "OnFileTransferEnterpriseConnector, "
            "OnBulkDataEntryEnterpriseConnector or OnPrintEnterpriseConnector "
            "policy is set, a request is made to scan a file attached to "
            "Chrome, a file downloaded by Chrome, a file transfered from a "
            "ChromeOS file system, data pasted in "
            "Chrome or data printed from Chrome respectively."
          data:
            "The uploaded/downloaded/transfered file, pasted data or printed "
            "data. Also includes an access token (enterprise only)."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              owners: "//chrome/browser/safe_browsing/cloud_content_scanning/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: FILE_DATA
            type: USER_CONTENT
            type: WEB_CONTENT
          }
          last_reviewed: "2023-07-28"
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
            OnFileTransferEnterpriseConnector {
              OnFileTransferEnterpriseConnector: "[]"
            }
            OnPrintEnterpriseConnector {
              OnPrintEnterpriseConnector: "[]"
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
  if (IsConsumerScanRequest(request)) {
    return false;
  }

  // Allow the access token to be used on unmanaged devices, but not on
  // managed devices that aren't affiliated.
  if (!policy::ManagementServiceFactory::GetForProfile(profile)
           ->HasManagementAuthority(
               policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    return true;
  }

  // The access token can always be included in affiliated use cases.
  if (enterprise_util::IsProfileAffiliated(profile)) {
    return true;
  }

  // This code being reached implies that the browser and profile are
  // not affiliated.
  return request.per_profile_request();
}

bool IgnoreErrorResultForResumableUpload(
    BinaryUploadService::Request* request,
    enterprise_connectors::ScanRequestUploadResult result) {
  return enterprise_connectors::IsResumableUpload(*request) &&
         (result ==
              enterprise_connectors::ScanRequestUploadResult ::FILE_TOO_LARGE ||
          result ==
              enterprise_connectors::ScanRequestUploadResult ::FILE_ENCRYPTED);
}

}  // namespace

// static
size_t CloudBinaryUploadService::GetParallelActiveRequestsMax() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kWpMaxParallelActiveRequests)) {
    int parsed_max;
    if (base::StringToInt(command_line->GetSwitchValueASCII(
                              switches::kWpMaxParallelActiveRequests),
                          &parsed_max) &&
        parsed_max > 0) {
      return parsed_max;
    } else {
      DVLOG(1) << "wp-max-parallel-active-requests had invalid value";
    }
  }

  return kDefaultMaxParallelActiveRequests;
}

CloudBinaryUploadService::CloudBinaryUploadService(Profile* profile)
    : url_loader_factory_(profile->GetURLLoaderFactory()),
      profile_(profile),
      weakptr_factory_(this) {}

CloudBinaryUploadService::CloudBinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory),
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

    const enterprise_connectors::ScanRequestUploadResult
        is_deep_scan_authorized =
            is_advanced_protection || is_enhanced_protection
                ? enterprise_connectors::ScanRequestUploadResult ::SUCCESS
                : enterprise_connectors::ScanRequestUploadResult ::UNAUTHORIZED;
    MaybeUploadForDeepScanningCallback(
        std::move(request),
        /*auth_check_result=*/is_deep_scan_authorized);
    return;
  }

  // Make copies of the connector and DM token since |request| is about to move.
  auto connector = request->analysis_connector();
  std::string dm_token = request->device_token();
  TokenAndConnector token_and_connector = {dm_token, connector};

  if (dm_token.empty()) {
    MaybeUploadForDeepScanningCallback(
        std::move(request),
        /*authorized*/ enterprise_connectors::ScanRequestUploadResult::
            UNAUTHORIZED);
    return;
  }

  // Validate if `token_and_connector` is authorized to upload data if this is
  // the first time or the previous check failed.
  if (!can_upload_enterprise_data_.contains(token_and_connector) ||
      can_upload_enterprise_data_[token_and_connector] !=
          enterprise_connectors::ScanRequestUploadResult::SUCCESS) {
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
  // TODO(crbug.com/40242713): Might consider canceling requests in
  // `request_queue_`.
}

base::WeakPtr<BinaryUploadService> CloudBinaryUploadService::AsWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CloudBinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<CloudBinaryUploadService::Request> request,
    enterprise_connectors::ScanRequestUploadResult auth_check_result) {
  // Ignore the request if the browser cannot upload data.
  if (auth_check_result !=
      enterprise_connectors::ScanRequestUploadResult::SUCCESS) {
    // TODO(crbug.com/40660637): Add extra logic to handle UX for non-authorized
    // users.
    request->FinishRequest(auth_check_result,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }
  QueueForDeepScanning(std::move(request));
}

void CloudBinaryUploadService::QueueForDeepScanning(
    std::unique_ptr<CloudBinaryUploadService::Request> request) {
  if (active_requests_.size() >= GetParallelActiveRequestsMax()) {
    request_queue_.push(std::move(request));
  } else {
    UploadForDeepScanning(std::move(request));
  }
}

void CloudBinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Request* raw_request = request.get();
  Request::Id id = request_id_generator_.GenerateNextId();
  request->set_id(id);
  request->StartRequest();
  active_requests_[id] = std::move(request);
  start_times_[id] = base::TimeTicks::Now();

  std::string token = raw_request->SetRandomRequestToken();
  active_tokens_[id] = token;

  PrepareRequestForUpload(id);
}

void CloudBinaryUploadService::PrepareRequestForUpload(Request::Id request_id) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (request->IsAuthRequest()) {
    request->GetRequestData(
        base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                       weakptr_factory_.GetWeakPtr(), request_id));
  } else if (!IsConsumerScanRequest(*request)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&::enterprise_connectors::GetLocalIpAddresses),
        base::BindOnce(&CloudBinaryUploadService::OnIpAddressesFetched,
                       weakptr_factory_.GetWeakPtr(), request_id));
  } else {
    MaybeGetAccessToken(request_id);
  }

  // `request` might have been destroyed by `OnGetRequestData`
  request = GetRequest(request_id);
  if (!request) {
    return;
  }

  active_timers_[request_id] = std::make_unique<base::OneShotTimer>();
  active_timers_[request_id]->Start(
      FROM_HERE, request->IsAuthRequest() ? kAuthTimeout : kScanningTimeout,
      base::BindOnce(&CloudBinaryUploadService::FinishIfActive,
                     weakptr_factory_.GetWeakPtr(), request_id,
                     enterprise_connectors::ScanRequestUploadResult::TIMEOUT,
                     enterprise_connectors::ContentAnalysisResponse()));
}

void CloudBinaryUploadService::MaybeGetAccessToken(Request::Id request_id) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (CanUseAccessToken(*request, profile_)) {
    if (!token_fetcher_) {
      token_fetcher_ = std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
          IdentityManagerFactory::GetForProfile(profile_));
    }
    token_fetcher_->Start(
        base::BindOnce(&CloudBinaryUploadService::OnGetAccessToken,
                       weakptr_factory_.GetWeakPtr(), request_id));
    return;
  }

  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request_id));
}

void CloudBinaryUploadService::OnGetAccessToken(
    Request::Id request_id,
    const std::string& access_token) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  request->set_access_token(access_token);
  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request_id));
}

void CloudBinaryUploadService::OnIpAddressesFetched(
    Request::Id request_id,
    std::vector<std::string> ip_addresses) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  for (const auto& ip_address : ip_addresses) {
    request->add_local_ips(ip_address);
  }

  MaybeGetAccessToken(request_id);
}

void CloudBinaryUploadService::OnGetRequestData(
    Request::Id request_id,
    enterprise_connectors::ScanRequestUploadResult result,
    Request::Data data) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (result != enterprise_connectors::ScanRequestUploadResult::SUCCESS) {
    if (!IgnoreErrorResultForResumableUpload(request, result)) {
      FinishAndCleanupRequest(request, result,
                              enterprise_connectors::ContentAnalysisResponse());
      return;
    }

    // If the error is not unrecoverable, chrome can attempt to sent the
    // file contents to the content analysis service.  Let the service know that
    // a metadata-only analysis is required.
    request->set_require_metadata_verdict(true);
    // If the file is encrypted, let the service know that the file is
    // encrypted.
    if (result ==
        enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED) {
      request->set_is_content_encrypted(true);
    }
    if (result ==
        enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE) {
      request->set_is_content_too_large(true);
    }
  }

  if (!request->IsAuthRequest() && data.size == 0) {
    // A size of 0 implies an edge case like an empty file being uploaded. In
    // such a case, the file doesn't need to scan so the request can simply
    // finish early.
    FinishAndCleanupRequest(
        request, enterprise_connectors::ScanRequestUploadResult::SUCCESS,
        enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  std::string metadata;
  request->SerializeToString(&metadata);
  metadata = base::Base64Encode(metadata);

  GURL url = request->GetUrlWithParams();
  if (!url.is_valid()) {
    url = GetUploadUrl(IsConsumerScanRequest(*request));
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      GetTrafficAnnotationTag(IsConsumerScanRequest(*request));
  std::string histogram_suffix =
      IsConsumerScanRequest(*request) ? "ConsumerUpload" : "EnterpriseUpload";
  auto callback = base::BindOnce(&CloudBinaryUploadService::OnUploadComplete,
                                 weakptr_factory_.GetWeakPtr(), request_id);
  auto verdict_received_callback =
      base::BindOnce(&CloudBinaryUploadService::OnGetContentAnalysisResponse,
                     weakptr_factory_.GetWeakPtr(), request_id);
  auto content_uploaded_callback =
      base::BindOnce(&CloudBinaryUploadService::OnContentUploaded,
                     weakptr_factory_.GetWeakPtr(), request_id);
  std::unique_ptr<enterprise_connectors::ConnectorUploadRequest> upload_request;
  // The downloaded file will not be available for deep scan upload due to the
  // newly introduced download obfuscation step. We must wait for deobfuscation
  // to complete before uploading, which is guaranteed under the pre-async
  // upload behaviour.
  bool force_sync_upload =
      request->analysis_connector() == enterprise_connectors::FILE_DOWNLOADED;
  if (request->IsAuthRequest()) {
    upload_request = MultipartUploadRequest::CreateStringRequest(
        url_loader_factory_, url, metadata, data.contents, histogram_suffix,
        std::move(traffic_annotation), std::move(callback));
  } else if (!data.contents.empty()) {
    upload_request =
        (enterprise_connectors::IsResumableUpload(*request) &&
         base::FeatureList::IsEnabled(
             enterprise_connectors::kDlpScanPastedImages))
            ? ResumableUploadRequest::CreateStringRequest(
                  url_loader_factory_, url, metadata, data.contents,
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload)
            : MultipartUploadRequest::CreateStringRequest(
                  url_loader_factory_, url, metadata, data.contents,
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(callback));
  } else if (!data.path.empty()) {
    upload_request =
        enterprise_connectors::IsResumableUpload(*request)
            ? ResumableUploadRequest::CreateFileRequest(
                  url_loader_factory_, url, metadata, result, data.path,
                  data.size, data.is_obfuscated, histogram_suffix,
                  std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload)
            : MultipartUploadRequest::CreateFileRequest(
                  url_loader_factory_, url, metadata, data.path, data.size,
                  data.is_obfuscated, histogram_suffix,
                  std::move(traffic_annotation), std::move(callback));

  } else if (data.page.IsValid()) {
    upload_request =
        enterprise_connectors::IsResumableUpload(*request)
            ? ResumableUploadRequest::CreatePageRequest(
                  url_loader_factory_, url, metadata, result,
                  std::move(data.page), histogram_suffix,
                  std::move(traffic_annotation),
                  std::move(verdict_received_callback),
                  std::move(content_uploaded_callback), force_sync_upload)
            : MultipartUploadRequest::CreatePageRequest(
                  url_loader_factory_, url, metadata, std::move(data.page),
                  histogram_suffix, std::move(traffic_annotation),
                  std::move(callback));
  } else {
    NOTREACHED();
  }
  upload_request->set_access_token(request->access_token());

  WebUIContentInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), request->access_token(),
      upload_request->GetUploadInfo(), url.spec(),
      request->content_analysis_request());

  // |request| might have been deleted by the call to Start() in tests, so don't
  // dereference it afterwards.
  upload_request->Start();
  active_uploads_[request_id] = std::move(upload_request);
}

void CloudBinaryUploadService::OnUploadComplete(
    Request::Id request_id,
    bool success,
    int http_status,
    const std::string& response_data) {
  OnGetContentAnalysisResponse(request_id, success, http_status, response_data);
  OnContentUploaded(request_id);
}

void CloudBinaryUploadService::OnContentUploaded(Request::Id request_id) {
  if (Request* request = GetRequest(request_id); request) {
    CleanupRequest(request);
  }
}

void CloudBinaryUploadService::OnGetContentAnalysisResponse(
    Request::Id request_id,
    bool success,
    int http_status,
    const std::string& response_data) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  if (http_status == net::HTTP_UNAUTHORIZED) {
    FinishRequest(request,
                  enterprise_connectors::ScanRequestUploadResult::UNAUTHORIZED,
                  enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (http_status == net::HTTP_TOO_MANY_REQUESTS) {
    FinishRequest(
        request,
        enterprise_connectors::ScanRequestUploadResult::TOO_MANY_REQUESTS,
        enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  if (!success) {
    FinishRequest(
        request, enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE,
        enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  enterprise_connectors::ContentAnalysisResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(
        request, enterprise_connectors::ScanRequestUploadResult::UPLOAD_FAILURE,
        enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request_id, response);
}

void CloudBinaryUploadService::OnGetResponse(
    Request::Id request_id,
    enterprise_connectors::ContentAnalysisResponse response) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  for (const auto& result : response.results()) {
    if (result.has_tag() && !result.tag().empty()) {
      DVLOG(1) << "Request " << request->request_token()
               << " finished scanning tag <" << result.tag() << ">";
      received_connector_results_[request_id][result.tag()] = result;
    }
  }

  MaybeFinishRequest(request_id);
}

void CloudBinaryUploadService::MaybeFinishRequest(Request::Id request_id) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  // It's OK to move here since the map entry is about to be removed.
  enterprise_connectors::ContentAnalysisResponse response;
  response.set_request_token(request->request_token());
  for (auto& tag_and_result : received_connector_results_[request_id]) {
    *response.add_results() = std::move(tag_and_result.second);
  }

  // Set `result` to be INCOMPLETE_RESPONSE, if the request is terminated with incomplete
  // response.
  enterprise_connectors::ScanRequestUploadResult result =
      enterprise_connectors::ScanRequestUploadResult::SUCCESS;
  if (!ResponseIsComplete(request_id)) {
    result =
        enterprise_connectors::ScanRequestUploadResult::INCOMPLETE_RESPONSE;
  } else if (request->is_content_too_large()) {
    result = enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE;
  } else if (request->is_content_encrypted()) {
    result = enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED;
  }

  FinishRequest(request, result, std::move(response));
}

void CloudBinaryUploadService::FinishIfActive(
    Request::Id request_id,
    enterprise_connectors::ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  Request* request = GetRequest(request_id);
  if (request) {
    FinishAndCleanupRequest(request, result, response);
  }
}

void CloudBinaryUploadService::FinishAndCleanupRequest(
    Request* request,
    enterprise_connectors::ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  FinishRequest(request, result, response);
  CleanupRequest(request);
}

void CloudBinaryUploadService::FinishRequest(
    Request* request,
    enterprise_connectors::ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordRequestMetrics(request->id(), result, response);
  std::string upload_info = "None";
  if (active_uploads_.count(request->id()) && !request->IsAuthRequest()) {
    upload_info = active_uploads_[request->id()]->GetUploadInfo();
  }

  // We add the request here in case we never actually uploaded anything, so
  // it wasn't added in OnGetRequestData
  WebUIContentInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), request->access_token(), upload_info,
      request->GetUrlWithParams().spec(), request->content_analysis_request());
  WebUIContentInfoSingleton::GetInstance()->AddToDeepScanResponses(
      active_tokens_[request->id()],
      enterprise_connectors::ScanRequestUploadResultToString(result), response);

  request->FinishRequest(result, response);
}

void CloudBinaryUploadService::CleanupRequest(Request* request) {
  Request::Id request_id = request->id();
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();
  active_requests_.erase(request_id);
  active_timers_.erase(request_id);
  active_uploads_.erase(request_id);
  received_connector_results_.erase(request_id);
  active_tokens_.erase(request_id);

  MaybeRunAuthorizationCallbacks(dm_token, connector);

  // Now that a request has been cleaned up, we can try to allocate resources
  // for queued uploads.
  PopRequestQueue();
}

void CloudBinaryUploadService::RecordRequestMetrics(
    Request::Id request_id,
    enterprise_connectors::ScanRequestUploadResult result) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);

  auto duration = base::TimeTicks::Now() - start_times_[request_id];
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                duration, base::Milliseconds(1),
                                base::Minutes(6), 50);

  Request* request = GetRequest(request_id);
  if (request && !IsConsumerScanRequest(*request)) {
    std::string request_type;
    switch (request->analysis_connector()) {
      case enterprise_connectors::FILE_DOWNLOADED:
      case enterprise_connectors::FILE_ATTACHED:
      case enterprise_connectors::FILE_TRANSFER:
        request_type = "File";
        break;
      case enterprise_connectors::BULK_DATA_ENTRY:
        request_type = "Text";
        break;
      case enterprise_connectors::PRINT:
        request_type = "Print";
        break;
      case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
        break;
    }
    if (request_type.empty()) {
      return;
    }

    std::string protocol = enterprise_connectors::IsResumableUpload(*request)
                               ? "Resumable"
                               : "Multipart";

    // Example values:
    //   "Enterprise.ResumableRequest.Print.Duration
    //   "Enterprise.MultipartRequest.Text.Duration
    //   "Enterprise.ResumableRequest.File.Result
    base::UmaHistogramCustomTimes(
        base::StrCat(
            {"Enterprise.", protocol, "Request.", request_type, ".Duration"}),
        duration, base::Milliseconds(1), base::Minutes(6), 50);
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Enterprise.", protocol, "Request.", request_type, ".Result"}),
        result);
  }
}

void CloudBinaryUploadService::RecordRequestMetrics(
    Request::Id request_id,
    enterprise_connectors::ScanRequestUploadResult result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  RecordRequestMetrics(request_id, result);
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

bool CloudBinaryUploadService::ResponseIsComplete(Request::Id request_id) {
  Request* request = GetRequest(request_id);
  if (!request) {
    return false;
  }

  for (const std::string& tag : request->content_analysis_request().tags()) {
    if (received_connector_results_[request_id].count(tag) == 0) {
      return false;
    }
  }

  return true;
}

BinaryUploadService::Request* CloudBinaryUploadService::GetRequest(
    Request::Id request_id) {
  auto it = active_requests_.find(request_id);
  if (it != active_requests_.end()) {
    return it->second.get();
  }

  return nullptr;
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
  std::move(callback).Run(
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
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
  // Validate if `token_and_connector` is authorized to upload data if this is
  // the first time or the previous check failed.
  if (!can_upload_enterprise_data_.contains(token_and_connector) ||
      can_upload_enterprise_data_[token_and_connector] !=
          enterprise_connectors::ScanRequestUploadResult::SUCCESS) {
    // Send a request to check if the browser can upload data.
    auto [iter, inserted] = authorization_callbacks_.try_emplace(
        token_and_connector,
        std::make_unique<base::OnceCallbackList<void(
            enterprise_connectors::ScanRequestUploadResult)>>());
    iter->second->AddUnsafe(std::move(callback));

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

#if BUILDFLAG(IS_CHROMEOS)
      // WebProtect handles requests from ChromeOS Managed Guest Sessions
      // differently, as it cannot rely on the GAIA ID to determine whether or
      // not the user has the BCE license.
      enterprise_connectors::ClientMetadata client_metadata;
      client_metadata.set_is_chrome_os_managed_guest_session(
          chromeos::IsManagedGuestSession());
      request->set_client_metadata(std::move(client_metadata));
#endif

      QueueForDeepScanning(std::move(request));
    }
    return;
  }
  std::move(callback).Run(can_upload_enterprise_data_[token_and_connector]);
}

void CloudBinaryUploadService::ValidateDataUploadRequestConnectorCallback(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector,
    enterprise_connectors::ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  pending_validate_data_upload_request_.erase(token_and_connector);
  can_upload_enterprise_data_[token_and_connector] = result;
}

void CloudBinaryUploadService::MaybeRunAuthorizationCallbacks(
    const std::string& dm_token,
    enterprise_connectors::AnalysisConnector connector) {
  TokenAndConnector token_and_connector = {dm_token, connector};
  if (!can_upload_enterprise_data_.contains(token_and_connector)) {
    return;
  }

  // TODO(crbug.com/402435358): Add test coverage to catch this regression
  // after FCM service is completely removed.
  auto it = authorization_callbacks_.find(token_and_connector);
  if (it == authorization_callbacks_.end()) {
    return;
  }
  // To avoid race condition, save the callback and erase it from the map
  // before running it.
  std::unique_ptr<base::OnceCallbackList<void(
      enterprise_connectors::ScanRequestUploadResult)>>
      callbacks = std::move(it->second);
  authorization_callbacks_.erase(it);
  callbacks->Notify(can_upload_enterprise_data_[token_and_connector]);
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

void CloudBinaryUploadService::SetAuthForTesting(
    const std::string& dm_token,
    enterprise_connectors::ScanRequestUploadResult auth_check_result) {
  for (enterprise_connectors::AnalysisConnector connector : {
           enterprise_connectors::AnalysisConnector::
               ANALYSIS_CONNECTOR_UNSPECIFIED,
           enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED,
           enterprise_connectors::AnalysisConnector::FILE_ATTACHED,
           enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY,
           enterprise_connectors::AnalysisConnector::PRINT,
#if BUILDFLAG(IS_CHROMEOS)
           enterprise_connectors::AnalysisConnector::FILE_TRANSFER,
#endif
       }) {
    TokenAndConnector token_and_connector = {dm_token, connector};
    can_upload_enterprise_data_[token_and_connector] = auth_check_result;
  }
}

void CloudBinaryUploadService::SetTokenFetcherForTesting(
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher) {
  token_fetcher_ = std::move(token_fetcher);
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
