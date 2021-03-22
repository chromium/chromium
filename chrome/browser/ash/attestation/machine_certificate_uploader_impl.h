// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"
// TODO(https://crbug.com/1164001): forward declare AttestatoinFlow
// after //chromeos/attestation is moved to ash.
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace ash {
namespace attestation {

// A class which uploads enterprise machine certificates.
class MachineCertificateUploaderImpl : public MachineCertificateUploader {
 public:
  explicit MachineCertificateUploaderImpl(
      policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom AttestationFlow implementations. Useful
  // for testing.
  MachineCertificateUploaderImpl(policy::CloudPolicyClient* policy_client,
                                 AttestationFlow* attestation_flow);

  ~MachineCertificateUploaderImpl() override;

  // Sets the retry limit in number of tries; useful in testing.
  void set_retry_limit(int limit) { retry_limit_ = limit; }
  // Sets the retry delay in seconds; useful in testing.
  void set_retry_delay(int retry_delay) { retry_delay_ = retry_delay; }

  using UploadCallback =
      base::OnceCallback<void(bool /*certificate_uploaded*/)>;

  // Checks if the machine certificate has been uploaded, and if not, do so.
  // A certificate will be obtained if needed.
  void UploadCertificateIfNeeded(UploadCallback callback) override;

  // Refreshes a fresh machine certificate and uploads it.
  void RefreshAndUploadCertificate(UploadCallback callback) override;

  // Non-blocking wait for a certificate to be uploaded. Calls the |callback|
  // immediately if the certificate was already uploaded or wait for the next
  // attempt to do so.
  void WaitForUploadComplete(UploadCallback callback) override;

 private:
  // Starts certificate obtention and upload.
  void Start();

  // Gets a new certificate for the Enterprise Machine Key (EMK).
  void GetNewCertificate();

  // Called when `GetKeyInfo()` returned to check the existing certificate.
  // There are 3 cases by the status replied from attestation service:
  //     1. If the existing EMK is found, calls `CheckCertificateExpiry()`.
  //     2. If the key does not exist, calls `GetNewCertificate()`.
  //     3. Otherwise, there is an error and `Reschedule()` is called to retry.
  void OnGetExistingCertificate(const ::attestation::GetKeyInfoReply& reply);

  // Checks if any certificate in the given `reply` is expired and, if so, gets
  // a new one. If not renewing, calls `CheckIfUploaded()`.
  void CheckCertificateExpiry(const ::attestation::GetKeyInfoReply& reply);

  // Uploads a machine certificate to the policy server.
  void UploadCertificate(const std::string& pem_certificate_chain);

  // Checks if a certificate in `reply` has already been uploaded and, if not,
  // upload.
  void CheckIfUploaded(const ::attestation::GetKeyInfoReply& reply);

  // Called when a certificate upload operation completes.  On success, |status|
  // will be true.
  void OnUploadComplete(bool status);

  // Marks a key as uploaded in the payload proto.
  void MarkAsUploaded(const ::attestation::GetKeyInfoReply& reply);

  // Handles failure of getting a certificate.
  void HandleGetCertificateFailure(AttestationStatus status);

  // Reschedules a policy check (i.e. a call to Start) for a later time.
  // TODO(dkrahn): A better solution would be to wait for a dbus signal which
  // indicates the system is ready to process this task. See crbug.com/256845.
  void Reschedule();

  void RunCallbacks(bool status);

  policy::CloudPolicyClient* policy_client_ = nullptr;
  AttestationFlow* attestation_flow_ = nullptr;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  bool refresh_certificate_ = false;
  std::vector<UploadCallback> callbacks_;
  int num_retries_ = {};
  int retry_limit_ = {};
  int retry_delay_ = {};
  base::Optional<bool> certificate_uploaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MachineCertificateUploaderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MachineCertificateUploaderImpl);
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
