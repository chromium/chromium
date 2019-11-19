// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"
#include "chrome/browser/safe_browsing/download_protection/multipart_uploader.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

namespace safe_browsing {
namespace {

const int kScanningTimeoutSeconds = 5 * 60;           // 5 minutes

// TODO(crbug/1020434): Once we have an endpoint for uploads, place the URL
// here.
const char kSbBinaryUploadUrl[] = "";

policy::DMToken* GetTestingDMTokenStorage() {
  static policy::DMToken dm_token =
      policy::DMToken::CreateEmptyTokenForTesting();
  return &dm_token;
}

policy::DMToken GetDMToken() {
  policy::DMToken dm_token = *GetTestingDMTokenStorage();

#if !defined(OS_CHROMEOS)
  // This is not compiled on chromeos because
  // ChromeBrowserCloudManagementController does not exist.  Also,
  // policy::BrowserDMTokenStorage::Get()->RetrieveDMToken() does not return a
  // valid token either.  Once these are fixed the #if !defined can be removed.

  if (dm_token.is_empty() &&
      policy::ChromeBrowserCloudManagementController::IsEnabled()) {
    dm_token = policy::BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
  }
#endif

  return dm_token;
}

}  // namespace

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(BinaryFCMService::Create(profile)),
      weakptr_factory_(this) {}

BinaryUploadService::BinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<BinaryFCMService> binary_fcm_service)
    : url_loader_factory_(url_loader_factory),
      binary_fcm_service_(std::move(binary_fcm_service)),
      weakptr_factory_(this) {}

BinaryUploadService::~BinaryUploadService() {}

void BinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  can_upload_data_ = true;
  if (!can_upload_data_.has_value()) {
    IsAuthorized(
        base::BindOnce(&BinaryUploadService::MaybeUploadForDeepScanningCallback,
                       weakptr_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  MaybeUploadForDeepScanningCallback(std::move(request),
                                     can_upload_data_.value());
}

void BinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadService::Request> request,
    bool authorized) {
  // Ignore the request if the browser cannot upload data.
  if (!authorized)
    return;
  UploadForDeepScanning(std::move(request));
}

void BinaryUploadService::UploadForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Request* raw_request = request.get();
  active_requests_[raw_request] = std::move(request);
  start_times_[raw_request] = base::TimeTicks::Now();

  if (!binary_fcm_service_) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&BinaryUploadService::FinishRequest,
                                  weakptr_factory_.GetWeakPtr(), raw_request,
                                  Result::FAILED_TO_GET_TOKEN,
                                  DeepScanningClientResponse()));
    return;
  }

  std::string token = base::RandBytesAsString(128);
  token = base::HexEncode(token.data(), token.size());
  active_tokens_[raw_request] = token;
  binary_fcm_service_->SetCallbackForToken(
      token, base::BindRepeating(&BinaryUploadService::OnGetResponse,
                                 weakptr_factory_.GetWeakPtr(), raw_request));
  raw_request->set_request_token(std::move(token));

  binary_fcm_service_->GetInstanceID(
      base::BindOnce(&BinaryUploadService::OnGetInstanceID,
                     weakptr_factory_.GetWeakPtr(), raw_request));
  active_timers_[raw_request] = std::make_unique<base::OneShotTimer>();
  active_timers_[raw_request]->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kScanningTimeoutSeconds),
      base::BindOnce(&BinaryUploadService::OnTimeout,
                     weakptr_factory_.GetWeakPtr(), raw_request));
}

void BinaryUploadService::OnGetInstanceID(Request* request,
                                          const std::string& instance_id) {
  if (!IsActive(request))
    return;

  if (instance_id == BinaryFCMService::kInvalidId) {
    FinishRequest(request, Result::FAILED_TO_GET_TOKEN,
                  DeepScanningClientResponse());
    return;
  }

  request->set_fcm_token(instance_id);
  request->GetRequestData(base::BindOnce(&BinaryUploadService::OnGetRequestData,
                                         weakptr_factory_.GetWeakPtr(),
                                         request));
}

void BinaryUploadService::OnGetRequestData(Request* request,
                                           Result result,
                                           const Request::Data& data) {
  if (!IsActive(request))
    return;

  if (result != Result::SUCCESS) {
    FinishRequest(request, result, DeepScanningClientResponse());
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_binary_upload", R"(
        semantics {
          sender: "Safe Browsing Download Protection"
          description:
            "For users with the enterprise policy "
            "SendFilesForMalwareCheck set, when a file is "
            "downloaded, Chrome will upload that file to Safe Browsing for "
            "detailed scanning."
          trigger:
            "The browser will upload the file to Google when "
            "the user downloads a file, and the enterprise policy "
            "SendFilesForMalwareCheck is set."
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
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 0
            }
          }
          chrome_policy {
            SendFilesForMalwareCheck {
              SendFilesForMalwareCheck: 1
            }
          }
        }
        comments: "Setting SendFilesForMalwareCheck to 0 (Do not scan "
          "downloads) or 1 (Forbid the scanning of downloads) will disable "
          "this feature"
        )");

  std::string metadata;
  request->deep_scanning_request().SerializeToString(&metadata);
  base::Base64Encode(metadata, &metadata);

  auto upload_request = MultipartUploadRequest::Create(
      url_loader_factory_, GURL(kSbBinaryUploadUrl), metadata, data.contents,
      traffic_annotation,
      base::BindOnce(&BinaryUploadService::OnUploadComplete,
                     weakptr_factory_.GetWeakPtr(), request));
  upload_request->Start();
  active_uploads_[request] = std::move(upload_request);
}

void BinaryUploadService::OnUploadComplete(Request* request,
                                           bool success,
                                           const std::string& response_data) {
  if (!IsActive(request))
    return;

  if (!success) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  DeepScanningClientResponse());
    return;
  }

  DeepScanningClientResponse response;
  if (!response.ParseFromString(response_data)) {
    FinishRequest(request, Result::UPLOAD_FAILURE,
                  DeepScanningClientResponse());
    return;
  }

  active_uploads_.erase(request);

  // Synchronous scans can return results in the initial response proto, so
  // check for those.
  OnGetResponse(request, response);
}

void BinaryUploadService::OnGetResponse(Request* request,
                                        DeepScanningClientResponse response) {
  if (!IsActive(request))
    return;

  if (response.has_dlp_scan_verdict()) {
    received_dlp_verdicts_[request].reset(response.release_dlp_scan_verdict());
  }

  if (response.has_malware_scan_verdict()) {
    received_malware_verdicts_[request].reset(
        response.release_malware_scan_verdict());
  }

  MaybeFinishRequest(request);
}

void BinaryUploadService::MaybeFinishRequest(Request* request) {
  bool requested_dlp_scan_response =
      request->deep_scanning_request().has_dlp_scan_request();
  auto received_dlp_response = received_dlp_verdicts_.find(request);
  if (requested_dlp_scan_response &&
      received_dlp_response == received_dlp_verdicts_.end()) {
    return;
  }

  bool requested_malware_scan_response =
      request->deep_scanning_request().has_malware_scan_request();
  auto received_malware_response = received_malware_verdicts_.find(request);
  if (requested_malware_scan_response &&
      received_malware_response == received_malware_verdicts_.end()) {
    return;
  }

  DeepScanningClientResponse response;
  if (requested_dlp_scan_response) {
    // Transfers ownership of the DLP response to |response|.
    response.set_allocated_dlp_scan_verdict(
        received_dlp_response->second.release());
  }

  if (requested_malware_scan_response) {
    // Transfers ownership of the malware response to |response|.
    response.set_allocated_malware_scan_verdict(
        received_malware_response->second.release());
  }

  FinishRequest(request, Result::SUCCESS, std::move(response));
}

void BinaryUploadService::OnTimeout(Request* request) {
  if (IsActive(request))
    FinishRequest(request, Result::TIMEOUT, DeepScanningClientResponse());
}

void BinaryUploadService::FinishRequest(Request* request,
                                        Result result,
                                        DeepScanningClientResponse response) {
  RecordRequestMetrics(request, result, response);

  request->FinishRequest(result, response);
  active_requests_.erase(request);
  active_timers_.erase(request);
  active_uploads_.erase(request);
  received_malware_verdicts_.erase(request);
  received_dlp_verdicts_.erase(request);

  auto token_it = active_tokens_.find(request);
  if (token_it != active_tokens_.end()) {
    binary_fcm_service_->ClearCallbackForToken(token_it->second);
    active_tokens_.erase(token_it);
  }
}

void BinaryUploadService::RecordRequestMetrics(
    Request* request,
    Result result,
    const DeepScanningClientResponse& response) {
  base::UmaHistogramEnumeration("SafeBrowsingBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes("SafeBrowsingBinaryUploadRequest.Duration",
                                base::TimeTicks::Now() - start_times_[request],
                                base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(6), 50);

  if (response.has_malware_scan_verdict()) {
    // For now just distinguish safe from unsafe verdicts.
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.MalwareResult",
                              response.malware_scan_verdict().verdict() !=
                                  MalwareDeepScanningVerdict::CLEAN);
  }

  if (response.has_dlp_scan_verdict()) {
    base::UmaHistogramBoolean("SafeBrowsingBinaryUploadRequest.DlpResult",
                              response.dlp_scan_verdict().status() ==
                                  DlpDeepScanningVerdict::SUCCESS);
  }
}

BinaryUploadService::Request::Data::Data() = default;

BinaryUploadService::Request::Request(Callback callback)
    : callback_(std::move(callback)) {}

BinaryUploadService::Request::~Request() {}

void BinaryUploadService::Request::set_request_dlp_scan(
    DlpDeepScanningClientRequest dlp_request) {
  *deep_scanning_request_.mutable_dlp_scan_request() = std::move(dlp_request);
}

void BinaryUploadService::Request::set_request_malware_scan(
    MalwareDeepScanningClientRequest malware_request) {
  *deep_scanning_request_.mutable_malware_scan_request() =
      std::move(malware_request);
}

void BinaryUploadService::Request::set_fcm_token(const std::string& token) {
  deep_scanning_request_.set_fcm_notification_token(token);
}

void BinaryUploadService::Request::set_dm_token(const std::string& token) {
  deep_scanning_request_.set_dm_token(token);
}

void BinaryUploadService::Request::set_request_token(const std::string& token) {
  deep_scanning_request_.set_request_token(token);
}

void BinaryUploadService::Request::FinishRequest(
    Result result,
    DeepScanningClientResponse response) {
  std::move(callback_).Run(result, response);
}

bool BinaryUploadService::IsActive(Request* request) {
  return (active_requests_.find(request) != active_requests_.end());
}

class ValidateDataUploadRequest : public BinaryUploadService::Request {
 public:
  explicit ValidateDataUploadRequest(BinaryUploadService::Callback callback)
      : BinaryUploadService::Request(std::move(callback)) {}
  ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
  ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
      delete;
  ~ValidateDataUploadRequest() override = default;

 private:
  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;
};

inline void ValidateDataUploadRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(BinaryUploadService::Result::SUCCESS,
                          BinaryUploadService::Request::Data());
}

void BinaryUploadService::IsAuthorized(AuthorizationCallback callback) {
  // Start |timer_| on the first call to IsAuthorized. This is necessary in
  // order to invalidate the authorization every 24 hours.
  if (!timer_.IsRunning()) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromHours(24), this,
                 &BinaryUploadService::ResetAuthorizationData);
  }

  if (!can_upload_data_.has_value()) {
    // Send a request to check if the browser can upload data.
    if (!pending_validate_data_upload_request_) {
      auto dm_token = GetDMToken();
      if (!dm_token.is_valid()) {
        std::move(callback).Run(false);
        return;
      }

      pending_validate_data_upload_request_ = true;
      auto request = std::make_unique<ValidateDataUploadRequest>(base::BindOnce(
          &BinaryUploadService::ValidateDataUploadRequestCallback,
          weakptr_factory_.GetWeakPtr()));
      request->set_dm_token(dm_token.value());
      UploadForDeepScanning(std::move(request));
    }
    authorization_callbacks_.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(can_upload_data_.value());
}

void BinaryUploadService::ValidateDataUploadRequestCallback(
    BinaryUploadService::Result result,
    DeepScanningClientResponse response) {
  pending_validate_data_upload_request_ = false;
  can_upload_data_ = result == BinaryUploadService::Result::SUCCESS;
  RunAuthorizationCallbacks();
}

void BinaryUploadService::RunAuthorizationCallbacks() {
  DCHECK(can_upload_data_.has_value());
  for (auto& callback : authorization_callbacks_) {
    std::move(callback).Run(can_upload_data_.value());
  }
  authorization_callbacks_.clear();
}

void BinaryUploadService::ResetAuthorizationData() {
  // Setting |can_upload_data_| to base::nullopt will make the next call to
  // IsAuthorized send out a request to validate data uploads.
  can_upload_data_ = base::nullopt;

  // Call IsAuthorized  to update |can_upload_data_| right away.
  IsAuthorized(base::DoNothing());
}

// static
bool BinaryUploadService::ShouldBlockFileSize(size_t file_size) {
  int block_large_file_transfer = g_browser_process->local_state()->GetInteger(
      prefs::kBlockLargeFileTransfer);
  if (block_large_file_transfer !=
          BlockLargeFileTransferValues::BLOCK_LARGE_DOWNLOADS &&
      block_large_file_transfer !=
          BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS)
    return false;

  return (file_size > kMaxUploadSizeBytes);
}

// static
GURL BinaryUploadService::GetUploadUrl() {
  return GURL(kSbBinaryUploadUrl);
}

}  // namespace safe_browsing
