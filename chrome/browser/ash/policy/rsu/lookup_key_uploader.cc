// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/rsu/lookup_key_uploader.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/prefs/pref_service.h"

namespace policy {

const base::TimeDelta LookupKeyUploader::kRetryFrequency = base::Hours(10);

LookupKeyUploader::LookupKeyUploader(
    DeviceCloudPolicyStoreAsh* policy_store,
    PrefService* pref_service,
    ash::attestation::EnrollmentCertificateUploader* certificate_uploader)
    : policy_store_(policy_store),
      prefs_(pref_service),
      certificate_uploader_(certificate_uploader),
      cryptohome_misc_client_(ash::CryptohomeMiscClient::Get()),
      clock_(base::DefaultClock::GetInstance()) {
  // Can be null in tests.
  if (policy_store_)
    policy_store_->AddObserver(this);
}

LookupKeyUploader::~LookupKeyUploader() {
  // Can be null in tests.
  if (policy_store_)
    policy_store_->RemoveObserver(this);
}

void LookupKeyUploader::OnStoreLoaded(CloudPolicyStore* store) {
  const enterprise_management::PolicyData* policy_data = store->policy();
  if (clock_->Now() - last_upload_time_ < kRetryFrequency)
    return;
  if (policy_data && policy_data->has_client_action_required() &&
      policy_data->client_action_required().enrollment_certificate_needed()) {
    // We clear it in the case when this lookup key was uploaded earlier.
    prefs_->SetString(prefs::kLastRsuDeviceIdUploaded, std::string());
    needs_upload_ = true;
  }
  if (!needs_upload_)
    return;
  needs_upload_ = false;

  cryptohome_misc_client_->WaitForServiceToBeAvailable(base::BindOnce(
      &LookupKeyUploader::GetDataFromCryptohome, weak_factory_.GetWeakPtr()));
}

void LookupKeyUploader::GetDataFromCryptohome(bool available) {
  if (!available) {
    needs_upload_ = true;
    return;
  }
  cryptohome_misc_client_->GetRsuDeviceId(
      user_data_auth::GetRsuDeviceIdRequest(),
      base::BindOnce(&LookupKeyUploader::OnRsuDeviceIdReceived,
                     weak_factory_.GetWeakPtr()));
}

void LookupKeyUploader::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void LookupKeyUploader::OnRsuDeviceIdReceived(
    std::optional<user_data_auth::GetRsuDeviceIdReply> result) {
  if (!result.has_value()) {
    Result(std::string(), false /* success */);
    return;
  }

  if (result->rsu_device_id().empty()) {
    LOG(ERROR) << "Failed to extract RSU lookup key.";
    Result(std::string(), false /* success */);
    return;
  }
  const std::string rsu_device_id = result->rsu_device_id();

  // Making it printable so we can store it in prefs.
  std::string encoded_rsu_device_id = base::Base64Encode(rsu_device_id);

  // If this ID was uploaded previously -- we are not uploading it.
  if (rsu_device_id == prefs_->GetString(prefs::kLastRsuDeviceIdUploaded))
    return;
  certificate_uploader_->ObtainAndUploadCertificate(
      base::BindOnce(&LookupKeyUploader::OnEnrollmentCertificateUploaded,
                     weak_factory_.GetWeakPtr(), encoded_rsu_device_id));
}

void LookupKeyUploader::OnEnrollmentCertificateUploaded(
    const std::string& encoded_uploaded_key,
    ash::attestation::EnrollmentCertificateUploader::Status status) {
  const bool success =
      status ==
      ash::attestation::EnrollmentCertificateUploader::Status::kSuccess;
  Result(encoded_uploaded_key, success);
}

void LookupKeyUploader::Result(const std::string& encoded_uploaded_key,
                               bool success) {
  last_upload_time_ = clock_->Now();
  if (success) {
    prefs_->SetString(prefs::kLastRsuDeviceIdUploaded, encoded_uploaded_key);
    return;
  }
  // Reschedule upload on fail.
  needs_upload_ = true;
}

}  // namespace policy
