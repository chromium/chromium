// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/util/statusor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

policy::DeviceManagementService*
EncryptedReportingClient::Delegate::device_management_service() const {
  if (!g_browser_process || !g_browser_process->browser_policy_connector()) {
    return nullptr;
  }
  return g_browser_process->browser_policy_connector()
      ->device_management_service();
}

EncryptedReportingClient::EncryptedReportingClient(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {}

EncryptedReportingClient::~EncryptedReportingClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void EncryptedReportingClient::UploadReport(
    base::Value::Dict merging_payload,
    absl::optional<base::Value::Dict> context,
    policy::CloudPolicyClient* cloud_policy_client,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::DeviceManagementService* const device_management_service =
      delegate_->device_management_service();
  if (!device_management_service) {
    std::move(callback).Run(base::unexpected(
        Status(error::NOT_FOUND,
               "Device management service required, but not found")));
    return;
  }

  // This is the case for uploading managed user events from an
  // unmanaged device. The server will authenticate by looking at the user dm
  // tokens inside the records instead of a single request-level device dm
  // token.
  policy::DMAuth auth_data = policy::DMAuth::NoAuth();

  if (cloud_policy_client) {
    // The device cloud policy client only exists on managed devices and is the
    // source of the DM token. So if the device is managed, we use the device dm
    // token as authentication.
    auth_data = policy::DMAuth::FromDMToken(cloud_policy_client->dm_token());
  }

  auto config = std::make_unique<policy::EncryptedReportingJobConfiguration>(
      g_browser_process->shared_url_loader_factory(), std::move(auth_data),
      device_management_service->configuration()
          ->GetEncryptedReportingServerUrl(),
      std::move(merging_payload), cloud_policy_client,
      base::BindOnce(&EncryptedReportingClient::OnReportUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  if (context.has_value()) {
    config->UpdateContext(std::move(context.value()));
  }
  const base::TimeDelta delay = config->WhenIsAllowedToProceed();
  if (delay.is_positive()) {
    // Reject upload.
    config->CancelNotAllowedJob();  // Invokes callback to response back.
    return;
  }
  // Accept upload.
  config->AccountForAllowedJob();
  std::unique_ptr<policy::DeviceManagementService::Job> job =
      device_management_service->CreateJob(std::move(config));
  request_jobs_.emplace(std::move(job));
}

void EncryptedReportingClient::OnReportUploadCompleted(
    ResponseCallback callback,
    policy::DeviceManagementService::Job* job,
    policy::DeviceManagementStatus status,
    int response_code,
    absl::optional<base::Value::Dict> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (job) {
    request_jobs_.erase(job);
  }
  if (response_code == ::policy::DeviceManagementService::kTooManyRequests) {
    std::move(callback).Run(base::unexpected(
        Status(error::OUT_OF_RANGE, "Too many upload requests")));
    return;
  }
  if (response_code != ::policy::DeviceManagementService::kSuccess) {
    std::move(callback).Run(base::unexpected(
        Status(error::DATA_LOSS,
               base::StrCat(
                   {"Response code: ", base::NumberToString(response_code)}))));
    return;
  }
  if (!response.has_value()) {
    std::move(callback).Run(base::unexpected(
        Status(error::DATA_LOSS, "Success response is empty")));
    return;
  }
  std::move(callback).Run(std::move(response.value()));
}

}  // namespace reporting
