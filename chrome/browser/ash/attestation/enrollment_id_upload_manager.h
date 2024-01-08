// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_ID_UPLOAD_MANAGER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_ID_UPLOAD_MANAGER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace ash {
namespace attestation {

// A class which observes policy changes and triggers uploading identification
// for enrollment if necessary.
class EnrollmentIdUploadManager : public DeviceSettingsService::Observer {
 public:
  using UploadManagerCallback = base::OnceCallback<void(bool success)>;

  // The manager immediately connects with DeviceSettingsService to listen for
  // policy changes. The EnrollmentCertificateUploader is used to attempt to
  // upload enrollment certificate first. If it fails, the manager attempts to
  // upload enrollment ID. The CloudPolicyClient is used to upload enrollment ID
  // to the server; it must be in the registered state. This class does not take
  // ownership of |policy_client|.
  EnrollmentIdUploadManager(
      policy::CloudPolicyClient* policy_client,
      EnrollmentCertificateUploader* certificate_uploader);

  // A constructor which accepts custom instances useful for testing.
  EnrollmentIdUploadManager(
      policy::CloudPolicyClient* policy_client,
      DeviceSettingsService* device_settings_service,
      EnrollmentCertificateUploader* certificate_uploader);

  // Disallow copy and assign.
  EnrollmentIdUploadManager(const EnrollmentIdUploadManager&) = delete;
  EnrollmentIdUploadManager& operator=(const EnrollmentIdUploadManager&) =
      delete;

  ~EnrollmentIdUploadManager() override;

  // Obtains a fresh enrollment certificate, which contains enrollment ID, and
  // uploads it. If it fails, the manager will attempt to upload enrollment ID.
  // Calls the callback when the processing is complete, with `success` set to
  // `true` if either the enrollment certificate or the enrollment ID was
  // uploaded, or `false`, otherwise.
  void ObtainAndUploadEnrollmentId(UploadManagerCallback callback);

  // Sets the retry limit in number of tries; useful for testing.
  void set_retry_limit_for_testing(int limit) { retry_limit_ = limit; }

  // Sets the retry delay in seconds; useful for testing.
  void set_retry_delay_for_testing(int retry_delay) {
    retry_delay_ = retry_delay;
  }

 private:
  // Called when the device settings change.
  void DeviceSettingsUpdated() override;

  // Checks enrollment setting and starts any necessary work.
  void Start();

  // Handles certificate upload status. If succeeded or failed to upload - does
  // nothing more. If failed to fetch - starts computed enrollment ID flow.
  void OnEnrollmentCertificateUploaded(
      EnrollmentCertificateUploader::Status status);

  // Gets an enrollment identifier directly. Does nothing if |policy_client_| is
  // not registered, or if an empty ID has already been uploaded.
  void GetEnrollmentId();

  // Handles an enrollment identifier obtained directly.
  void OnGetEnrollmentId(const ::attestation::GetEnrollmentIdReply& reply);

  // Reschedule an attempt to get an enrollment identifier directly.
  void RescheduleGetEnrollmentId();

  // Called when an enrollment identifier upload operation completes.
  // On success, |result| will be true. The string |enrollment_id| contains
  // the enrollment identifier that was uploaded.
  void OnUploadComplete(const std::string& enrollment_id,
                        policy::CloudPolicyClient::Result result);

  // Run all callbacks with |status|.
  void RunCallbacks(bool status);

  const raw_ptr<DeviceSettingsService> device_settings_service_;
  const raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> policy_client_;
  const raw_ptr<EnrollmentCertificateUploader> certificate_uploader_;
  int num_retries_;
  int retry_limit_;
  int retry_delay_;

  // Used to remember we uploaded an empty identifier this session for
  // devices that can't obtain the identifier until they are powerwashed or
  // updated and rebooted (see http://crbug.com/867724).
  bool did_upload_empty_eid_ = false;

  // Callbacks for the enrollment ID upload that is in progress.
  std::queue<UploadManagerCallback> upload_manager_callbacks_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EnrollmentIdUploadManager> weak_factory_{this};

  friend class EnrollmentIdUploadManagerTest;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_ID_UPLOAD_MANAGER_H_
