// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace attestation {

using ::attestation::VerifiedAccessFlow;
using Result = TpmChallengeKeyResult;
using ResultCode = TpmChallengeKeyResultCode;

//==================== TpmChallengeKeySubtleFactory ============================

TpmChallengeKeySubtle* TpmChallengeKeySubtleFactory::next_result_for_testing_ =
    nullptr;

// static
std::unique_ptr<TpmChallengeKeySubtle> TpmChallengeKeySubtleFactory::Create() {
  if (next_result_for_testing_) [[unlikely]] {
    std::unique_ptr<TpmChallengeKeySubtle> result(next_result_for_testing_);
    next_result_for_testing_ = nullptr;
    return result;
  }

  return std::make_unique<TpmChallengeKeySubtleImpl>();
}

// static
std::unique_ptr<TpmChallengeKeySubtle>
TpmChallengeKeySubtleFactory::CreateForPreparedKey(
    VerifiedAccessFlow flow_type,
    bool will_register_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::string& public_key,
    Profile* profile) {
  auto result = TpmChallengeKeySubtleFactory::Create();
  result->RestorePreparedKeyState(flow_type, will_register_key, key_crypto_type,
                                  key_name, public_key, profile);
  return result;
}

// static
void TpmChallengeKeySubtleFactory::SetForTesting(
    std::unique_ptr<TpmChallengeKeySubtle> next_result) {
  DCHECK(next_result_for_testing_ == nullptr);
  // unique_ptr itself cannot be stored in a static variable because of its
  // complex destructor.
  next_result_for_testing_ = next_result.release();
}

// static
bool TpmChallengeKeySubtleFactory::WillReturnTestingInstance() {
  return (next_result_for_testing_ != nullptr);
}

//===================== TpmChallengeKeySubtleImpl ==============================

namespace {

// Returns true if the device is enterprise managed.
bool IsEnterpriseDevice() {
  return InstallAttributes::Get()->IsEnterpriseManaged();
}

// For unmanaged devices we need to ask for user consent if the key does not
// exist because data will be sent to the PCA. In case of the flow type being
// DEVICE_TRUST_CONNECTOR, user consent is not required since it's only used
// for attesting the DTC payload and is not usable by extensions.
// Historical note: For managed device there used to be policies to control this
// (AttestationEnabledForUser,AttestationEnabledForDevice) but they were removed
// from the client after having been set to true unconditionally for all clients
// for a long time.
bool IsUserConsentRequired(VerifiedAccessFlow flow_type) {
  if (flow_type == VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR) {
    return false;
  }

  return !IsEnterpriseDevice();
}

// If no key name was given, use default well-known key names so they can be
// reused across attestation operations (multiple challenge responses can be
// generated using the same key).
std::string GetDefaultKeyName(VerifiedAccessFlow flow_type,
                              ::attestation::KeyType key_crypto_type) {
  // When the caller wants to "register" a key through attestation (resulting in
  // a general-purpose key in the chaps PKCS#11 store), the behavior is
  // different between EUK and EMK:
  //
  // For EUK, the EUK used to sign the attestation response is also the key that
  // is "registered". If the EUK does not exist, one will be generated; but if
  // it does already exist, the existing key will be used.  Thus it must be
  // parameterized with the crypto algorithm type to ensure that the caller gets
  // a key of the type they expect.
  //
  //  For EMK, the EMK used to sign the attestation response must remain stable
  //  and instead a newly-generated EMK is registered. This function returns the
  //  EMK that is used to sign the attesation response, so it may not be
  //  parametrized with the crypto algorithm type.
  //
  //  See http://go/chromeos-va-registering-device-wide-keys-support for details
  //  on the concept of the stable EMK.
  switch (flow_type) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      return kEnterpriseMachineKey;
    case VerifiedAccessFlow::ENTERPRISE_USER:
      switch (key_crypto_type) {
        case ::attestation::KEY_TYPE_RSA:
          return kEnterpriseUserKey;
        case ::attestation::KEY_TYPE_ECC:
          return std::string(kEnterpriseUserKey) + "-ecdsa";
      }
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// Returns the key name that should be used for the attestation platform APIs.
std::string GetKeyNameWithDefault(VerifiedAccessFlow flow_type,
                                  ::attestation::KeyType key_crypto_type,
                                  const std::string& key_name) {
  if (!key_name.empty())
    return key_name;

  return GetDefaultKeyName(flow_type, key_crypto_type);
}

}  // namespace

TpmChallengeKeySubtleImpl::TpmChallengeKeySubtleImpl()
    : default_attestation_flow_(std::make_unique<AttestationFlowAdaptive>(
          std::make_unique<AttestationCAClient>())),
      attestation_flow_(default_attestation_flow_.get()) {
  policy::DeviceCloudPolicyManagerAsh* manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceCloudPolicyManager();
  if (manager) {
    machine_certificate_uploader_ = manager->GetMachineCertificateUploader();
  }
}

TpmChallengeKeySubtleImpl::TpmChallengeKeySubtleImpl(
    AttestationFlow* attestation_flow_for_testing,
    MachineCertificateUploader* machine_certificate_uploader_for_testing)
    : attestation_flow_(attestation_flow_for_testing),
      machine_certificate_uploader_(machine_certificate_uploader_for_testing) {}

TpmChallengeKeySubtleImpl::~TpmChallengeKeySubtleImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeySubtleImpl::RestorePreparedKeyState(
    VerifiedAccessFlow flow_type,
    bool will_register_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    const std::string& public_key,
    Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!will_register_key || !public_key.empty());

  // Ensure that the selected flow type is supported
  CHECK(flow_type == VerifiedAccessFlow::ENTERPRISE_MACHINE ||
        flow_type == VerifiedAccessFlow::ENTERPRISE_USER);

  // For the ENTERPRISE_USER flow, a |profile| is strictly necessary.
  DCHECK(flow_type != VerifiedAccessFlow::ENTERPRISE_USER || profile);

  // For DEVICE_TRUST_CONNECTOR, a key name is required and registering a key is
  // not allowed.
  CHECK(flow_type != VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR ||
        !key_name.empty());
  CHECK(flow_type != VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR ||
        !will_register_key);

  flow_type_ = flow_type;
  will_register_key_ = will_register_key;
  key_crypto_type_ = key_crypto_type;
  key_name_ = GetKeyNameWithDefault(flow_type, key_crypto_type, key_name);
  public_key_ = public_key;
  profile_ = profile;
}

void TpmChallengeKeySubtleImpl::StartPrepareKeyStep(
    VerifiedAccessFlow flow_type,
    bool will_register_key,
    ::attestation::KeyType key_crypto_type,
    const std::string& key_name,
    Profile* profile,
    TpmChallengeKeyCallback callback,
    const std::optional<std::string>& signals) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());

  // For ENTERPRISE_MACHINE: if |will_register_key| is true, |key_name| should
  // not be empty, if |register_key| is false, |key_name| will not be used.
  DCHECK((flow_type != VerifiedAccessFlow::ENTERPRISE_MACHINE) ||
         (will_register_key == !key_name.empty()))
      << "Invalid arguments: " << will_register_key << " " << !key_name.empty();

  // For ENTERPRISE_USER, a |profile| is strictly necessary.
  DCHECK(flow_type != VerifiedAccessFlow::ENTERPRISE_USER || profile);

  // For DEVICE_TRUST_CONNECTOR, a key name is required and registering a key is
  // not allowed.
  CHECK(flow_type != VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR ||
        !key_name.empty());
  CHECK(flow_type != VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR ||
        !will_register_key);

  // Ensure that the selected flow type is supported
  if (flow_type != VerifiedAccessFlow::ENTERPRISE_MACHINE &&
      flow_type != VerifiedAccessFlow::ENTERPRISE_USER &&
      flow_type != VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR) {
    std::move(callback).Run(
        Result::MakeError(ResultCode::kVerifiedAccessFlowUnsupportedError));
    return;
  }

  flow_type_ = flow_type;
  will_register_key_ = will_register_key;
  key_crypto_type_ = key_crypto_type;
  key_name_ = GetKeyNameWithDefault(flow_type, key_crypto_type, key_name);
  profile_ = profile;
  callback_ = std::move(callback);
  signals_ = signals;

  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      PrepareEnterpriseMachineFlow();
      return;
    case VerifiedAccessFlow::ENTERPRISE_USER:
      PrepareEnterpriseUserFlow();
      return;
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      PrepareDeviceTrustConnectorFlow();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void TpmChallengeKeySubtleImpl::PrepareEnterpriseMachineFlow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if the device is enterprise enrolled.
  if (!IsEnterpriseDevice()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kNonEnterpriseDeviceError));
    return;
  }

  // Check whether the user is affiliated unless this is a device-wide instance.
  if (GetUser() && !IsUserAffiliated()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserNotManagedError));
    return;
  }

  // Wait for the machine certificate to be uploaded.
  if (machine_certificate_uploader_) {
    machine_certificate_uploader_->WaitForUploadComplete(base::BindOnce(
        &TpmChallengeKeySubtleImpl::PrepareKey, weak_factory_.GetWeakPtr()));
  } else {
    PrepareKey(true);
  }
}

void TpmChallengeKeySubtleImpl::PrepareEnterpriseUserFlow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if user keys are available in this profile.
  if (!GetUser()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserKeyNotAvailableError));
    return;
  }

  if (IsEnterpriseDevice()) {
    if (!IsUserAffiliated()) {
      std::move(callback_).Run(
          Result::MakeError(ResultCode::kUserNotManagedError));
      return;
    }
  }

  PrepareKey(true);
}

void TpmChallengeKeySubtleImpl::PrepareDeviceTrustConnectorFlow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/277707201): remove once user email from login screen is available
  // here.
  if (!GetUser()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserKeyNotAvailableError));
    return;
  }

  // Check whether the user is managed unless this is a device-wide instance.
  if (GetUser() && !IsUserManaged()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserNotManagedError));
    return;
  }

  PrepareKey(true);
}

bool TpmChallengeKeySubtleImpl::IsUserManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!profile_) {
    return false;
  }

  const auto* profile_policy_connector = profile_->GetProfilePolicyConnector();

  if (!profile_policy_connector) {
    return false;
  }

  return profile_policy_connector->IsManaged();
}

bool TpmChallengeKeySubtleImpl::IsUserAffiliated() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const user_manager::User* const user = GetUser();
  if (user) {
    return user->IsAffiliated();
  }
  return false;
}

std::string TpmChallengeKeySubtleImpl::GetEmail() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      return std::string();
    case VerifiedAccessFlow::ENTERPRISE_USER:
      [[fallthrough]];
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      return GetAccountId().GetUserEmail();
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

AttestationCertificateProfile TpmChallengeKeySubtleImpl::GetCertificateProfile()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      return PROFILE_ENTERPRISE_MACHINE_CERTIFICATE;
    case VerifiedAccessFlow::ENTERPRISE_USER:
      return PROFILE_ENTERPRISE_USER_CERTIFICATE;
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      return PROFILE_DEVICE_TRUST_USER_CERTIFICATE;
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

const user_manager::User* TpmChallengeKeySubtleImpl::GetUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!profile_)
    return nullptr;
  return ProfileHelper::Get()->GetUserByProfile(profile_);
}

AccountId TpmChallengeKeySubtleImpl::GetAccountId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const user_manager::User* user = GetUser();
  if (user) {
    return user->GetAccountId();
  }
  // Signin profile doesn't have associated user.
  return EmptyAccountId();
}

AccountId TpmChallengeKeySubtleImpl::GetAccountIdForAttestationFlow() const {
  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      [[fallthrough]];
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      return EmptyAccountId();
    case VerifiedAccessFlow::ENTERPRISE_USER:
      return GetAccountId();
    default:
      LOG(DFATAL) << "Unsupported Verified Access flow type: " << flow_type_;
      return EmptyAccountId();
  }
}

std::string TpmChallengeKeySubtleImpl::GetUsernameForAttestationClient() const {
  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      [[fallthrough]];
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      return std::string();
    case VerifiedAccessFlow::ENTERPRISE_USER:
      return cryptohome::Identification(GetAccountId()).id();
    default:
      LOG(DFATAL) << "Unsupported Verified Access flow type: " << flow_type_;
      return std::string();
  }
}

// For ENTERPRISE_MACHINE attestation, don't include the certificate of the
// signing key, because the verified access server uses the "stable EMK
// certificate" uploaded to DMServer after enrollment.
bool TpmChallengeKeySubtleImpl::ShouldIncludeSigningKeyCertificate() const {
  if (flow_type_ == VerifiedAccessFlow::ENTERPRISE_MACHINE) {
    return false;
  }
  return true;
}

bool TpmChallengeKeySubtleImpl::ShouldIncludeCustomerId() const {
  // Request to include the customer ID in the challenge response when:
  // * the request is a machine challenge
  // * the request is a user challenge and this is a kiosk session
  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      return true;
    case VerifiedAccessFlow::ENTERPRISE_USER:
      return chromeos::IsKioskSession();
    case VerifiedAccessFlow::DEVICE_TRUST_CONNECTOR:
      return false;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unsupported Verified Access flow type: " << flow_type_;
      return false;
  }
}

void TpmChallengeKeySubtleImpl::PrepareKey(bool can_continue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!can_continue) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUploadCertificateFailedError));
    return;
  }

  ::attestation::GetEnrollmentPreparationsRequest request;
  AttestationClient::Get()->GetEnrollmentPreparations(
      request,
      base::BindOnce(
          &TpmChallengeKeySubtleImpl::GetEnrollmentPreparationsCallback,
          weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::GetEnrollmentPreparationsCallback(
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    std::move(callback_).Run(
        Result::MakeError(reply.status() == ::attestation::STATUS_DBUS_ERROR
                              ? ResultCode::kDbusError
                              : ResultCode::kAttestationServiceInternalError));
    return;
  }

  if (!AttestationClient::IsAttestationPrepared(reply)) {
    chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
        ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
        base::BindOnce(
            &TpmChallengeKeySubtleImpl::PrepareKeyErrorHandlerCallback,
            weak_factory_.GetWeakPtr()));
    return;
  }

  ::attestation::GetKeyInfoRequest request;
  request.set_username(GetUsernameForAttestationClient());
  request.set_key_label(key_name_);
  AttestationClient::Get()->GetKeyInfo(
      request, base::BindOnce(&TpmChallengeKeySubtleImpl::DoesKeyExistCallback,
                              weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::PrepareKeyErrorHandlerCallback(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to get TPM status; status: " << reply.status();
    std::move(callback_).Run(Result::MakeError(ResultCode::kDbusError));
    return;
  }

  if (reply.is_enabled()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kResetRequiredError));
  } else {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kAttestationUnsupportedError));
  }
}

void TpmChallengeKeySubtleImpl::DoesKeyExistCallback(
    const ::attestation::GetKeyInfoReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::attestation::STATUS_SUCCESS &&
      reply.status() != ::attestation::STATUS_INVALID_PARAMETER) {
    std::move(callback_).Run(
        Result::MakeError(reply.status() == ::attestation::STATUS_DBUS_ERROR
                              ? ResultCode::kDbusError
                              : ResultCode::kAttestationServiceInternalError));
    return;
  }

  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    // The key exists. Do nothing more.
    PrepareKeyFinished(reply);
    return;
  }

  // The key does not exist. Create a new key and have it signed by PCA.
  if (IsUserConsentRequired(flow_type_)) {
    // We should ask the user explicitly before sending any private
    // information to PCA.
    AskForUserConsent(
        base::BindOnce(&TpmChallengeKeySubtleImpl::AskForUserConsentCallback,
                       weak_factory_.GetWeakPtr()));
  } else {
    // User consent is not required. Skip to the next step.
    AskForUserConsentCallback(true);
  }
}

void TpmChallengeKeySubtleImpl::AskForUserConsent(
    base::OnceCallback<void(bool)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(davidyu): right now we just simply reject the request before we have
  // a way to ask for user consent.
  std::move(callback).Run(false);
}

void TpmChallengeKeySubtleImpl::AskForUserConsentCallback(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result) {
    // The user rejects the request.
    std::move(callback_).Run(Result::MakeError(ResultCode::kUserRejectedError));
    return;
  }

  // Generate a new key and have it signed by PCA.
  attestation_flow_->GetCertificate(
      /*certificate_profile=*/GetCertificateProfile(),
      /*account_id=*/GetAccountIdForAttestationFlow(),
      /*request_origin=*/std::string(),  // Not used.
      /*force_new_key=*/true, /*key_crypto_type=*/key_crypto_type_,
      /*key_name=*/key_name_, /*profile_specific_data=*/std::nullopt,
      /*callback=*/
      base::BindOnce(&TpmChallengeKeySubtleImpl::GetCertificateCallback,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::GetCertificateCallback(
    AttestationStatus status,
    const std::string& pem_certificate_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != ATTESTATION_SUCCESS) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kGetCertificateFailedError));
    return;
  }

  GetPublicKey();
}

void TpmChallengeKeySubtleImpl::GetPublicKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ::attestation::GetKeyInfoRequest request;
  request.set_username(GetUsernameForAttestationClient());
  request.set_key_label(key_name_);
  AttestationClient::Get()->GetKeyInfo(
      request, base::BindOnce(&TpmChallengeKeySubtleImpl::PrepareKeyFinished,
                              weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::PrepareKeyFinished(
    const ::attestation::GetKeyInfoReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kGetPublicKeyFailedError));
    return;
  }

  if (will_register_key_) {
    public_key_ = reply.public_key();
  }

  std::move(callback_).Run(Result::MakePublicKey(reply.public_key()));
}

void TpmChallengeKeySubtleImpl::StartSignChallengeStep(
    const std::string& challenge,
    TpmChallengeKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);

  // See http://go/chromeos-va-registering-device-wide-keys-support for details
  // about both key names.

  // Name of the key that will be used to sign challenge.
  // ENTERPRISE_MACHINE challenges are signed using a stable key.
  std::string key_name_for_challenge =
      (flow_type_ == VerifiedAccessFlow::ENTERPRISE_MACHINE)
          ? GetDefaultKeyName(flow_type_, key_crypto_type_)
          : key_name_;
  // Name of the key that will be included in SPKAC, it is used only when SPKAC
  // should be included for the flow type ENTERPRISE_MACHINE.
  std::string key_name_for_spkac =
      (will_register_key_ &&
       flow_type_ == VerifiedAccessFlow::ENTERPRISE_MACHINE)
          ? key_name_
          : std::string();

  ::attestation::SignEnterpriseChallengeRequest request;
  request.set_username(GetUsernameForAttestationClient());
  request.set_key_label(key_name_for_challenge);
  request.set_key_name_for_spkac(key_name_for_spkac);
  request.set_domain(GetEmail());
  request.set_device_id(InstallAttributes::Get()->GetDeviceId());
  request.set_include_signed_public_key(will_register_key_);
  request.set_challenge(challenge);
  request.set_va_type(AttestationClient::GetVerifiedAccessServerType());
  request.set_flow_type(flow_type_);
  request.set_include_certificate(ShouldIncludeSigningKeyCertificate());
  if (signals_.has_value()) {
    request.set_device_trust_signals_json(signals_.value());
  }
  request.set_include_customer_id(ShouldIncludeCustomerId());
  AttestationClient::Get()->SignEnterpriseChallenge(
      request, base::BindOnce(&TpmChallengeKeySubtleImpl::SignChallengeCallback,
                              weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::SignChallengeCallback(
    const ::attestation::SignEnterpriseChallengeReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kSignChallengeFailedError));
    return;
  }

  std::move(callback_).Run(
      Result::MakeChallengeResponse(reply.challenge_response()));
}

void TpmChallengeKeySubtleImpl::StartRegisterKeyStep(
    TpmChallengeKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(will_register_key_);

  callback_ = std::move(callback);

  ::attestation::RegisterKeyWithChapsTokenRequest request;
  request.set_username(GetUsernameForAttestationClient());
  request.set_key_label(key_name_);
  request.set_include_certificates(false);

  AttestationClient::Get()->RegisterKeyWithChapsToken(
      request, base::BindOnce(&TpmChallengeKeySubtleImpl::RegisterKeyCallback,
                              weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::RegisterKeyCallback(
    const ::attestation::RegisterKeyWithChapsTokenReply& reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "Failed to call RegisterKeyWithChapsToken; status: "
               << reply.status();
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kKeyRegistrationFailedError));
    return;
  }

  DCHECK(flow_type_ == VerifiedAccessFlow::ENTERPRISE_MACHINE || profile_);

  platform_keys::KeyPermissionsManager* key_permissions_manager = nullptr;
  switch (flow_type_) {
    case VerifiedAccessFlow::ENTERPRISE_USER:
      key_permissions_manager = platform_keys::KeyPermissionsManagerImpl::
          GetUserPrivateTokenKeyPermissionsManager(profile_);
      break;
    case VerifiedAccessFlow::ENTERPRISE_MACHINE:
      key_permissions_manager = platform_keys::KeyPermissionsManagerImpl::
          GetSystemTokenKeyPermissionsManager();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  DCHECK(!public_key_.empty());
  key_permissions_manager->AllowKeyForUsage(
      base::BindOnce(&TpmChallengeKeySubtleImpl::MarkCorporateKeyCallback,
                     weak_factory_.GetWeakPtr()),
      platform_keys::KeyUsage::kCorporate,
      std::vector<uint8_t>(public_key_.begin(), public_key_.end()));
}

void TpmChallengeKeySubtleImpl::MarkCorporateKeyCallback(
    chromeos::platform_keys::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kMarkCorporateKeyFailedError));
    return;
  }

  std::move(callback_).Run(Result::MakeSuccess());
}

}  // namespace attestation
}  // namespace ash
