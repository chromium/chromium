// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/public_saml_url_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;
namespace {
std::string GetDeviceId() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetInstallAttributes()->GetDeviceId();
}

std::string GetAccountId(std::string user_id) {
  std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(chromeos::CrosSettings::Get());
  for (auto account : device_local_accounts) {
    if (account.user_id == user_id) {
      return account.account_id;
    }
  }
  return std::string();
}
}  // namespace

namespace chromeos {
PublicSamlUrlFetcher::PublicSamlUrlFetcher(AccountId account_id)
    : account_id_(GetAccountId(account_id.GetUserEmail())) {}

PublicSamlUrlFetcher::~PublicSamlUrlFetcher() = default;

std::string PublicSamlUrlFetcher::GetRedirectUrl() {
  return redirect_url_;
}

bool PublicSamlUrlFetcher::FetchSucceeded() {
  return fetch_succeeded_;
}

void PublicSamlUrlFetcher::Fetch(base::OnceClosure callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  policy::DeviceManagementService* service =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->device_management_service();
  std::unique_ptr<policy::DMServerJobConfiguration> config = std::make_unique<
      policy::DMServerJobConfiguration>(
      service,
      policy::DeviceManagementService::JobConfiguration::TYPE_REQUEST_SAML_URL,
      GetDeviceId(), /*critical=*/false,
      policy::DMAuth::FromDMToken(chromeos::DeviceSettingsService::Get()
                                      ->policy_data()
                                      ->request_token()),
      /*oauth_token=*/base::nullopt,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::Bind(&PublicSamlUrlFetcher::OnPublicSamlUrlReceived,
                 weak_ptr_factory_.GetWeakPtr()));

  em::PublicSamlUserRequest* saml_url_request =
      config->request()->mutable_public_saml_user_request();
  saml_url_request->set_account_id(account_id_);
  fetch_request_job_ = service->CreateJob(std::move(config));
}

void PublicSamlUrlFetcher::OnPublicSamlUrlReceived(
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus dm_status,
    int net_error,
    const enterprise_management::DeviceManagementResponse& response) {
  VLOG(1) << "Public SAML url response received. DM Status: " << dm_status;
  fetch_request_job_.reset();
  std::string user_id;

  switch (dm_status) {
    case policy::DM_STATUS_SUCCESS: {
      if (!response.has_public_saml_user_response()) {
        LOG(WARNING) << "Invalid public SAML url response.";
        break;
      }
      const em::PublicSamlUserResponse& saml_url_response =
          response.public_saml_user_response();

      if (!saml_url_response.has_saml_parameters()) {
        LOG(WARNING) << "Invalid public SAML url response.";
        break;
      }
      // Fetch has succeeded.
      fetch_succeeded_ = true;
      const em::SamlParametersProto& saml_params =
          saml_url_response.saml_parameters();
      redirect_url_ = saml_params.auth_redirect_url();
      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Fetching public SAML url failed. DM Status: " << dm_status;
      break;
    }
  }
  std::move(callback_).Run();
}

}  // namespace chromeos
