// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_robot_auth_code_fetcher.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
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

}  // namespace

namespace arc {

ArcRobotAuthCodeFetcher::ArcRobotAuthCodeFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      browser_policy_connector_ash_(CHECK_DEREF(browser_policy_connector_ash)) {
  CHECK(shared_url_loader_factory_);
}

ArcRobotAuthCodeFetcher::~ArcRobotAuthCodeFetcher() = default;

void ArcRobotAuthCodeFetcher::Fetch(FetchCallback callback) {
  CHECK(!fetch_request_job_, base::NotFatalUntil::M160);
  const policy::CloudPolicyClient* client =
      browser_policy_connector_ash_->GetDeviceCloudPolicyManager()
          ->core()
          ->client();
  if (!client) {
    LOG(WARNING) << "Synchronously failing auth request because "
                    "CloudPolicyClient is not initialized.";
    std::move(callback).Run(false, std::string());
    return;
  }

  policy::DeviceManagementService* service =
      browser_policy_connector_ash_->device_management_service();
  std::unique_ptr<policy::DMServerJobConfiguration> config =
      std::make_unique<policy::DMServerJobConfiguration>(
          service,
          policy::DeviceManagementService::JobConfiguration::
              TYPE_API_AUTH_CODE_FETCH,
          client->client_id(), /*critical=*/false,
          policy::DMAuth::FromDMToken(client->dm_token()),
          /*oauth_token=*/std::nullopt, shared_url_loader_factory_,
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
