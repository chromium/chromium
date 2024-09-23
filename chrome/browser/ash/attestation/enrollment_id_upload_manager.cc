// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/enrollment_id_upload_manager.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace {

const int kRetryDelay = 5;  // Seconds.
const int kRetryLimit = 100;

}  // namespace

namespace ash {
namespace attestation {

EnrollmentIdUploadManager::EnrollmentIdUploadManager(
    policy::CloudPolicyClient* policy_client,
    EnrollmentCertificateUploader* certificate_uploader)
    : EnrollmentIdUploadManager(policy_client,
                                DeviceSettingsService::Get(),
                                certificate_uploader) {}

EnrollmentIdUploadManager::EnrollmentIdUploadManager(
    policy::CloudPolicyClient* policy_client,
    DeviceSettingsService* device_settings_service,
    EnrollmentCertificateUploader* certificate_uploader)
    : device_settings_service_(device_settings_service),
      policy_client_(policy_client),
      certificate_uploader_(certificate_uploader),
      num_retries_(0),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  device_settings_service_->AddObserver(this);
  Start();
}

EnrollmentIdUploadManager::~EnrollmentIdUploadManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(device_settings_service_);
  device_settings_service_->RemoveObserver(this);
}

void EnrollmentIdUploadManager::DeviceSettingsUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  num_retries_ = 0;
  Start();
}

void EnrollmentIdUploadManager::Start() {
  // If identification for enrollment isn't needed, there is nothing to do.
  const enterprise_management::PolicyData* policy_data =
      device_settings_service_->policy_data();
  if (!policy_data || !policy_data->enrollment_id_needed())
    return;

  // Do not check the result of the upload request, because DMServer will
  // inform us if the enrollment ID is needed the next time device policies
  // are fetched.
  ObtainAndUploadEnrollmentId(base::DoNothing());
}

void EnrollmentIdUploadManager::ObtainAndUploadEnrollmentId(
    UploadManagerCallback callback) {
  DCHECK(!callback.is_null());

  bool start = upload_manager_callbacks_.empty();
  upload_manager_callbacks_.push(std::move(callback));

  // Do not allow multiple concurrent requests.
  if (start) {
    certificate_uploader_->ObtainAndUploadCertificate(base::BindOnce(
        &EnrollmentIdUploadManager::OnEnrollmentCertificateUploaded,
        weak_factory_.GetWeakPtr()));
  }
}

void EnrollmentIdUploadManager::OnEnrollmentCertificateUploaded(
    EnrollmentCertificateUploader::Status status) {
  switch (status) {
    case EnrollmentCertificateUploader::Status::kSuccess:
      // Enrollment certificate uploaded successfully. No need to compute EID.
      RunCallbacks(/*status=*/true);
      break;
    case EnrollmentCertificateUploader::Status::kFailedToFetch:
      LOG(WARNING) << "EnrollmentIdUploadManager: Failed to fetch certificate. "
                      "Trying to compute and upload enrollment ID.";
      GetEnrollmentId();
      break;
    case EnrollmentCertificateUploader::Status::kFailedToUpload:
      // Enrollment certificate was fetched but not uploaded. It can be uploaded
      // later so we will not proceed with computed EID.
      RunCallbacks(/*status=*/false);
      break;
    case EnrollmentCertificateUploader::Status::kInvalidClient:
      // Enrollment certificate was not uploaded due to invalid
      // `CloudPolicyClient`. The certificate can be uploaded later when the
      // client is working again. The manager is also not able to upload EID
      // with invalid `CloudPolicyClient` so there is no reason to fall back to
      // EID computation.
      RunCallbacks(/*status=*/false);
      break;
  }
}

void EnrollmentIdUploadManager::GetEnrollmentId() {
  // If we already uploaded an empty identification, we are done, because
  // we asked to compute and upload it only if the PCA refused to give
  // us an enrollment certificate, an error that will happen again (the
  // AIK certificate sent to request an enrollment certificate does not
  // contain an EID).
  if (did_upload_empty_eid_) {
    RunCallbacks(/*status=*/false);
    return;
  }

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "EnrollmentIdUploadManager: Invalid CloudPolicyClient.";
    RunCallbacks(/*status=*/false);
    return;
  }

  ::attestation::GetEnrollmentIdRequest request;
  request.set_ignore_cache(true);
  AttestationClient::Get()->GetEnrollmentId(
      request, base::BindOnce(&EnrollmentIdUploadManager::OnGetEnrollmentId,
                              weak_factory_.GetWeakPtr()));
}

void EnrollmentIdUploadManager::OnGetEnrollmentId(
    const ::attestation::GetEnrollmentIdReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(WARNING) << "Failed to get enrollment ID: " << reply.status();
    RescheduleGetEnrollmentId();
    return;
  }
  if (reply.enrollment_id().empty()) {
    LOG(WARNING) << "EnrollmentIdUploadManager: The enrollment identifier "
                    "obtained is empty.";
  }
  policy_client_->UploadEnterpriseEnrollmentId(
      reply.enrollment_id(),
      base::BindOnce(&EnrollmentIdUploadManager::OnUploadComplete,
                     weak_factory_.GetWeakPtr(), reply.enrollment_id()));
}

void EnrollmentIdUploadManager::RescheduleGetEnrollmentId() {
  if (++num_retries_ < retry_limit_) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EnrollmentIdUploadManager::GetEnrollmentId,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(retry_delay_));
  } else {
    LOG(WARNING) << "EnrollmentIdUploadManager: Retry limit exceeded.";
    RunCallbacks(/*status=*/false);
  }
}

void EnrollmentIdUploadManager::OnUploadComplete(
    const std::string& enrollment_id,
    policy::CloudPolicyClient::Result result) {
  const std::string printable_enrollment_id =
      base::ToLowerASCII(base::HexEncode(enrollment_id));

  if (!result.IsSuccess()) {
    LOG(ERROR) << "Failed to upload Enrollment Identifier \""
               << printable_enrollment_id << "\" to DMServer.";
    RunCallbacks(/*status=*/false);
    return;
  }

  if (enrollment_id.empty())
    did_upload_empty_eid_ = true;

  VLOG(1) << "Enrollment Identifier \"" << printable_enrollment_id
          << "\" uploaded to DMServer.";
  RunCallbacks(/*status=*/true);
}

void EnrollmentIdUploadManager::RunCallbacks(bool status) {
  std::queue<UploadManagerCallback> callbacks;
  callbacks.swap(upload_manager_callbacks_);

  while (!callbacks.empty()) {
    std::move(callbacks.front()).Run(status);
    callbacks.pop();
  }
}

}  // namespace attestation
}  // namespace ash
