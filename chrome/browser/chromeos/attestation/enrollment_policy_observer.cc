// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/enrollment_policy_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/cert/pem_tokenizer.h"
#include "net/cert/x509_certificate.h"

namespace {

const int kRetryDelay = 5;  // Seconds.
const int kRetryLimit = 100;

// A dbus callback which handles a string result.
//
// Parameters
//   on_success - Called when result is successful and has a value.
//   on_failure - Called otherwise.
void DBusStringCallback(
    base::OnceCallback<void(const std::string&)> on_success,
    base::OnceClosure on_failure,
    const base::Location& from_here,
    base::Optional<chromeos::CryptohomeClient::TpmAttestationDataResult>
        result) {
  if (!result.has_value() || !result->success) {
    LOG(ERROR) << "Cryptohome DBus method failed: " << from_here.ToString();
    if (!on_failure.is_null())
      std::move(on_failure).Run();
    return;
  }
  std::move(on_success).Run(result->data);
}

}  // namespace

namespace chromeos {
namespace attestation {

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
    policy::CloudPolicyClient* policy_client)
    : device_settings_service_(DeviceSettingsService::Get()),
      policy_client_(policy_client),
      cryptohome_client_(nullptr),
      num_retries_(0),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  device_settings_service_->AddObserver(this);
  Start();
}

EnrollmentPolicyObserver::EnrollmentPolicyObserver(
    policy::CloudPolicyClient* policy_client,
    DeviceSettingsService* device_settings_service,
    CryptohomeClient* cryptohome_client)
    : device_settings_service_(device_settings_service),
      policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
      num_retries_(0),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
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
  // If we already uploaded an empty identification, we are done, because
  // we asked to compute and upload it only if the PCA refused to give
  // us an enrollment certificate, an error that will happen again (the
  // AIK certificate sent to request an enrollment certificate does not
  // contain an EID).
  if (did_upload_empty_eid_)
    return;

  // If identification for enrollment isn't needed, there is nothing to do.
  const enterprise_management::PolicyData* policy_data =
      device_settings_service_->policy_data();
  if (!policy_data || !policy_data->enrollment_id_needed())
    return;

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "EnrollmentPolicyObserver: Invalid CloudPolicyClient.";
    return;
  }

  // Do not allow multiple concurrent starts.
  if (request_in_flight_)
    return;
  request_in_flight_ = true;

  if (!cryptohome_client_)
    cryptohome_client_ = DBusThreadManager::Get()->GetCryptohomeClient();

  GetEnrollmentId();
}

void EnrollmentPolicyObserver::GetEnrollmentId() {
  cryptohome_client_->TpmAttestationGetEnrollmentId(
      true /* ignore_cache */,
      base::BindOnce(
          DBusStringCallback,
          base::BindOnce(&EnrollmentPolicyObserver::HandleEnrollmentId,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&EnrollmentPolicyObserver::RescheduleGetEnrollmentId,
                         weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void EnrollmentPolicyObserver::HandleEnrollmentId(
    const std::string& enrollment_id) {
  if (enrollment_id.empty()) {
    LOG(WARNING) << "EnrollmentPolicyObserver: The enrollment identifier"
                    " obtained is empty.";
  }
  policy_client_->UploadEnterpriseEnrollmentId(
      enrollment_id,
      base::BindRepeating(&EnrollmentPolicyObserver::OnUploadComplete,
                          weak_factory_.GetWeakPtr(), enrollment_id));
}

void EnrollmentPolicyObserver::RescheduleGetEnrollmentId() {
  if (++num_retries_ < retry_limit_) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
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
}  // namespace chromeos
