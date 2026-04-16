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

void CloudBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  // TODO(crbug.com/501456247): Clean up this indirection layer once
  // CloudBinaryUploadServiceBase inherits from BinaryUploadService.
  CloudBinaryUploadServiceBase::MaybeUploadForDeepScanning(std::move(request));
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

BinaryUploadRequest::BrowserPolicyConnectorGetter
CloudBinaryUploadService::BrowserPolicyConnectorGetter() {
  return base::BindRepeating(&GetBrowserPolicyConnector);
}

bool CloudBinaryUploadService::IsAdvancedProtection() {
  return safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
             profile_)
      ->IsUnderAdvancedProtection();
}

bool CloudBinaryUploadService::IsEnhancedProtection() {
  return profile_ && IsEnhancedProtectionEnabled(*profile_->GetPrefs());
}

#if BUILDFLAG(IS_CHROMEOS)
bool CloudBinaryUploadService::IsManagedGuestSession() {
  return chromeos::IsManagedGuestSession();
}
#endif

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
