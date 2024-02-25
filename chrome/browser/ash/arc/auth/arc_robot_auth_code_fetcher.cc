// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_robot_auth_code_fetcher.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// OAuth2 Client id of Android.
constexpr char kAndoidClientId[] =
    "1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.googleusercontent.com";

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorAsh* const connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->device_management_service();
}

const policy::CloudPolicyClient* GetCloudPolicyClient() {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  return policy_manager->core()->client();
}

}  // namespace

namespace arc {

ArcRobotAuthCodeFetcher::ArcRobotAuthCodeFetcher() {}

ArcRobotAuthCodeFetcher::~ArcRobotAuthCodeFetcher() = default;

void ArcRobotAuthCodeFetcher::Fetch(FetchCallback callback) {
  DCHECK(!fetch_request_job_);
  const policy::CloudPolicyClient* client = GetCloudPolicyClient();
  if (!client) {
    LOG(WARNING) << "Synchronously failing auth request because "
                    "CloudPolicyClient is not initialized.";
    std::move(callback).Run(false, std::string());
    return;
  }

  policy::DeviceManagementService* service = GetDeviceManagementService();
  std::unique_ptr<policy::DMServerJobConfiguration> config =
      std::make_unique<policy::DMServerJobConfiguration>(
          service,
          policy::DeviceManagementService::JobConfiguration::
              TYPE_API_AUTH_CODE_FETCH,
          client->client_id(), /*critical=*/false,
          policy::DMAuth::FromDMToken(client->dm_token()),
          /*oauth_token=*/std::nullopt,
          url_loader_factory_for_testing()
              ? url_loader_factory_for_testing()
              : g_browser_process->system_network_context_manager()
                    ->GetSharedURLLoaderFactory(),
          base::BindOnce(
              &ArcRobotAuthCodeFetcher::OnFetchRobotAuthCodeCompleted,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  enterprise_management::DeviceServiceApiAccessRequest* request =
      config->request()->mutable_service_api_access_request();
  request->set_oauth2_client_id(kAndoidClientId);
  request->add_auth_scopes(GaiaConstants::kAnyApiOAuth2Scope);
  request->set_device_type(
      enterprise_management::DeviceServiceApiAccessRequest::ANDROID_OS);

  fetch_request_job_ = service->CreateJob(std::move(config));
}

void ArcRobotAuthCodeFetcher::OnFetchRobotAuthCodeCompleted(
    FetchCallback callback,
    policy::DMServerJobResult result) {
  fetch_request_job_.reset();

  if (result.dm_status == policy::DM_STATUS_SUCCESS &&
      (!result.response.has_service_api_access_response())) {
    LOG(WARNING) << "Invalid service api access response.";
    result.dm_status = policy::DM_STATUS_RESPONSE_DECODING_ERROR;
  }

  if (result.dm_status != policy::DM_STATUS_SUCCESS) {
    LOG(ERROR) << "Fetching of robot auth code failed. DM Status: "
               << result.dm_status;
    std::move(callback).Run(false /* success */, std::string());
    return;
  }

  std::move(callback).Run(
      true /* success */,
      result.response.service_api_access_response().auth_code());
}

}  // namespace arc
