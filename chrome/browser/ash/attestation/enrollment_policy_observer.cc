// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/enrollment_policy_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate.h"

namespace {

const int kRetryDelay = 5;  // Seconds.
const int kRetryLimit = 100;

}  // namespace

namespace ash {
namespace attestation {

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
    policy::CloudPolicyClient* policy_client,
    EnrollmentCertificateUploader* certificate_uploader)
    : EnrollmentPolicyObserver(policy_client,
                               DeviceSettingsService::Get(),
                               certificate_uploader) {}

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
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

EnrollmentPolicyObserver::~EnrollmentPolicyObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(DeviceSettingsService::IsInitialized());
  device_settings_service_->RemoveObserver(this);
}

void EnrollmentPolicyObserver::DeviceSettingsUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  num_retries_ = 0;
  Start();
}

void EnrollmentPolicyObserver::Start() {
  // If identification for enrollment isn't needed, there is nothing to do.
  const enterprise_management::PolicyData* policy_data =
      device_settings_service_->policy_data();
  if (!policy_data || !policy_data->enrollment_id_needed())
    return;

  // Do not allow multiple concurrent starts.
  if (request_in_flight_)
    return;

  request_in_flight_ = true;

  ObtainAndUploadCertificate();
}

void EnrollmentPolicyObserver::ObtainAndUploadCertificate() {
  certificate_uploader_->ObtainAndUploadCertificate(
      base::BindOnce(&EnrollmentPolicyObserver::OnEnrollmentCertificateUploaded,
                     weak_factory_.GetWeakPtr()));
}

void EnrollmentPolicyObserver::OnEnrollmentCertificateUploaded(
    EnrollmentCertificateUploader::Status status) {
  switch (status) {
    case EnrollmentCertificateUploader::Status::kSuccess:
      // Enrollment certificate uploaded successfully. No need to compute EID.
      request_in_flight_ = false;
      break;
    case EnrollmentCertificateUploader::Status::kFailedToFetch:
      LOG(WARNING) << "EnrollmentPolicyObserver: Failed to fetch certificate. "
                      "Trying to compute and upload enrollment ID.";
      GetEnrollmentId();
      break;
    case EnrollmentCertificateUploader::Status::kFailedToUpload:
      // Enrollment certificate was fetched but not uploaded. It can be uploaded
      // later so we will not proceed with computed EID.
      request_in_flight_ = false;
      break;
  }
}

void EnrollmentPolicyObserver::GetEnrollmentId() {
  // If we already uploaded an empty identification, we are done, because
  // we asked to compute and upload it only if the PCA refused to give
  // us an enrollment certificate, an error that will happen again (the
  // AIK certificate sent to request an enrollment certificate does not
  // contain an EID).
  if (did_upload_empty_eid_) {
    request_in_flight_ = false;
    return;
  }

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "EnrollmentPolicyObserver: Invalid CloudPolicyClient.";
    request_in_flight_ = false;
    return;
  }

  ::attestation::GetEnrollmentIdRequest request;
  request.set_ignore_cache(true);
  AttestationClient::Get()->GetEnrollmentId(
      request, base::BindOnce(&EnrollmentPolicyObserver::OnGetEnrollmentId,
                              weak_factory_.GetWeakPtr()));
}

void EnrollmentPolicyObserver::OnGetEnrollmentId(
    const ::attestation::GetEnrollmentIdReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(WARNING) << "Failed to get enrollment ID: " << reply.status();
    RescheduleGetEnrollmentId();
    return;
  }
  if (reply.enrollment_id().empty()) {
    LOG(WARNING) << "EnrollmentPolicyObserver: The enrollment identifier"
                    " obtained is empty.";
  }
  policy_client_->UploadEnterpriseEnrollmentId(
      reply.enrollment_id(),
      base::BindOnce(&EnrollmentPolicyObserver::OnUploadComplete,
                     weak_factory_.GetWeakPtr(), reply.enrollment_id()));
}

void EnrollmentPolicyObserver::RescheduleGetEnrollmentId() {
  if (++num_retries_ < retry_limit_) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EnrollmentPolicyObserver::GetEnrollmentId,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(retry_delay_));
  } else {
    LOG(WARNING) << "EnrollmentPolicyObserver: Retry limit exceeded.";
    request_in_flight_ = false;
  }
}

void EnrollmentPolicyObserver::OnUploadComplete(
    const std::string& enrollment_id,
    bool status) {
  const std::string& printable_enrollment_id = base::ToLowerASCII(
      base::HexEncode(enrollment_id.data(), enrollment_id.size()));
  request_in_flight_ = false;
  if (status) {
    if (enrollment_id.empty())
      did_upload_empty_eid_ = true;
  } else {
    LOG(ERROR) << "Failed to upload Enrollment Identifier \""
               << printable_enrollment_id << "\" to DMServer.";
    return;
  }
  VLOG(1) << "Enrollment Identifier \"" << printable_enrollment_id
          << "\" uploaded to DMServer.";
}

}  // namespace attestation
}  // namespace ash
