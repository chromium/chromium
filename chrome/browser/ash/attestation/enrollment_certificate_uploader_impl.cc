// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash::attestation {

namespace {

// Constants for retrying certificate obtention and upload.
constexpr base::TimeDelta kRetryDelay = base::Seconds(5);
const int kRetryLimit = 100;

void DBusPrivacyCACallback(
    const base::RepeatingCallback<void(const std::string&)> on_success,
    const base::RepeatingCallback<void(AttestationStatus)> on_failure,
    const base::Location& from_here,
    AttestationStatus status,
    const std::string& data) {
  DCHECK(on_success);
  DCHECK(on_failure);

  if (status == ATTESTATION_SUCCESS) {
    on_success.Run(data);
    return;
  }

  LOG(ERROR) << "Attestation DBus method or server called failed with status: "
             << status << ": " << from_here.ToString();
  on_failure.Run(status);
}

}  // namespace

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client)
    : policy_client_(policy_client),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

EnrollmentCertificateUploaderImpl::~EnrollmentCertificateUploaderImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void EnrollmentCertificateUploaderImpl::ObtainAndUploadCertificate(
    UploadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool start = callbacks_.empty();
  callbacks_.push(std::move(callback));
  if (start) {
    num_retries_ = 0;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&EnrollmentCertificateUploaderImpl::Start,
                                  weak_factory_.GetWeakPtr()));
  }
}

void EnrollmentCertificateUploaderImpl::Start() {
  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_ = std::make_unique<AttestationFlowAdaptive>(
        std::move(attestation_ca_client));
    attestation_flow_ = default_attestation_flow_.get();
  }

  GetCertificate();
}

void EnrollmentCertificateUploaderImpl::GetCertificate() {
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "CloudPolicyClient not registered.";
    RunCallbacks(Status::kInvalidClient);
    return;
  }
  auto callback = base::BindOnce(
      [](base::RepeatingCallback<void(const std::string&)> on_success,
         base::RepeatingCallback<void(AttestationStatus)> on_failure,
         const base::Location& from_here, AttestationStatus status,
         const std::string& data) {
        DBusPrivacyCACallback(on_success, on_failure, from_here, status, data);
      },
      base::BindRepeating(
          &EnrollmentCertificateUploaderImpl::UploadCertificateIfNeeded,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure,
          weak_factory_.GetWeakPtr()),
      FROM_HERE);
  AttestationFeatures::GetFeatures(
      base::BindOnce(&EnrollmentCertificateUploaderImpl::OnGetFeaturesReady,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void EnrollmentCertificateUploaderImpl::OnGetFeaturesReady(
    AttestationFlow::CertificateCallback callback,
    const AttestationFeatures* features) {
  if (!features) {
    LOG(ERROR) << "Failed to get AttestationFeatures.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }
  if (!features->IsAttestationAvailable()) {
    LOG(ERROR) << "The Attestation is not available.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // prefers ECC certificate if available
  ::attestation::KeyType key_crypto_type;
  if (features->IsEccSupported()) {
    key_crypto_type = ::attestation::KEY_TYPE_ECC;
  } else if (features->IsRsaSupported()) {
    key_crypto_type = ::attestation::KEY_TYPE_RSA;
  } else {
    LOG(ERROR) << "No appropriate crypto key type supported.";
    std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
    return;
  }

  // Always force a new key to obtain a fresh certificate.
  // Expired certificates are rejected by the server. It is easier to force
  // the certificate refresh rather than ensure certificate expiry status, since
  // the certificate upload is not expected to happen too often. See b/163817801
  // and b/216220722 for the context.
  attestation_flow_->GetCertificate(
      /*certificate_profile=*/PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      /*account_id=*/EmptyAccountId(),   // Not used.
      /*request_origin=*/std::string(),  // Not used.
      /*force_new_key=*/true, key_crypto_type,
      /*key_name=*/kEnterpriseEnrollmentKey,
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));
}

void EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure(
    AttestationStatus status) {
  if (status == ATTESTATION_SERVER_BAD_REQUEST_FAILURE) {
    LOG(ERROR)
        << "Failed to fetch Enterprise Enrollment Certificate: bad request.";
    RunCallbacks(Status::kFailedToFetch);
    return;
  }

  LOG(WARNING) << "Failed to fetch Enterprise Enrollment Certificate.";

  if (!Reschedule()) {
    RunCallbacks(Status::kFailedToFetch);
  }
}

void EnrollmentCertificateUploaderImpl::UploadCertificateIfNeeded(
    const std::string& pem_certificate_chain) {
  if (has_already_uploaded_) {
    RunCallbacks(Status::kSuccess);
    return;
  }

  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "CloudPolicyClient not registered.";
    RunCallbacks(Status::kInvalidClient);
    return;
  }

  policy_client_->UploadEnterpriseEnrollmentCertificate(
      pem_certificate_chain,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void EnrollmentCertificateUploaderImpl::OnUploadComplete(
    policy::CloudPolicyClient::Result result) {
  if (result.IsSuccess()) {
    has_already_uploaded_ = true;
    if (num_retries_ != 0) {
      LOG(WARNING) << "Enterprise Enrollment Certificate uploaded to DMServer "
                      "after retries: "
                   << num_retries_;
    } else {
      VLOG(1) << "Enterprise Enrollment Certificate uploaded to DMServer.";
    }
    RunCallbacks(Status::kSuccess);
    return;
  }

  LOG(WARNING)
      << "Failed to upload Enterprise Enrollment Certificate to DMServer.";

  if (!Reschedule()) {
    RunCallbacks(Status::kFailedToUpload);
  }
}

bool EnrollmentCertificateUploaderImpl::Reschedule() {
  if (num_retries_ >= retry_limit_) {
    LOG(ERROR) << "Retry limit exceeded to fetch enrollment certificate.";
    return false;
  }

  ++num_retries_;

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::Start,
                     weak_factory_.GetWeakPtr()),
      retry_delay_);
  return true;
}

void EnrollmentCertificateUploaderImpl::RunCallbacks(Status status) {
  std::queue<UploadCallback> callbacks;
  callbacks.swap(callbacks_);
  for (; !callbacks.empty(); callbacks.pop())
    std::move(callbacks.front()).Run(status);
}

}  // namespace ash::attestation
