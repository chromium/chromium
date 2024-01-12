// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefService;

namespace ash {
class CryptohomeMiscClient;
}

namespace policy {

class DeviceCloudPolicyStoreAsh;

// This class is used for uploading Remote Server Unlock lookup keys once per
// enrollment, attempting it whenever we receive device policy from the cloud.
class LookupKeyUploader : public CloudPolicyStore::Observer {
 public:
  // The observer immediately connects with DeviceCloudPolicyStoreAsh
  // to listen for policy load events.
  LookupKeyUploader(
      DeviceCloudPolicyStoreAsh* policy_store,
      PrefService* pref_service,
      ash::attestation::EnrollmentCertificateUploader* certificate_uploader);

  LookupKeyUploader(const LookupKeyUploader&) = delete;
  LookupKeyUploader& operator=(const LookupKeyUploader&) = delete;

  ~LookupKeyUploader() override;

 private:
  // Minimum period of time between consecutive uploads.
  static const base::TimeDelta kRetryFrequency;

  friend class LookupKeyUploaderTest;
  // Observers from ChromePolicy store.
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  void GetDataFromCryptohome(bool available);
  void OnRsuDeviceIdReceived(
      std::optional<user_data_auth::GetRsuDeviceIdReply> result);

  void OnEnrollmentCertificateUploaded(
      const std::string& uploaded_key,
      ash::attestation::EnrollmentCertificateUploader::Status status);

  void Result(const std::string& uploaded_key, bool success);
  // Used in tests.
  void SetClock(base::Clock* clock) { clock_ = clock; }

  raw_ptr<DeviceCloudPolicyStoreAsh> policy_store_;
  raw_ptr<PrefService, DanglingUntriaged> prefs_;
  raw_ptr<ash::attestation::EnrollmentCertificateUploader>
      certificate_uploader_;
  raw_ptr<ash::CryptohomeMiscClient, DanglingUntriaged> cryptohome_misc_client_;

  // Whether we need to upload the lookup key right now. By default, it is set
  // to true. Later, it is set to false after first successful upload or finding
  // prefs::kLastRSULookupKeyUploaded to be equal to the current lookup key.
  bool needs_upload_ = true;

  raw_ptr<base::Clock> clock_;
  // Timestamp of the last lookup key upload, used for resrticting too frequent
  // usage.
  base::Time last_upload_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<LookupKeyUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_
