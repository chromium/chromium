// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class CryptohomeClient;

namespace attestation {

// A class which observes policy changes and triggers uploading identification
// for enrollment if necessary.
class EnrollmentPolicyObserver : public DeviceSettingsService::Observer {
 public:
  // The observer immediately connects with DeviceSettingsService to listen for
  // policy changes.  The CloudPolicyClient is used to upload data to the
  // server; it must be in the registered state.  This class does not take
  // ownership of |policy_client|.
  explicit EnrollmentPolicyObserver(policy::CloudPolicyClient* policy_client);

  // A constructor which accepts custom instances useful for testing.
  EnrollmentPolicyObserver(policy::CloudPolicyClient* policy_client,
                           DeviceSettingsService* device_settings_service,
                           CryptohomeClient* cryptohome_client);

  ~EnrollmentPolicyObserver() override;

  // Sets the retry limit in number of tries; useful in testing.
  void set_retry_limit(int limit) { retry_limit_ = limit; }
  // Sets the retry delay in seconds; useful in testing.
  void set_retry_delay(int retry_delay) { retry_delay_ = retry_delay; }

 private:
  // Called when the device settings change.
  void DeviceSettingsUpdated() override;

  // Checks enrollment setting and starts any necessary work.
  void Start();

  // Gets an enrollment identifier directly.
  void GetEnrollmentId();

  // Handles an enrollment identifer obtained directly.
  void HandleEnrollmentId(const std::string& enrollment_id);

  // Reschedule an attempt to get an enrollment identifier directly.
  void RescheduleGetEnrollmentId();

  // Called when an enrollment identifier upload operation completes.
  // On success, |status| will be true. The string |enrollment_id| contains
  // the enrollment identifier that was uploaded.
  void OnUploadComplete(const std::string& enrollment_id, bool status);

  DeviceSettingsService* device_settings_service_;
  policy::CloudPolicyClient* policy_client_;
  CryptohomeClient* cryptohome_client_;
  int num_retries_;
  int retry_limit_;
  int retry_delay_;
  // Whether we are requesting an EID right now.
  bool request_in_flight_ = false;
  // Used to remember we uploaded an empty identifier this session for
  // devices that can't obtain the identifier until they are powerwashed or
  // updated and rebooted (see http://crbug.com/867724).
  bool did_upload_empty_eid_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EnrollmentPolicyObserver> weak_factory_{this};

  friend class EnrollmentPolicyObserverTest;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentPolicyObserver);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_POLICY_OBSERVER_H_
