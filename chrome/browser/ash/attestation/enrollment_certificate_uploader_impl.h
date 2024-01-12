// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_
#define CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_

#include <memory>
#include <queue>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/enrollment_certificate_uploader.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace ash {
namespace attestation {

class AttestationFeatures;

// A class which uploads enterprise enrollment certificates.
class EnrollmentCertificateUploaderImpl : public EnrollmentCertificateUploader {
 public:
  explicit EnrollmentCertificateUploaderImpl(
      policy::CloudPolicyClient* policy_client);

  EnrollmentCertificateUploaderImpl(const EnrollmentCertificateUploaderImpl&) =
      delete;
  EnrollmentCertificateUploaderImpl& operator=(
      const EnrollmentCertificateUploaderImpl&) = delete;

  ~EnrollmentCertificateUploaderImpl() override;

  // Sets the retry limit in number of tries; useful for testing.
  void set_retry_limit_for_testing(int limit) { retry_limit_ = limit; }

  // Sets the retry delay; useful for testing.
  void set_retry_delay_for_testing(base::TimeDelta retry_delay) {
    retry_delay_ = retry_delay;
  }

  // Sets a custom AttestationFlow implementation; useful for testing.
  void set_attestation_flow_for_testing(AttestationFlow* attestation_flow) {
    attestation_flow_ = attestation_flow;
  }

  // Obtains a fresh enrollment certificate and uploads it. If certificate has
  // already been uploaded - reports success immediately and does not upload
  // second time.
  // If fails to fetch existing certificate, retries to fetch until success or
  // |retry_limit_|.
  // If fails to upload existing certificate, retries to fetch and upload a new
  // one until success or |retry_limit_|.
  void ObtainAndUploadCertificate(UploadCallback callback) override;

 private:
  // Starts certificate obtention and upload.
  void Start();

  // Gets a fresh certificate.
  void GetCertificate();

  // Checks the Attestation Features and gets a fresh certificate.
  void OnGetFeaturesReady(
      AttestationFlow::CertificateCallback callback,
      const ash::attestation::AttestationFeatures* features);

  // Handles failure of getting a certificate.
  void HandleGetCertificateFailure(AttestationStatus status);

  // Uploads an enterprise certificate to the policy server if it has not been
  // already uploaded. If it was uploaded - reports success without further
  // upload.
  void UploadCertificateIfNeeded(const std::string& pem_certificate_chain);

  // Called when a certificate upload operation completes.
  void OnUploadComplete(policy::CloudPolicyClient::Result result);

  // Reschedules certificate upload from |Start()| checkpoint and returns true.
  // If |retry_limit_| is exceeded, does not reschedule and returns false.
  // TODO(crbug.com/256845): A better solution would be to wait for a dbus
  // signal which indicates the system is ready to process this task.
  bool Reschedule();

  // Run all callbacks with |status|.
  void RunCallbacks(Status status);

  raw_ptr<policy::CloudPolicyClient, DanglingUntriaged> policy_client_;
  raw_ptr<AttestationFlow> attestation_flow_ = nullptr;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  // Callbacks to run when a certificate is uploaded (or we fail to).
  std::queue<UploadCallback> callbacks_;
  // Values for retries.
  int num_retries_;
  int retry_limit_;
  base::TimeDelta retry_delay_;

  // Indicates whether certificate has already been uploaded successfully. False
  // when no certificate is uploaded or the current certificate has expired.
  bool has_already_uploaded_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<EnrollmentCertificateUploaderImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_IMPL_H_
