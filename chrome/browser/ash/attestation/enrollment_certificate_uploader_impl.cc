// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/attestation/attestation_flow_adaptive.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Constants for retrying certificate obtention and upload.
constexpr base::TimeDelta kRetryDelay = base::Seconds(5);
const int kRetryLimit = 100;

void DBusPrivacyCACallback(
    const base::RepeatingCallback<void(const std::string&)> on_success,
    const base::RepeatingCallback<
        void(chromeos::attestation::AttestationStatus)> on_failure,
    const base::Location& from_here,
    chromeos::attestation::AttestationStatus status,
    const std::string& data) {
  DCHECK(on_success);
  DCHECK(on_failure);

  if (status == chromeos::attestation::ATTESTATION_SUCCESS) {
    on_success.Run(data);
    return;
  }

  LOG(ERROR) << "Attestation DBus method or server called failed with status: "
             << status << ": " << from_here.ToString();
  on_failure.Run(status);
}

}  // namespace

namespace ash {
namespace attestation {

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client)
    : EnrollmentCertificateUploaderImpl(policy_client,
                                        nullptr /* attestation_flow */) {}

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client,
    AttestationFlow* attestation_flow)
    : policy_client_(policy_client),
      attestation_flow_(attestation_flow),
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
  if (start)
    Start();
}

void EnrollmentCertificateUploaderImpl::Start() {
  num_retries_ = 0;

  if (has_already_uploaded_) {
    // Certificate was successfully uploaded earlier. Do not upload second time.
    RunCallbacks(Status::kSuccess);
    return;
  }

  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_ = std::make_unique<AttestationFlowAdaptive>(
        std::move(attestation_ca_client));
    attestation_flow_ = default_attestation_flow_.get();
  }

  GetCertificate(EnrollmentCertificateRequest::kExistingCertificate);
}

void EnrollmentCertificateUploaderImpl::RunCallbacks(Status status) {
  for (; !callbacks_.empty(); callbacks_.pop())
    std::move(callbacks_.front()).Run(status);
}

void EnrollmentCertificateUploaderImpl::GetCertificate(
    EnrollmentCertificateRequest certificate_request) {
  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "CloudPolicyClient not registered.";
    RunCallbacks(Status::kFailedToFetch);
    return;
  }

  bool force_new_key = false;
  switch (certificate_request) {
    case EnrollmentCertificateRequest::kExistingCertificate:
      force_new_key = false;
      break;
    case EnrollmentCertificateRequest::kNewCertificate:
      force_new_key = true;
      break;
  }

  VLOG_IF(1, force_new_key) << "Fetching new certificate";
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      force_new_key,
      std::string(),  // Leave key name empty to generate a default name.
      base::BindOnce(
          [](const base::RepeatingCallback<void(const std::string&)> on_success,
             const base::RepeatingCallback<void(AttestationStatus)> on_failure,
             const base::Location& from_here, AttestationStatus status,
             const std::string& data) {
            DBusPrivacyCACallback(on_success, on_failure, from_here, status,
                                  std::move(data));
          },
          base::BindRepeating(
              &EnrollmentCertificateUploaderImpl::UploadCertificate,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure,
              weak_factory_.GetWeakPtr(), certificate_request),
          FROM_HERE));
}

void EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure(
    EnrollmentCertificateRequest certificate_request,
    AttestationStatus status) {
  if (status == ATTESTATION_SERVER_BAD_REQUEST_FAILURE) {
    LOG(ERROR)
        << "Failed to fetch Enterprise Enrollment Certificate: bad request.";
    RunCallbacks(Status::kFailedToFetch);
    return;
  }

  LOG(WARNING) << "Failed to fetch Enterprise Enrollment Certificate.";

  if (!Reschedule(certificate_request)) {
    RunCallbacks(Status::kFailedToFetch);
  }
}

void EnrollmentCertificateUploaderImpl::UploadCertificate(
    const std::string& pem_certificate_chain) {
  policy_client_->UploadEnterpriseEnrollmentCertificate(
      pem_certificate_chain,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void EnrollmentCertificateUploaderImpl::OnUploadComplete(bool status) {
  if (status) {
    has_already_uploaded_ = true;
    VLOG(1) << "Enterprise Enrollment Certificate uploaded to DMServer.";
    RunCallbacks(Status::kSuccess);
    return;
  }

  LOG(WARNING)
      << "Failed to upload Enterprise Enrollment Certificate to DMServer.";

  if (!Reschedule(EnrollmentCertificateRequest::kNewCertificate)) {
    RunCallbacks(Status::kFailedToUpload);
  }
}

bool EnrollmentCertificateUploaderImpl::Reschedule(
    EnrollmentCertificateRequest certificate_request) {
  if (num_retries_ >= retry_limit_) {
    LOG(ERROR) << "Retry limit exceeded to fetch enrollment certificate.";
    return false;
  }

  ++num_retries_;

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::GetCertificate,
                     weak_factory_.GetWeakPtr(), certificate_request),
      retry_delay_);
  return true;
}

}  // namespace attestation
}  // namespace ash
