// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/uuid.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

AccountStatusCheckFetcher::AccountStatus ParseStatus(
    const em::CheckUserAccountResponse& response,
    const std::string& email) {
  if (!response.has_user_account_type()) {
    return AccountStatusCheckFetcher::AccountStatus::kUnknown;
  }
  if (response.user_account_type() ==
      em::CheckUserAccountResponse::UNKNOWN_USER_ACCOUNT_TYPE) {
    return AccountStatusCheckFetcher::AccountStatus::kUnknown;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::CONSUMER) {
    const std::string domain = gaia::ExtractDomainName(email);
    if (chrome::enterprise_util::IsKnownConsumerDomain(domain)) {
      return AccountStatusCheckFetcher::AccountStatus::
          kConsumerWithConsumerDomain;
    }
    return AccountStatusCheckFetcher::AccountStatus::
        kConsumerWithBusinessDomain;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::DASHER) {
    return AccountStatusCheckFetcher::AccountStatus::kDasher;
  }

  if (response.user_account_type() == em::CheckUserAccountResponse::NOT_EXIST) {
    if (!response.has_domain_verified()) {
      return AccountStatusCheckFetcher::AccountStatus::kUnknown;
    }
    if (response.domain_verified()) {
      return AccountStatusCheckFetcher::AccountStatus::
          kOrganisationalAccountVerified;
    }
    return AccountStatusCheckFetcher::AccountStatus::
        kOrganisationalAccountUnverified;
  }
  return AccountStatusCheckFetcher::AccountStatus::kUnknown;
}

void RecordAccountStatusCheckResult(
    AccountStatusCheckFetcher::AccountStatus value) {
  base::UmaHistogramEnumeration("Enterprise.AccountStatusCheckResult", value);
}

}  // namespace

AccountStatusCheckFetcher::AccountStatusCheckFetcher(
    const std::string& canonicalized_email)
    : AccountStatusCheckFetcher(
          canonicalized_email,
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->device_management_service(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory()) {}

AccountStatusCheckFetcher::AccountStatusCheckFetcher(
    const std::string& canonicalized_email,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : email_(canonicalized_email),
      service_(service),
      url_loader_factory_(url_loader_factory),
      random_device_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()) {}

AccountStatusCheckFetcher::~AccountStatusCheckFetcher() = default;

void AccountStatusCheckFetcher::Fetch(FetchCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = std::move(callback);
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          service_,
          DeviceManagementService::JobConfiguration::TYPE_CHECK_USER_ACCOUNT,
          random_device_id_, /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/absl::nullopt, url_loader_factory_,
          base::BindOnce(
              &AccountStatusCheckFetcher::OnAccountStatusCheckReceived,
              weak_ptr_factory_.GetWeakPtr()));

  em::CheckUserAccountRequest* request =
      config->request()->mutable_check_user_account_request();
  request->set_user_email(email_);
  fetch_request_job_ = service_->CreateJob(std::move(config));
}

void AccountStatusCheckFetcher::OnAccountStatusCheckReceived(
    DMServerJobResult result) {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Account check response received. DM Status: "
               << result.dm_status;

  fetch_request_job_.reset();
  std::string user_id;
  bool fetch_succeeded = false;
  switch (result.dm_status) {
    case DM_STATUS_SUCCESS: {
      if (!result.response.has_check_user_account_response()) {
        LOG(WARNING) << "Invalid Account check response.";
        break;
      }

      // Fetch has succeeded.
      fetch_succeeded = true;
      result_ =
          ParseStatus(result.response.check_user_account_response(), email_);
      RecordAccountStatusCheckResult(result_);
      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Account check failed. DM Status: " << result.dm_status;
      break;
    }
  }
  std::move(callback_).Run(fetch_succeeded, result_);
}

}  // namespace policy
