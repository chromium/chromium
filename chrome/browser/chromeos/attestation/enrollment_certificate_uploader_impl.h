// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class CryptohomeClient;

namespace attestation {

class AttestationFlow;

// A class which uploads enterprise enrollment certificates.
class EnrollmentCertificateUploaderImpl : public EnrollmentCertificateUploader {
 public:
  explicit EnrollmentCertificateUploaderImpl(
      policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom CryptohomeClient and AttestationFlow
  // implementations. Useful for testing.
  EnrollmentCertificateUploaderImpl(policy::CloudPolicyClient* policy_client,
                                    CryptohomeClient* cryptohome_client,
                                    AttestationFlow* attestation_flow);

  ~EnrollmentCertificateUploaderImpl() override;

  // Sets the retry limit in number of tries; useful in testing.
  void set_retry_limit(int limit) { retry_limit_ = limit; }
  // Sets the retry delay; useful in testing.
  void set_retry_delay(base::TimeDelta retry_delay) {
    retry_delay_ = retry_delay;
  }

  // Obtains a fresh enrollment certificate and uploads it.
  void ObtainAndUploadCertificate(UploadCallback callback) override;

 private:
  // Starts certificate obtention and upload.
  void Start();

  // Run all callbacks with |status|.
  void RunCallbacks(bool status);

  // Gets a certificate.
  void GetCertificate();

  // Called when a certificate upload operation completes. On success, |status|
  // will be true.
  void OnUploadComplete(bool status);

  // Uploads an enterprise certificate to the policy server.
  void UploadCertificate(const std::string& pem_certificate_chain);

  // Handles failure of getting a certificate.
  void HandleGetCertificateFailure(AttestationStatus status);

  // Reschedules a policy check (i.e. a call to Start) for a later time.
  // TODO(crbug.com/256845): A better solution would be to wait for a dbus
  // signal which indicates the system is ready to process this task.
  void Reschedule();

  policy::CloudPolicyClient* policy_client_;
  CryptohomeClient* cryptohome_client_;
  AttestationFlow* attestation_flow_;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  // Callbacks to run when a certificate is uploaded (or we fail to).
  std::queue<UploadCallback> callbacks_;
  // Values for retries.
  int num_retries_;
  int retry_limit_;
  base::TimeDelta retry_delay_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EnrollmentCertificateUploaderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EnrollmentCertificateUploaderImpl);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_
