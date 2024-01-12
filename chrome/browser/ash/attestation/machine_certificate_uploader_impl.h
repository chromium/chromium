// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace ash {
namespace attestation {

class AttestationFlow;

// A class which uploads enterprise machine certificates.
class MachineCertificateUploaderImpl : public MachineCertificateUploader {
 public:
  explicit MachineCertificateUploaderImpl(
      policy::CloudPolicyClient* policy_client);

  // A constructor which allows custom AttestationFlow implementations. Useful
  // for testing.
  MachineCertificateUploaderImpl(policy::CloudPolicyClient* policy_client,
                                 AttestationFlow* attestation_flow);

  MachineCertificateUploaderImpl(const MachineCertificateUploaderImpl&) =
      delete;
  MachineCertificateUploaderImpl& operator=(
      const MachineCertificateUploaderImpl&) = delete;

  ~MachineCertificateUploaderImpl() override;

  // Sets the retry limit in number of tries; useful in testing.
  void set_retry_limit_for_testing(int limit) { retry_limit_ = limit; }
  // Sets the retry delay; useful in testing.
  void set_retry_delay_for_testing(base::TimeDelta retry_delay) {
    retry_delay_ = std::move(retry_delay);
  }

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

  // Called when a certificate upload operation completes.
  void OnUploadComplete(policy::CloudPolicyClient::Result result);

  // Marks a key as uploaded in the payload proto.
  void MarkAsUploaded(const ::attestation::GetKeyInfoReply& reply);

  // Handles failure of getting a certificate.
  void HandleGetCertificateFailure(AttestationStatus status);

  // Reschedules a policy check (i.e. a call to Start) for a later time.
  // TODO(dkrahn): A better solution would be to wait for a dbus signal which
  // indicates the system is ready to process this task. See crbug.com/256845.
  void Reschedule();

  void RunCallbacks(bool status);

  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> policy_client_ =
      nullptr;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  raw_ptr<AttestationFlow> attestation_flow_ = nullptr;
  bool refresh_certificate_ = false;
  std::vector<UploadCallback> callbacks_;
  int num_retries_ = 0;
  int retry_limit_ = 0;
  base::TimeDelta retry_delay_;
  std::optional<bool> certificate_uploaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MachineCertificateUploaderImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_IMPL_H_
