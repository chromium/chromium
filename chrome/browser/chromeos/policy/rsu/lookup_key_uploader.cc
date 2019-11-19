// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/rsu/lookup_key_uploader.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/prefs/pref_service.h"

namespace policy {

const base::TimeDelta LookupKeyUploader::kRetryFrequency =
    base::TimeDelta::FromHours(10);

LookupKeyUploader::LookupKeyUploader(
    DeviceCloudPolicyStoreChromeOS* policy_store,
    PrefService* pref_service,
    chromeos::attestation::EnrollmentCertificateUploader* certificate_uploader)
    : policy_store_(policy_store),
      prefs_(pref_service),
      certificate_uploader_(certificate_uploader),
      cryptohome_client_(chromeos::CryptohomeClient::Get()),
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

  cryptohome_client_->WaitForServiceToBeAvailable(base::BindOnce(
      &LookupKeyUploader::GetDataFromCryptohome, weak_factory_.GetWeakPtr()));
}

void LookupKeyUploader::GetDataFromCryptohome(bool available) {
  if (!available) {
    needs_upload_ = true;
    return;
  }
  cryptohome_client_->GetRsuDeviceId(base::BindOnce(
      &LookupKeyUploader::OnRsuDeviceIdReceived, weak_factory_.GetWeakPtr()));
}

void LookupKeyUploader::OnStoreError(CloudPolicyStore* store) {
  // Do nothing.
}

void LookupKeyUploader::OnRsuDeviceIdReceived(
    base::Optional<cryptohome::BaseReply> result) {
  if (!result.has_value()) {
    Result(std::string(), false /* success */);
    return;
  }

  if (!result->HasExtension(cryptohome::GetRsuDeviceIdReply::reply) ||
      !result->GetExtension(cryptohome::GetRsuDeviceIdReply::reply)
           .has_rsu_device_id() ||
      result->GetExtension(cryptohome::GetRsuDeviceIdReply::reply)
          .rsu_device_id()
          .empty()) {
    LOG(ERROR) << "Failed to extract RSU lookup key.";
    Result(std::string(), false /* success */);
    return;
  }
  const std::string rsu_device_id =
      result->GetExtension(cryptohome::GetRsuDeviceIdReply::reply)
          .rsu_device_id();

  // Making it printable so we can store it in prefs.
  std::string encoded_rsu_device_id;
  base::Base64Encode(rsu_device_id, &encoded_rsu_device_id);

  // If this ID was uploaded previously -- we are not uploading it.
  if (rsu_device_id == prefs_->GetString(prefs::kLastRsuDeviceIdUploaded))
    return;
  certificate_uploader_->ObtainAndUploadCertificate(
      base::BindOnce(&LookupKeyUploader::Result, weak_factory_.GetWeakPtr(),
                     encoded_rsu_device_id));
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
