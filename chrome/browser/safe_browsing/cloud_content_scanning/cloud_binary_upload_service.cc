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
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#endif

namespace safe_browsing {
namespace {

using ::enterprise_connectors::BinaryUploadRequest;
using ::enterprise_connectors::GetBrowserPolicyConnector;
using ::enterprise_connectors::IsConsumerScanRequest;

bool CanUseAccessToken(const BinaryUploadRequest& request, Profile* profile) {
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

}  // namespace

CloudBinaryUploadService::CloudBinaryUploadService(Profile* profile)
    : profile_(profile) {
  url_loader_factory_ = profile->GetURLLoaderFactory();
  ui_task_runner_ = content::GetUIThreadTaskRunner({});
}

CloudBinaryUploadService::CloudBinaryUploadService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile)
    : profile_(profile) {
  url_loader_factory_ = url_loader_factory;
  ui_task_runner_ = content::GetUIThreadTaskRunner({});
}

CloudBinaryUploadService::~CloudBinaryUploadService() = default;

enterprise_connectors::ScanRequestUploadResult
CloudBinaryUploadService::GetConsumerAuthResult(
    const enterprise_connectors::BinaryUploadRequest& request) {
  DCHECK(!request.IsAuthRequest());
  const bool is_advanced_protection =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile_)
          ->IsUnderAdvancedProtection();
  const bool is_enhanced_protection =
      profile_ && IsEnhancedProtectionEnabled(*profile_->GetPrefs());

  return is_advanced_protection || is_enhanced_protection
             ? enterprise_connectors::ScanRequestUploadResult::kSuccess
             : enterprise_connectors::ScanRequestUploadResult::kUnauthorized;
}

std::optional<enterprise_connectors::ScanRequestUploadResult>
CloudBinaryUploadService::MaybeGetEnterpriseAuthResult(
    const enterprise_connectors::BinaryUploadRequest& request) {
  auto connector = request.analysis_connector();
  std::string dm_token = request.device_token();
  TokenAndConnector token_and_connector = {dm_token, connector};

  if (dm_token.empty()) {
    return enterprise_connectors::ScanRequestUploadResult::kUnauthorized;
  }

  if (!can_upload_enterprise_data_.contains(token_and_connector) ||
      can_upload_enterprise_data_[token_and_connector] !=
          enterprise_connectors::ScanRequestUploadResult::kSuccess) {
    return std::nullopt;
  }

  return can_upload_enterprise_data_[token_and_connector];
}

void CloudBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  AssertCalledOnUIThread();

  if (IsConsumerScanRequest(*request)) {
    auto consumer_auth_result = GetConsumerAuthResult(*request);
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       consumer_auth_result);
    return;
  }

  std::optional<enterprise_connectors::ScanRequestUploadResult>
      enterprise_auth_result = MaybeGetEnterpriseAuthResult(*request);
  if (enterprise_auth_result.has_value()) {
    MaybeUploadForDeepScanningCallback(std::move(request),
                                       enterprise_auth_result.value());
    return;
  }

  // Get data from `request` before calling `IsAuthorized` since it is about
  // to move.
  GURL url = request->GetUrlWithParams();
  bool per_profile_request = request->per_profile_request();
  std::string dm_token = request->device_token();
  auto connector = request->analysis_connector();

  // Send a new auth request to compute the result.
  IsAuthorized(
      std::move(url), per_profile_request,
      base::BindOnce(
          &CloudBinaryUploadService::MaybeUploadForDeepScanningCallback,
          weakptr_factory_.GetWeakPtr(), std::move(request)),
      dm_token, connector);
}

void CloudBinaryUploadService::MaybeAcknowledge(
    std::unique_ptr<enterprise_connectors::BinaryUploadAck> ack) {
  // Nothing to do for cloud upload service.
}

void CloudBinaryUploadService::MaybeCancelRequests(
    std::unique_ptr<enterprise_connectors::BinaryUploadCancelRequests> cancel) {
  AssertCalledOnUIThread();

  std::string action_id = cancel->get_user_action_id();
  if (user_action_data_.contains(action_id)) {
    user_action_data_[action_id].cancelled_time = base::TimeTicks::Now();
  }

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  base::EraseIf(
      request_queue_,
      [&cancel](const std::unique_ptr<BinaryUploadRequest>& request) {
        if (request->user_action_id() == cancel->get_user_action_id()) {
          request->FinishRequest(
              enterprise_connectors::ScanRequestUploadResult::kUserCancelled,
              enterprise_connectors::ContentAnalysisResponse());
          return true;
        }
        return false;
      });

  // Also cancel active requests.
  std::vector<BinaryUploadRequest::Id> ids_to_cancel;
  for (const auto& it : active_requests_) {
    if (it.second->user_action_id() == cancel->get_user_action_id()) {
      ids_to_cancel.push_back(it.first);
    }
  }

  for (const auto& id : ids_to_cancel) {
    FinishIfActive(
        id, enterprise_connectors::ScanRequestUploadResult::kUserCancelled,
        enterprise_connectors::ContentAnalysisResponse());
  }
}

base::WeakPtr<enterprise_connectors::BinaryUploadService>
CloudBinaryUploadService::AsWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

void CloudBinaryUploadService::MaybeUploadForDeepScanningCallback(
    std::unique_ptr<BinaryUploadRequest> request,
    enterprise_connectors::ScanRequestUploadResult auth_check_result) {
  // Ignore the request if the browser cannot upload data.
  if (auth_check_result !=
      enterprise_connectors::ScanRequestUploadResult::kSuccess) {
    request->FinishRequest(auth_check_result,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }
  QueueForDeepScanning(std::move(request));
}

void CloudBinaryUploadService::QueueForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  // Track the start time for the entire user action bundle
  std::string action_id = request->user_action_id();
  if (!action_id.empty() && !user_action_data_.contains(action_id)) {
    user_action_data_[action_id] = {
        request->cloud_or_local_settings().is_cloud_analysis(),
        safe_browsing::AccessPointFromRequest(request->analysis_connector(),
                                              request->reason())};
  }
  if (active_requests_.size() >= GetParallelActiveRequestsMax()) {
    request_queue_.push_back(std::move(request));
  } else {
    UploadForDeepScanning(std::move(request));
  }
}


void CloudBinaryUploadService::MaybeGetAccessToken(
    BinaryUploadRequest::Id request_id) {
  BinaryUploadRequest* request = GetRequest(request_id);
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
    BinaryUploadRequest::Id request_id,
    const std::string& access_token) {
  BinaryUploadRequest* request = GetRequest(request_id);
  if (!request) {
    return;
  }

  request->set_access_token(access_token);
  request->GetRequestData(
      base::BindOnce(&CloudBinaryUploadService::OnGetRequestData,
                     weakptr_factory_.GetWeakPtr(), request_id));
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
          enterprise_connectors::ScanRequestUploadResult::kSuccess) {
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
          std::move(settings), base::BindRepeating(&GetBrowserPolicyConnector));
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

}  // namespace safe_browsing
