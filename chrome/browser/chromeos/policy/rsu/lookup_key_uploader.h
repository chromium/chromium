// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefService;

namespace chromeos {
namespace attestation {
class EnrollmentCertificateUploader;
}
class CryptohomeClient;
}  // namespace chromeos

namespace cryptohome {
class BaseReply;
}

namespace policy {

class DeviceCloudPolicyStoreChromeOS;

// This class is used for uploading Remote Server Unlock lookup keys once per
// enrollment, attempting it whenever we receive device policy from the cloud.
class LookupKeyUploader : public CloudPolicyStore::Observer {
 public:
  // The observer immediately connects with DeviceCloudPolicyStoreChromeOS
  // to listen for policy load events.
  LookupKeyUploader(DeviceCloudPolicyStoreChromeOS* policy_store,
                    PrefService* pref_service,
                    chromeos::attestation::EnrollmentCertificateUploader*
                        certificate_uploader);

  ~LookupKeyUploader() override;

 private:
  // Minimum period of time between consecutive uploads.
  static const base::TimeDelta kRetryFrequency;

  friend class LookupKeyUploaderTest;
  // Observers from ChromePolicy store.
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  void Start();
  void GetDataFromCryptohome(bool available);
  void OnRsuDeviceIdReceived(base::Optional<cryptohome::BaseReply> result);
  void HandleRsuDeviceId(const std::string& rsu_device_id);

  void Result(const std::string& uploaded_key, bool success);
  // Used in tests.
  void SetClock(base::Clock* clock) { clock_ = clock; }

  DeviceCloudPolicyStoreChromeOS* policy_store_;
  PrefService* prefs_;
  chromeos::attestation::EnrollmentCertificateUploader* certificate_uploader_;
  chromeos::CryptohomeClient* cryptohome_client_;

  // Whether we need to upload the lookup key right now. By default, it is set
  // to true. Later, it is set to false after first successful upload or finding
  // prefs::kLastRSULookupKeyUploaded to be equal to the current lookup key.
  bool needs_upload_ = true;

  base::Clock* clock_;
  // Timestamp of the last lookup key upload, used for resrticting too frequent
  // usage.
  base::Time last_upload_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<LookupKeyUploader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LookupKeyUploader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_RSU_LOOKUP_KEY_UPLOADER_H_
