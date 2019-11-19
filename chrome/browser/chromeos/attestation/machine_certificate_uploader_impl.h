// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/attestation/machine_certificate_uploader.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace policy {
class CloudPolicyClient;
}

namespace chromeos {

class CryptohomeClient;

namespace attestation {

class AttestationFlow;

// A class which uploads enterprise machine certificates.
class MachineCertificateUploaderImpl : public MachineCertificateUploader {
 public:
  explicit MachineCertificateUploaderImpl(
      policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom CryptohomeClient and AttestationFlow
  // implementations.  Useful for testing.
  MachineCertificateUploaderImpl(policy::CloudPolicyClient* policy_client,
                                 CryptohomeClient* cryptohome_client,
                                 AttestationFlow* attestation_flow);

  ~MachineCertificateUploaderImpl() override;

  // Sets the retry limit in number of tries; useful in testing.
  void set_retry_limit(int limit) { retry_limit_ = limit; }
  // Sets the retry delay in seconds; useful in testing.
  void set_retry_delay(int retry_delay) { retry_delay_ = retry_delay; }

  using UploadCallback = base::OnceCallback<void(bool)>;

  // Checks if the machine certificate has been uploaded, and if not, do so.
  // A certificate will be obtained if needed.
  void UploadCertificateIfNeeded(UploadCallback callback) override;

  // Refreshs a fresh machine certificate and uploads it.
  void RefreshAndUploadCertificate(UploadCallback callback) override;

 private:
  // Starts certificate obtention and upload.
  void Start();

  // Gets a new certificate for the Enterprise Machine Key (EMK).
  void GetNewCertificate();

  // Gets the existing EMK certificate and sends it to CheckCertificateExpiry.
  void GetExistingCertificate();

  // Checks if any certificate in the given pem_certificate_chain is expired
  // and, if so, gets a new one. If not renewing, calls CheckIfUploaded.
  void CheckCertificateExpiry(const std::string& pem_certificate_chain);

  // Uploads a machine certificate to the policy server.
  void UploadCertificate(const std::string& pem_certificate_chain);

  // Checks if a certificate has already been uploaded and, if not, upload.
  void CheckIfUploaded(const std::string& pem_certificate_chain,
                       const std::string& key_payload);

  // Gets the payload associated with the EMK and sends it to |callback|,
  // or call |on_failure| with no arguments if the payload cannot be obtained.
  void GetKeyPayload(base::RepeatingCallback<void(const std::string&)> callback,
                     base::RepeatingCallback<void()> on_failure);

  // Called when a certificate upload operation completes.  On success, |status|
  // will be true.
  void OnUploadComplete(bool status);

  // Marks a key as uploaded in the payload proto.
  void MarkAsUploaded(const std::string& key_payload);

  // Handles failure of getting a certificate.
  void HandleGetCertificateFailure(AttestationStatus status);

  // Reschedules a policy check (i.e. a call to Start) for a later time.
  // TODO(dkrahn): A better solution would be to wait for a dbus signal which
  // indicates the system is ready to process this task. See crbug.com/256845.
  void Reschedule();

  policy::CloudPolicyClient* policy_client_;
  CryptohomeClient* cryptohome_client_;
  AttestationFlow* attestation_flow_;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  bool refresh_certificate_;
  UploadCallback callback_;
  int num_retries_;
  int retry_limit_;
  int retry_delay_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MachineCertificateUploaderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MachineCertificateUploaderImpl);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
