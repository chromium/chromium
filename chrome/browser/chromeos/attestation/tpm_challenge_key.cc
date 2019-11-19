// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace attestation {

//========================= TpmChallengeKeyFactory =============================

TpmChallengeKey* TpmChallengeKeyFactory::next_result_for_testing_ = nullptr;

// static
std::unique_ptr<TpmChallengeKey> TpmChallengeKeyFactory::Create() {
  if (LIKELY(!next_result_for_testing_)) {
    return std::make_unique<TpmChallengeKeyImpl>();
  }

  std::unique_ptr<TpmChallengeKey> result(next_result_for_testing_);
  next_result_for_testing_ = nullptr;
  return result;
}

// static
void TpmChallengeKeyFactory::SetForTesting(
    std::unique_ptr<TpmChallengeKey> next_result) {
  // unique_ptr itself cannot be stored in a static variable because of its
  // complex destructor.
  next_result_for_testing_ = next_result.release();
}

//=========================== TpmChallengeKeyImpl ==============================

namespace {
// Returns true if the device is enterprise managed.
bool IsEnterpriseDevice() {
  return InstallAttributes::Get()->IsEnterpriseManaged();
}

// For personal devices, we don't need to check if remote attestation is
// enabled in the device, but we need to ask for user consent if the key
// does not exist.
bool IsUserConsentRequired() {
  return !IsEnterpriseDevice();
}
}  // namespace

void TpmChallengeKey::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAttestationEnabled, false);
}

TpmChallengeKeyImpl::TpmChallengeKeyImpl()
    : default_attestation_flow_(std::make_unique<AttestationFlow>(
          cryptohome::AsyncMethodCaller::GetInstance(),
          CryptohomeClient::Get(),
          std::make_unique<AttestationCAClient>())),
      attestation_flow_(default_attestation_flow_.get()) {}

TpmChallengeKeyImpl::TpmChallengeKeyImpl(
    AttestationFlow* attestation_flow_for_testing)
    : attestation_flow_(attestation_flow_for_testing) {}

TpmChallengeKeyImpl::~TpmChallengeKeyImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeyImpl::BuildResponse(AttestationKeyType key_type,
                                        Profile* profile,
                                        TpmChallengeKeyCallback callback,
                                        const std::string& challenge,
                                        bool register_key,
                                        const std::string& key_name_for_spkac) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |key_name_for_spkac| was designed to only be used with KEY_DEVICE.
  DCHECK((key_type != KEY_USER) || key_name_for_spkac.empty())
      << "Key name for SPKAC will be unused.";

  // For device key: if |register_key| is true, |key_name_for_spkac| should not
  // be empty; if |register_key| is false, |key_name_for_spkac| is not used.
  DCHECK((key_type != KEY_DEVICE) ||
         (register_key == !key_name_for_spkac.empty()))
      << "Invalid arguments: " << register_key << " "
      << !key_name_for_spkac.empty();

  challenge_ = challenge;
  profile_ = profile;
  callback_ = std::move(callback);
  key_type_ = key_type;
  register_key_ = register_key;
  key_name_for_spkac_ = key_name_for_spkac;

  switch (key_type_) {
    case KEY_DEVICE:
      ChallengeMachineKey();
      return;
    case KEY_USER:
      ChallengeUserKey();
      return;
  }
  NOTREACHED();
}

void TpmChallengeKeyImpl::ChallengeMachineKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if the device is enterprise enrolled.
  if (!IsEnterpriseDevice()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kNonEnterpriseDeviceError));
    return;
  }

  // Check whether the user is managed unless the signin profile is used.
  if (GetUser() && !IsUserAffiliated()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kUserNotManagedError));
    return;
  }

  // Check if remote attestation is enabled in the device policy.
  GetDeviceAttestationEnabled(base::BindRepeating(
      &TpmChallengeKeyImpl::GetDeviceAttestationEnabledCallback,
      weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::ChallengeUserKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if user keys are available in this profile.
  if (!GetUser()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kUserKeyNotAvailableError));
    return;
  }

  if (!IsRemoteAttestationEnabledForUser()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kUserPolicyDisabledError));
    return;
  }

  if (IsEnterpriseDevice()) {
    if (!IsUserAffiliated()) {
      std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kUserNotManagedError));
      return;
    }

    // Check if remote attestation is enabled in the device policy.
    GetDeviceAttestationEnabled(base::BindRepeating(
        &TpmChallengeKeyImpl::GetDeviceAttestationEnabledCallback,
        weak_factory_.GetWeakPtr()));
  } else {
    GetDeviceAttestationEnabledCallback(true);
  }
}

bool TpmChallengeKeyImpl::IsUserAffiliated() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const user_manager::User* const user = GetUser();
  if (user) {
    return user->IsAffiliated();
  }
  return false;
}

bool TpmChallengeKeyImpl::IsRemoteAttestationEnabledForUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PrefService* prefs = profile_->GetPrefs();
  // TODO(crbug.com/1000589): Check it's mandatory after fixing corp policy.
  if (prefs) {
    return prefs->GetBoolean(prefs::kAttestationEnabled);
  }
  return false;
}

std::string TpmChallengeKeyImpl::GetEmail() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (key_type_) {
    case KEY_DEVICE:
      return InstallAttributes::Get()->GetDomain();
    case KEY_USER:
      return GetAccountId().GetUserEmail();
  }
  NOTREACHED();
}

const char* TpmChallengeKeyImpl::GetKeyName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (key_type_) {
    case KEY_DEVICE:
      return kEnterpriseMachineKey;
    case KEY_USER:
      return kEnterpriseUserKey;
  }
  NOTREACHED();
}

AttestationCertificateProfile TpmChallengeKeyImpl::GetCertificateProfile()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (key_type_) {
    case KEY_DEVICE:
      return PROFILE_ENTERPRISE_MACHINE_CERTIFICATE;
    case KEY_USER:
      return PROFILE_ENTERPRISE_USER_CERTIFICATE;
  }
  NOTREACHED();
}

std::string TpmChallengeKeyImpl::GetKeyNameForRegister() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (key_type_) {
    case KEY_DEVICE:
      return key_name_for_spkac_;
    case KEY_USER:
      return GetKeyName();
  }
  NOTREACHED();
}

const user_manager::User* TpmChallengeKeyImpl::GetUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ProfileHelper::Get()->GetUserByProfile(profile_);
}

AccountId TpmChallengeKeyImpl::GetAccountId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const user_manager::User* user = GetUser();
  if (user) {
    return user->GetAccountId();
  }
  // Signin profile doesn't have associated user.
  return EmptyAccountId();
}

void TpmChallengeKeyImpl::GetDeviceAttestationEnabled(
    const base::RepeatingCallback<void(bool)>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CrosSettings* settings = CrosSettings::Get();
  CrosSettingsProvider::TrustedStatus status = settings->PrepareTrustedValues(
      base::BindRepeating(&TpmChallengeKeyImpl::GetDeviceAttestationEnabled,
                          weak_factory_.GetWeakPtr(), callback));

  bool value = false;
  switch (status) {
    case CrosSettingsProvider::TRUSTED:
      if (!settings->GetBoolean(kDeviceAttestationEnabled, &value)) {
        value = false;
      }
      break;
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Do nothing. This function will be called again when the values are
      // ready.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // If the value cannot be trusted, we assume that the device attestation
      // is false to be on the safe side.
      break;
  }

  callback.Run(value);
}

void TpmChallengeKeyImpl::GetDeviceAttestationEnabledCallback(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kDevicePolicyDisabledError));
    return;
  }

  PrepareKey();
}

void TpmChallengeKeyImpl::PrepareKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CryptohomeClient::Get()->TpmAttestationIsPrepared(
      base::BindOnce(&TpmChallengeKeyImpl::IsAttestationPreparedCallback,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::IsAttestationPreparedCallback(
    base::Optional<bool> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kDbusError));
    return;
  }
  if (!result.value()) {
    CryptohomeClient::Get()->TpmIsEnabled(
        base::BindOnce(&TpmChallengeKeyImpl::PrepareKeyErrorHandlerCallback,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (!key_name_for_spkac_.empty()) {
    // Generate a new key and have it signed by PCA.
    attestation_flow_->GetCertificate(
        GetCertificateProfile(), GetAccountId(),
        std::string(),  // Not used.
        true,           // Force a new key to be generated.
        key_name_for_spkac_,
        base::BindRepeating(&TpmChallengeKeyImpl::GetCertificateCallback,
                            weak_factory_.GetWeakPtr()));
    return;
  }

  // Attestation is available, see if the key we need already exists.
  CryptohomeClient::Get()->TpmAttestationDoesKeyExist(
      key_type_,
      cryptohome::CreateAccountIdentifierFromAccountId(GetAccountId()),
      GetKeyName(),
      base::BindRepeating(&TpmChallengeKeyImpl::DoesKeyExistCallback,
                          weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::PrepareKeyErrorHandlerCallback(
    base::Optional<bool> is_tpm_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_tpm_enabled.has_value()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kDbusError));
    return;
  }

  if (is_tpm_enabled.value()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kResetRequiredError));
  } else {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kAttestationUnsupportedError));
  }
}

void TpmChallengeKeyImpl::DoesKeyExistCallback(base::Optional<bool> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kDbusError));
    return;
  }

  if (result.value()) {
    // The key exists. Do nothing more.
    PrepareKeyFinished();
    return;
  }

  // The key does not exist. Create a new key and have it signed by PCA.
  if (IsUserConsentRequired()) {
    // We should ask the user explicitly before sending any private
    // information to PCA.
    AskForUserConsent(
        base::BindOnce(&TpmChallengeKeyImpl::AskForUserConsentCallback,
                       weak_factory_.GetWeakPtr()));
  } else {
    // User consent is not required. Skip to the next step.
    AskForUserConsentCallback(true);
  }
}

void TpmChallengeKeyImpl::AskForUserConsent(
    base::OnceCallback<void(bool)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(davidyu): right now we just simply reject the request before we have
  // a way to ask for user consent.
  std::move(callback).Run(false);
}

void TpmChallengeKeyImpl::AskForUserConsentCallback(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result) {
    // The user rejects the request.
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kUserRejectedError));
    return;
  }

  // Generate a new key and have it signed by PCA.
  attestation_flow_->GetCertificate(
      GetCertificateProfile(), GetAccountId(),
      std::string(),  // Not used.
      true,           // Force a new key to be generated.
      std::string(),  // Leave key name empty to generate a default name.
      base::BindRepeating(&TpmChallengeKeyImpl::GetCertificateCallback,
                          weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeyImpl::GetCertificateCallback(
    AttestationStatus status,
    const std::string& pem_certificate_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != ATTESTATION_SUCCESS) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kGetCertificateFailedError));
    return;
  }

  PrepareKeyFinished();
}

void TpmChallengeKeyImpl::PrepareKeyFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Everything is checked. Sign the challenge.
  cryptohome::AsyncMethodCaller::GetInstance()
      ->TpmAttestationSignEnterpriseChallenge(
          key_type_, cryptohome::Identification(GetAccountId()), GetKeyName(),
          GetEmail(), InstallAttributes::Get()->GetDeviceId(),
          register_key_ ? CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY
                        : CHALLENGE_OPTION_NONE,
          challenge_, key_name_for_spkac_,
          base::BindRepeating(&TpmChallengeKeyImpl::SignChallengeCallback,
                              weak_factory_.GetWeakPtr(), register_key_));
}

void TpmChallengeKeyImpl::SignChallengeCallback(bool register_key,
                                                bool success,
                                                const std::string& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kSignChallengeFailedError));
    return;
  }

  if (register_key) {
    cryptohome::AsyncMethodCaller::GetInstance()->TpmAttestationRegisterKey(
        key_type_, cryptohome::Identification(GetAccountId()),
        GetKeyNameForRegister(),
        base::BindRepeating(&TpmChallengeKeyImpl::RegisterKeyCallback,
                            weak_factory_.GetWeakPtr(), response));
  } else {
    RegisterKeyCallback(response, true, cryptohome::MOUNT_ERROR_NONE);
  }
}

void TpmChallengeKeyImpl::RegisterKeyCallback(
    const std::string& response,
    bool success,
    cryptohome::MountError return_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    std::move(callback_).Run(TpmChallengeKeyResult::MakeError(
        TpmChallengeKeyResultCode::kKeyRegistrationFailedError));
    return;
  }
  std::move(callback_).Run(TpmChallengeKeyResult::MakeResult(response));
}

}  // namespace attestation
}  // namespace chromeos
