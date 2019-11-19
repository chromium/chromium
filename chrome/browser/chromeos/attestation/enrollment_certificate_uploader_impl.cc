// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Constants for retrying certificate obtention and upload.
constexpr base::TimeDelta kRetryDelay = base::TimeDelta::FromSeconds(5);
const int kRetryLimit = 100;

void DBusPrivacyCACallback(
    const base::RepeatingCallback<void(const std::string&)> on_success,
    const base::RepeatingCallback<
        void(chromeos::attestation::AttestationStatus)> on_failure,
    const base::Location& from_here,
    chromeos::attestation::AttestationStatus status,
    const std::string& data) {
  if (status == chromeos::attestation::ATTESTATION_SUCCESS) {
    on_success.Run(data);
    return;
  }
  LOG(ERROR) << "Cryptohome DBus method or server called failed with status: "
             << status << ": " << from_here.ToString();
  if (!on_failure.is_null())
    on_failure.Run(status);
}

}  // namespace

namespace chromeos {
namespace attestation {

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client)
    : EnrollmentCertificateUploaderImpl(policy_client,
                                        nullptr, /* cryptohome_client */
                                        nullptr /* attestation_flow */) {}

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client,
    CryptohomeClient* cryptohome_client,
    AttestationFlow* attestation_flow)
    : policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
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

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR)
        << "EnrollmentCertificateUploaderImpl: Invalid CloudPolicyClient.";
    RunCallbacks(false);
    return;
  }

  if (!cryptohome_client_)
    cryptohome_client_ = CryptohomeClient::Get();

  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_.reset(new AttestationFlow(
        cryptohome::AsyncMethodCaller::GetInstance(), cryptohome_client_,
        std::move(attestation_ca_client)));
    attestation_flow_ = default_attestation_flow_.get();
  }

  GetCertificate();
}

void EnrollmentCertificateUploaderImpl::RunCallbacks(bool status) {
  for (; !callbacks_.empty(); callbacks_.pop())
    std::move(callbacks_.front()).Run(status);
}

void EnrollmentCertificateUploaderImpl::GetCertificate() {
  // We can reuse the dbus callback handler logic.
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      false,             // Do not force a new key to be generated.
      std::string(),     // Leave key name empty to generate a default name.
      base::BindRepeating(
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
              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void EnrollmentCertificateUploaderImpl::UploadCertificate(
    const std::string& pem_certificate_chain) {
  policy_client_->UploadEnterpriseEnrollmentCertificate(
      pem_certificate_chain,
      base::BindRepeating(&EnrollmentCertificateUploaderImpl::OnUploadComplete,
                          weak_factory_.GetWeakPtr()));
}

void EnrollmentCertificateUploaderImpl::OnUploadComplete(bool status) {
  if (status) {
    VLOG(1) << "Enterprise Enrollment Certificate uploaded to DMServer.";
  } else {
    LOG(ERROR)
        << "Failed to upload Enterprise Enrollment Certificate to DMServer.";
  }

  RunCallbacks(status);
}

void EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure(
    AttestationStatus status) {
  if (status != ATTESTATION_SERVER_BAD_REQUEST_FAILURE)
    Reschedule();
  else
    RunCallbacks(false);
}

void EnrollmentCertificateUploaderImpl::Reschedule() {
  if (++num_retries_ < retry_limit_) {
    base::PostDelayedTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&EnrollmentCertificateUploaderImpl::GetCertificate,
                       weak_factory_.GetWeakPtr()),
        retry_delay_);
  } else {
    LOG(WARNING) << "EnrollmentCertificateUploaderImpl: Retry limit exceeded.";
    RunCallbacks(false);
  }
}

}  // namespace attestation
}  // namespace chromeos
