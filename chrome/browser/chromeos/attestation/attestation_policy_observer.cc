// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_policy_observer.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/attestation/attestation_key_payload.pb.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/cert/pem_tokenizer.h"
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
void DBusBoolRedirectCallback(const base::Closure& on_true,
                              const base::Closure& on_false,
                              const base::Closure& on_failure,
                              const base::Location& from_here,
                              base::Optional<bool> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Cryptohome DBus method failed: " << from_here.ToString();
    if (!on_failure.is_null())
      on_failure.Run();
    return;
  }
  const base::Closure& task = result.value() ? on_true : on_false;
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
    const base::Callback<void(const std::string&)> on_success,
    const base::Closure& on_failure,
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

AttestationPolicyObserver::AttestationPolicyObserver(
    policy::CloudPolicyClient* policy_client)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(nullptr),
      attestation_flow_(nullptr),
      num_retries_(0),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  attestation_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceAttestationEnabled,
      base::Bind(&AttestationPolicyObserver::AttestationSettingChanged,
                 base::Unretained(this)));
  Start();
}

AttestationPolicyObserver::AttestationPolicyObserver(
    policy::CloudPolicyClient* policy_client,
    CryptohomeClient* cryptohome_client,
    AttestationFlow* attestation_flow)
    : cros_settings_(CrosSettings::Get()),
      policy_client_(policy_client),
      cryptohome_client_(cryptohome_client),
      attestation_flow_(attestation_flow),
      num_retries_(0),
      retry_delay_(kRetryDelay),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  attestation_subscription_ = cros_settings_->AddSettingsObserver(
      kDeviceAttestationEnabled,
      base::Bind(&AttestationPolicyObserver::AttestationSettingChanged,
                 base::Unretained(this)));
  Start();
}

AttestationPolicyObserver::~AttestationPolicyObserver() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void AttestationPolicyObserver::AttestationSettingChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  num_retries_ = 0;
  Start();
}

void AttestationPolicyObserver::Start() {
  // If attestation is not enabled, there is nothing to do.
  bool enabled = false;
  if (!cros_settings_->GetBoolean(kDeviceAttestationEnabled, &enabled) ||
      !enabled)
    return;

  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "AttestationPolicyObserver: Invalid CloudPolicyClient.";
    return;
  }

  if (!cryptohome_client_)
    cryptohome_client_ = DBusThreadManager::Get()->GetCryptohomeClient();

  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_.reset(new AttestationFlow(
        cryptohome::AsyncMethodCaller::GetInstance(), cryptohome_client_,
        std::move(attestation_ca_client)));
    attestation_flow_ = default_attestation_flow_.get();
  }

  // Start a dbus call to check if an Enterprise Machine Key already exists.
  base::Closure on_does_exist =
      base::Bind(&AttestationPolicyObserver::GetExistingCertificate,
                 weak_factory_.GetWeakPtr());
  base::Closure on_does_not_exist =
      base::Bind(&AttestationPolicyObserver::GetNewCertificate,
                 weak_factory_.GetWeakPtr());
  cryptohome_client_->TpmAttestationDoesKeyExist(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey,
      base::BindOnce(DBusBoolRedirectCallback, on_does_exist, on_does_not_exist,
                     base::Bind(&AttestationPolicyObserver::Reschedule,
                                weak_factory_.GetWeakPtr()),
                     FROM_HERE));
}

void AttestationPolicyObserver::GetNewCertificate() {
  // We can reuse the dbus callback handler logic.
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      true,              // Force a new key to be generated.
      base::Bind(
          [](const base::RepeatingCallback<void(const std::string&)> on_success,
             const base::RepeatingCallback<void(AttestationStatus)> on_failure,
             const base::Location& from_here, AttestationStatus status,
             const std::string& data) {
            DBusPrivacyCACallback(on_success, on_failure, from_here, status,
                                  std::move(data));
          },
          base::BindRepeating(&AttestationPolicyObserver::UploadCertificate,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &AttestationPolicyObserver::HandleGetCertificateFailure,
              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void AttestationPolicyObserver::GetExistingCertificate() {
  cryptohome_client_->TpmAttestationGetCertificate(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey,
      base::Bind(DBusStringCallback,
                 base::Bind(&AttestationPolicyObserver::CheckCertificateExpiry,
                            weak_factory_.GetWeakPtr()),
                 base::Bind(&AttestationPolicyObserver::Reschedule,
                            weak_factory_.GetWeakPtr()),
                 FROM_HERE));
}

void AttestationPolicyObserver::CheckCertificateExpiry(
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
  // Get the payload and check if the certificate has already been uploaded.
  GetKeyPayload(base::Bind(&AttestationPolicyObserver::CheckIfUploaded,
                           weak_factory_.GetWeakPtr(),
                           pem_certificate_chain));
}

void AttestationPolicyObserver::UploadCertificate(
    const std::string& pem_certificate_chain) {
  policy_client_->UploadEnterpriseMachineCertificate(
      pem_certificate_chain,
      base::Bind(&AttestationPolicyObserver::OnUploadComplete,
                 weak_factory_.GetWeakPtr()));
}

void AttestationPolicyObserver::CheckIfUploaded(
    const std::string& pem_certificate_chain,
    const std::string& key_payload) {
  AttestationKeyPayload payload_pb;
  if (!key_payload.empty() &&
      payload_pb.ParseFromString(key_payload) &&
      payload_pb.is_certificate_uploaded()) {
    // Already uploaded... nothing more to do.
    return;
  }
  UploadCertificate(pem_certificate_chain);
}

void AttestationPolicyObserver::GetKeyPayload(
    base::Callback<void(const std::string&)> callback) {
  cryptohome_client_->TpmAttestationGetKeyPayload(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey,
      base::Bind(DBusStringCallback, callback,
                 base::Bind(&AttestationPolicyObserver::Reschedule,
                            weak_factory_.GetWeakPtr()),
                 FROM_HERE));
}

void AttestationPolicyObserver::OnUploadComplete(bool status) {
  if (!status)
    return;
  VLOG(1) << "Enterprise Machine Certificate uploaded to DMServer.";
  GetKeyPayload(base::Bind(&AttestationPolicyObserver::MarkAsUploaded,
                           weak_factory_.GetWeakPtr()));
}

void AttestationPolicyObserver::MarkAsUploaded(const std::string& key_payload) {
  AttestationKeyPayload payload_pb;
  if (!key_payload.empty())
    payload_pb.ParseFromString(key_payload);
  payload_pb.set_is_certificate_uploaded(true);
  std::string new_payload;
  if (!payload_pb.SerializeToString(&new_payload)) {
    LOG(WARNING) << "Failed to serialize key payload.";
    return;
  }
  cryptohome_client_->TpmAttestationSetKeyPayload(
      KEY_DEVICE,
      cryptohome::AccountIdentifier(),  // Not used.
      kEnterpriseMachineKey, new_payload,
      base::BindRepeating(DBusBoolRedirectCallback, base::RepeatingClosure(),
                          base::RepeatingClosure(), base::RepeatingClosure(),
                          FROM_HERE));
}

void AttestationPolicyObserver::HandleGetCertificateFailure(
    AttestationStatus status) {
  if (status != ATTESTATION_SERVER_BAD_REQUEST_FAILURE)
    Reschedule();
}

void AttestationPolicyObserver::Reschedule() {
  if (++num_retries_ < retry_limit_) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindRepeating(&AttestationPolicyObserver::Start,
                            weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(retry_delay_));
  } else {
    LOG(WARNING) << "AttestationPolicyObserver: Retry limit exceeded.";
  }
}

}  // namespace attestation
}  // namespace chromeos
