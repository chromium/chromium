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
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

AccountStatus::Type ParseAccountStatusType(
    const em::CheckUserAccountResponse& response,
    const std::string& email) {
  if (!response.has_user_account_type()) {
    return AccountStatus::Type::kUnknown;
  }
  if (response.user_account_type() ==
      em::CheckUserAccountResponse::UNKNOWN_USER_ACCOUNT_TYPE) {
    return AccountStatus::Type::kUnknown;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::CONSUMER) {
    const std::string domain = gaia::ExtractDomainName(email);
    if (enterprise_util::IsKnownConsumerDomain(domain)) {
      return AccountStatus::Type::kConsumerWithConsumerDomain;
    }
    return AccountStatus::Type::kConsumerWithBusinessDomain;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::DASHER) {
    return AccountStatus::Type::kDasher;
  }

  if (response.user_account_type() == em::CheckUserAccountResponse::NOT_EXIST) {
    if (!response.has_domain_verified()) {
      return AccountStatus::Type::kUnknown;
    }
    if (response.domain_verified()) {
      return AccountStatus::Type::kOrganisationalAccountVerified;
    }
    return AccountStatus::Type::kOrganisationalAccountUnverified;
  }
  return AccountStatus::Type::kUnknown;
}

EnrollmentNudgePolicyFetchResult ParseEnrollmentNudgePolicy(
    const em::CheckUserAccountResponse& response) {
  if (!response.has_enrollment_nudge_type()) {
    return EnrollmentNudgePolicyFetchResult::kNoPolicyInResponse;
  }
  switch (response.enrollment_nudge_type()) {
    case em::CheckUserAccountResponse::UNKNOWN_ENROLLMENT_NUDGE_TYPE:
      return EnrollmentNudgePolicyFetchResult::kUnknown;
    case em::CheckUserAccountResponse::NONE:
      return EnrollmentNudgePolicyFetchResult::kAllowConsumerSignIn;
    case em::CheckUserAccountResponse::ENROLLMENT_REQUIRED:
      return EnrollmentNudgePolicyFetchResult::kEnrollmentRequired;
  }
  LOG(ERROR)
      << "Unexpected enrollment nudge policy value received from DM server: "
      << static_cast<int>(response.enrollment_nudge_type());
  return EnrollmentNudgePolicyFetchResult::kUnknown;
}

void RecordAccountStatusCheckResult(AccountStatus::Type value) {
  base::UmaHistogramEnumeration("Enterprise.AccountStatusCheckResult", value);
}

void RecordEnrollmentNudgePolicyFetchResult(
    EnrollmentNudgePolicyFetchResult value) {
  base::UmaHistogramEnumeration("Enterprise.EnrollmentNudge.PolicyFetchResult",
                                value);
}

}  // namespace

bool operator==(const AccountStatus& lhs, const AccountStatus& rhs) {
  return lhs.type == rhs.type &&
         lhs.enrollment_required == rhs.enrollment_required;
}

bool operator!=(const AccountStatus& lhs, const AccountStatus& rhs) {
  return !(lhs == rhs);
}

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

void AccountStatusCheckFetcher::Fetch(FetchCallback callback,
                                      bool fetch_enrollment_nudge_policy) {
  CHECK(!callback_);
  CHECK(callback);
  callback_ = std::move(callback);
  is_fetching_enrollment_nudge_policy_ = fetch_enrollment_nudge_policy;
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          service_,
          DeviceManagementService::JobConfiguration::TYPE_CHECK_USER_ACCOUNT,
          random_device_id_, /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/std::nullopt, url_loader_factory_,
          base::BindOnce(
              &AccountStatusCheckFetcher::OnAccountStatusCheckReceived,
              weak_ptr_factory_.GetWeakPtr()));

  em::CheckUserAccountRequest* request =
      config->request()->mutable_check_user_account_request();
  request->set_user_email(email_);
  request->set_enrollment_nudge_request(fetch_enrollment_nudge_policy);
  fetch_request_job_ = service_->CreateJob(std::move(config));
}

void AccountStatusCheckFetcher::OnAccountStatusCheckReceived(
    DMServerJobResult result) {
  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Account check response received. DM Status: "
               << result.dm_status;

  fetch_request_job_.reset();
  std::string user_id;
  bool fetch_succeeded = false;
  AccountStatus account_status = {.type = AccountStatus::Type::kUnknown,
                                  .enrollment_required = false};
  switch (result.dm_status) {
    case DM_STATUS_SUCCESS: {
      if (!result.response.has_check_user_account_response()) {
        LOG(WARNING) << "Invalid Account check response.";
        break;
      }

      // Fetch has succeeded.
      fetch_succeeded = true;
      const em::CheckUserAccountResponse& response =
          result.response.check_user_account_response();
      const EnrollmentNudgePolicyFetchResult enrollment_nudge_policy =
          ParseEnrollmentNudgePolicy(response);
      account_status = {
          .type = ParseAccountStatusType(response, email_),
          .enrollment_required =
              enrollment_nudge_policy ==
              EnrollmentNudgePolicyFetchResult::kEnrollmentRequired};

      if (is_fetching_enrollment_nudge_policy_) {
        RecordEnrollmentNudgePolicyFetchResult(enrollment_nudge_policy);
      } else {
        // This call records UMA which is intended to reflect the account status
        // checks in enrollment flow. Enrollment nudge use-cases should not
        // affect it.
        RecordAccountStatusCheckResult(account_status.type);
      }

      if (account_status.enrollment_required &&
          account_status.type != AccountStatus::Type::kDasher) {
        LOG(ERROR)
            << "Unexpected response from DM Server: Enrollment Nudge policy is "
               "set to require enrollment for a non-Dasher account.";
        account_status.enrollment_required = false;
      }

      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Account check failed. DM Status: " << result.dm_status;
      break;
    }
  }
  std::move(callback_).Run(fetch_succeeded, account_status);
}

}  // namespace policy
