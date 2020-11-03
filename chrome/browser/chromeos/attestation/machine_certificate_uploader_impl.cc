// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/machine_certificate_uploader_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/cert/pem.h"
#include "net/cert/x509_certificate.h"

namespace {

// The number of days before a certificate expires during which it is
// considered 'expiring soon' and replacement is initiated.  The Chrome OS CA
// issues certificates with an expiry of at least two years.  This value has
// been set large enough so that the majority of users will have gone through
// a full sign-in during the period.
const int kExpiryThresholdInDays = 30;
const int kRetryDelay = 5;  // Seconds.
const int kRetryLimit = 100;

// A dbus callback which handles a boolean result.
//
// Parameters
//   on_true - Called when status=success and value=true.
//   on_false - Called when status=success and value=false.
//   status - The dbus operation status.
//   result - The value returned by the dbus operation.
void DBusBoolRedirectCallback(const base::RepeatingClosure& on_true,
                              const base::RepeatingClosure& on_false,
                              const base::RepeatingClosure& on_failure,
                              const base::Location& from_here,
                              base::Optional<bool> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Cryptohome DBus method failed: " << from_here.ToString();
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }
  const base::RepeatingClosure& task = result.value() ? on_true : on_false;
  if (!task.is_null())
    task.Run();
}

// A dbus callback which handles a string result.
//
// Parameters
//   on_success - Called when status=success and result=true.
//   status - The dbus operation status.
//   result - The result returned by the dbus operation.
//   data - The data returned by the dbus operation.
void DBusStringCallback(
    const base::RepeatingCallback<void(const std::string&)> on_success,
    const base::RepeatingClosure& on_failure,
    const base::Location& from_here,
    base::Optional<chromeos::CryptohomeClient::TpmAttestationDataResult>
        result) {
  if (!result.has_value() || !result->success) {
    LOG(ERROR) << "Cryptohome DBus method failed: " << from_here.ToString();
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }
  on_success.Run(result->data);
}

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
  LOG(ERROR) << "Cryptohome DBus method or server called failed with status:"
             << status << ": " << from_here.ToString();
  if (!on_failure.is_null())
    on_failure.Run(status);
}

}  // namespace

namespace chromeos {
namespace attestation {

MachineCertificateUploaderImpl::MachineCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client)
    : policy_client_(policy_client),
      cryptohome_client_(nullptr),
      attestation_flow_(nullptr),
      num_retries_(0),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MachineCertificateUploaderImpl::MachineCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client,
    CryptohomeClient* cryptohome_client,
    AttestationFlow* attestation_flow)
    : policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
      attestation_flow_(attestation_flow),
      num_retries_(0),
      retry_delay_(kRetryDelay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MachineCertificateUploaderImpl::~MachineCertificateUploaderImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MachineCertificateUploaderImpl::UploadCertificateIfNeeded(
    UploadCallback callback) {
  refresh_certificate_ = false;
  callbacks_.push_back(std::move(callback));
  num_retries_ = 0;
  Start();
}

void MachineCertificateUploaderImpl::RefreshAndUploadCertificate(
    UploadCallback callback) {
  refresh_certificate_ = true;
  callbacks_.push_back(std::move(callback));
  num_retries_ = 0;
  Start();
}

void MachineCertificateUploaderImpl::Start() {
  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "MachineCertificateUploaderImpl: Invalid CloudPolicyClient.";
    certificate_uploaded_ = false;
    RunCallbacks(certificate_uploaded_.value());
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

  // Always get a new certificate if we are asked for a fresh one.
  if (refresh_certificate_) {
    GetNewCertificate();
    return;
  }

  // Start a dbus call to check if an Enterprise Machine Key already exists.
  base::RepeatingClosure on_does_not_exist =
      base::BindRepeating(&MachineCertificateUploaderImpl::GetNewCertificate,
                          weak_factory_.GetWeakPtr());
  base::RepeatingClosure on_does_exist = base::BindRepeating(
      &MachineCertificateUploaderImpl::GetExistingCertificate,
      weak_factory_.GetWeakPtr());
  cryptohome_client_->TpmAttestationDoesKeyExist(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey,
      base::BindOnce(
          DBusBoolRedirectCallback, on_does_exist, on_does_not_exist,
          base::BindRepeating(&MachineCertificateUploaderImpl::Reschedule,
                              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void MachineCertificateUploaderImpl::GetNewCertificate() {
  // We can reuse the dbus callback handler logic.
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      true,              // Force a new key to be generated.
      std::string(),     // Leave key name empty to generate a default name.
      base::BindOnce(
          [](const base::RepeatingCallback<void(const std::string&)> on_success,
             const base::RepeatingCallback<void(AttestationStatus)> on_failure,
             const base::Location& from_here, AttestationStatus status,
             const std::string& data) {
            DBusPrivacyCACallback(on_success, on_failure, from_here, status,
                                  std::move(data));
          },
          base::BindRepeating(
              &MachineCertificateUploaderImpl::UploadCertificate,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &MachineCertificateUploaderImpl::HandleGetCertificateFailure,
              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void MachineCertificateUploaderImpl::GetExistingCertificate() {
  cryptohome_client_->TpmAttestationGetCertificate(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey,
      base::BindOnce(
          DBusStringCallback,
          base::BindRepeating(
              &MachineCertificateUploaderImpl::CheckCertificateExpiry,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(&MachineCertificateUploaderImpl::Reschedule,
                              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void MachineCertificateUploaderImpl::CheckCertificateExpiry(
    const std::string& pem_certificate_chain) {
  int num_certificates = 0;
  net::PEMTokenizer pem_tokenizer(pem_certificate_chain, {"CERTIFICATE"});
  while (pem_tokenizer.GetNext()) {
    ++num_certificates;
    scoped_refptr<net::X509Certificate> x509 =
        net::X509Certificate::CreateFromBytes(pem_tokenizer.data().data(),
                                              pem_tokenizer.data().length());
    if (!x509.get() || x509->valid_expiry().is_null()) {
      // This logic intentionally fails open. In theory this should not happen
      // but in practice parsing X.509 can be brittle and there are a lot of
      // factors including which underlying module is parsing the certificate,
      // whether that module performs more checks than just ASN.1/DER format,
      // and the server module that generated the certificate(s). Renewal is
      // expensive so we only renew certificates with good evidence that they
      // have expired or will soon expire; if we don't know, we don't renew.
      LOG(WARNING) << "Failed to parse certificate, cannot check expiry.";
      continue;
    }
    const base::TimeDelta threshold =
        base::TimeDelta::FromDays(kExpiryThresholdInDays);
    if ((base::Time::Now() + threshold) > x509->valid_expiry()) {
      // The certificate has expired or will soon, replace it.
      GetNewCertificate();
      return;
    }
  }
  if (num_certificates == 0) {
    LOG(WARNING) << "Failed to parse certificate chain, cannot check expiry.";
  }
  ::attestation::GetKeyInfoRequest request;
  request.set_username("");
  request.set_key_label(kEnterpriseMachineKey);
  AttestationClient::Get()->GetKeyInfo(
      request,
      base::BindOnce(&MachineCertificateUploaderImpl::CheckIfUploaded,
                     weak_factory_.GetWeakPtr(), pem_certificate_chain));
}

void MachineCertificateUploaderImpl::UploadCertificate(
    const std::string& pem_certificate_chain) {
  certificate_uploaded_.reset();
  policy_client_->UploadEnterpriseMachineCertificate(
      pem_certificate_chain,
      base::BindOnce(&MachineCertificateUploaderImpl::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void MachineCertificateUploaderImpl::CheckIfUploaded(
    const std::string& pem_certificate_chain,
    const ::attestation::GetKeyInfoReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    Reschedule();
    return;
  }

  AttestationKeyPayload payload_pb;
  if (!reply.payload().empty() && payload_pb.ParseFromString(reply.payload()) &&
      payload_pb.is_certificate_uploaded()) {
    // Already uploaded... nothing more to do.
    certificate_uploaded_ = true;
    RunCallbacks(certificate_uploaded_.value());
    return;
  }
  UploadCertificate(pem_certificate_chain);
}

void MachineCertificateUploaderImpl::OnUploadComplete(bool status) {
  if (status) {
    VLOG(1) << "Enterprise Machine Certificate uploaded to DMServer.";
    ::attestation::GetKeyInfoRequest request;
    request.set_username("");
    request.set_key_label(kEnterpriseMachineKey);
    AttestationClient::Get()->GetKeyInfo(
        request, base::BindOnce(&MachineCertificateUploaderImpl::MarkAsUploaded,
                                weak_factory_.GetWeakPtr()));
  }
  certificate_uploaded_ = status;
  RunCallbacks(certificate_uploaded_.value());
}

void MachineCertificateUploaderImpl::WaitForUploadComplete(
    UploadCallback callback) {
  if (certificate_uploaded_.has_value()) {
    std::move(callback).Run(certificate_uploaded_.value());
    return;
  }

  callbacks_.push_back(std::move(callback));
}

void MachineCertificateUploaderImpl::MarkAsUploaded(
    const ::attestation::GetKeyInfoReply& reply) {
  AttestationKeyPayload payload_pb;
  if (!reply.payload().empty())
    payload_pb.ParseFromString(reply.payload());
  payload_pb.set_is_certificate_uploaded(true);
  std::string new_payload;
  if (!payload_pb.SerializeToString(&new_payload)) {
    LOG(WARNING) << "Failed to serialize key payload.";
    return;
  }
  ::attestation::SetKeyPayloadRequest request;
  request.set_username("");
  request.set_key_label(kEnterpriseMachineKey);
  request.set_payload(new_payload);
  AttestationClient::Get()->SetKeyPayload(request, base::DoNothing());
}

void MachineCertificateUploaderImpl::HandleGetCertificateFailure(
    AttestationStatus status) {
  if (status != ATTESTATION_SERVER_BAD_REQUEST_FAILURE) {
    Reschedule();
  } else {
    certificate_uploaded_ = false;
    RunCallbacks(certificate_uploaded_.value());
  }
}

void MachineCertificateUploaderImpl::Reschedule() {
  if (++num_retries_ < retry_limit_) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MachineCertificateUploaderImpl::Start,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(retry_delay_));
  } else {
    LOG(WARNING) << "MachineCertificateUploaderImpl: Retry limit exceeded.";
    certificate_uploaded_ = false;
    RunCallbacks(certificate_uploaded_.value());
  }
}

void MachineCertificateUploaderImpl::RunCallbacks(bool status) {
  while (!callbacks_.empty()) {
    auto callbacks = std::move(callbacks_);
    callbacks_.clear();

    for (UploadCallback& callback : callbacks) {
      std::move(callback).Run(status);
    }
  }
}

}  // namespace attestation
}  // namespace chromeos
