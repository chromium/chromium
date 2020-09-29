// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key_subtle.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
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
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace attestation {

using Result = TpmChallengeKeyResult;
using ResultCode = TpmChallengeKeyResultCode;

//==================== TpmChallengeKeySubtleFactory ============================

TpmChallengeKeySubtle* TpmChallengeKeySubtleFactory::next_result_for_testing_ =
    nullptr;

// static
std::unique_ptr<TpmChallengeKeySubtle> TpmChallengeKeySubtleFactory::Create() {
  if (UNLIKELY(next_result_for_testing_)) {
    std::unique_ptr<TpmChallengeKeySubtle> result(next_result_for_testing_);
    next_result_for_testing_ = nullptr;
    return result;
  }

  return std::make_unique<TpmChallengeKeySubtleImpl>();
}

// static
std::unique_ptr<TpmChallengeKeySubtle>
TpmChallengeKeySubtleFactory::CreateForPreparedKey(AttestationKeyType key_type,
                                                   bool will_register_key,
                                                   const std::string& key_name,
                                                   Profile* profile) {
  auto result = TpmChallengeKeySubtleFactory::Create();
  result->RestorePreparedKeyState(key_type, will_register_key, key_name,
                                  profile);
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

// For personal devices, we don't need to check if remote attestation is
// enabled in the device, but we need to ask for user consent if the key
// does not exist.
bool IsUserConsentRequired() {
  return !IsEnterpriseDevice();
}

// If no key name was given, use default well-known key names so they can be
// reused across attestation operations (multiple challenge responses can be
// generated using the same key).
std::string GetDefaultKeyName(AttestationKeyType key_type) {
  switch (key_type) {
    case KEY_DEVICE:
      return kEnterpriseMachineKey;
    case KEY_USER:
      return kEnterpriseUserKey;
  }
  NOTREACHED();
}

// Returns the key name that should be used for the attestation platform APIs.
std::string GetKeyNameWithDefault(AttestationKeyType key_type,
                                  const std::string& key_name) {
  if (!key_name.empty())
    return key_name;

  return GetDefaultKeyName(key_type);
}

}  // namespace

TpmChallengeKeySubtleImpl::TpmChallengeKeySubtleImpl()
    : default_attestation_flow_(std::make_unique<AttestationFlow>(
          cryptohome::AsyncMethodCaller::GetInstance(),
          CryptohomeClient::Get(),
          std::make_unique<AttestationCAClient>())),
      attestation_flow_(default_attestation_flow_.get()) {}

TpmChallengeKeySubtleImpl::TpmChallengeKeySubtleImpl(
    AttestationFlow* attestation_flow_for_testing)
    : attestation_flow_(attestation_flow_for_testing) {}

TpmChallengeKeySubtleImpl::~TpmChallengeKeySubtleImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TpmChallengeKeySubtleImpl::RestorePreparedKeyState(
    AttestationKeyType key_type,
    bool will_register_key,
    const std::string& key_name,
    Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // For user keys, a |profile| is strictly necessary.
  DCHECK(key_type != KEY_USER || profile);

  key_type_ = key_type;
  will_register_key_ = will_register_key;
  key_name_ = GetKeyNameWithDefault(key_type, key_name);
  profile_ = profile;
}

void TpmChallengeKeySubtleImpl::StartPrepareKeyStep(
    AttestationKeyType key_type,
    bool will_register_key,
    const std::string& key_name,
    Profile* profile,
    TpmChallengeKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  // For device key: if |will_register_key| is true, |key_name| should not be
  // empty, if |register_key| is false, |key_name| will not be used.
  DCHECK((key_type != KEY_DEVICE) || (will_register_key == !key_name.empty()))
      << "Invalid arguments: " << will_register_key << " " << !key_name.empty();

  // For user keys, a |profile| is strictly necessary.
  DCHECK(key_type != KEY_USER || profile);

  key_type_ = key_type;
  will_register_key_ = will_register_key;
  key_name_ = GetKeyNameWithDefault(key_type, key_name);
  profile_ = profile;
  callback_ = std::move(callback);

  switch (key_type_) {
    case KEY_DEVICE:
      PrepareMachineKey();
      return;
    case KEY_USER:
      PrepareUserKey();
      return;
  }
  NOTREACHED();
}

void TpmChallengeKeySubtleImpl::PrepareMachineKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if the device is enterprise enrolled.
  if (!IsEnterpriseDevice()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kNonEnterpriseDeviceError));
    return;
  }

  // Check whether the user is managed unless this is a device-wide instance.
  if (GetUser() && !IsUserAffiliated()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserNotManagedError));
    return;
  }

  // Check if remote attestation is enabled in the device policy.
  GetDeviceAttestationEnabled(base::BindRepeating(
      &TpmChallengeKeySubtleImpl::GetDeviceAttestationEnabledCallback,
      weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::PrepareUserKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check if user keys are available in this profile.
  if (!GetUser()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserKeyNotAvailableError));
    return;
  }

  if (!IsRemoteAttestationEnabledForUser()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kUserPolicyDisabledError));
    return;
  }

  if (IsEnterpriseDevice()) {
    if (!IsUserAffiliated()) {
      std::move(callback_).Run(
          Result::MakeError(ResultCode::kUserNotManagedError));
      return;
    }

    // Check if remote attestation is enabled in the device policy.
    GetDeviceAttestationEnabled(base::BindRepeating(
        &TpmChallengeKeySubtleImpl::GetDeviceAttestationEnabledCallback,
        weak_factory_.GetWeakPtr()));
  } else {
    GetDeviceAttestationEnabledCallback(true);
  }
}

bool TpmChallengeKeySubtleImpl::IsUserAffiliated() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const user_manager::User* const user = GetUser();
  if (user) {
    return user->IsAffiliated();
  }
  return false;
}

bool TpmChallengeKeySubtleImpl::IsRemoteAttestationEnabledForUser() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);

  PrefService* prefs = profile_->GetPrefs();
  if (prefs && prefs->IsManagedPreference(prefs::kAttestationEnabled)) {
    return prefs->GetBoolean(prefs::kAttestationEnabled);
  }
  return false;
}

std::string TpmChallengeKeySubtleImpl::GetEmail() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (key_type_) {
    case KEY_DEVICE:
      return InstallAttributes::Get()->GetDomain();
    case KEY_USER:
      return GetAccountId().GetUserEmail();
  }
  NOTREACHED();
}

AttestationCertificateProfile TpmChallengeKeySubtleImpl::GetCertificateProfile()
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

void TpmChallengeKeySubtleImpl::GetDeviceAttestationEnabled(
    const base::RepeatingCallback<void(bool)>& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CrosSettings* settings = CrosSettings::Get();
  CrosSettingsProvider::TrustedStatus status = settings->PrepareTrustedValues(
      base::BindOnce(&TpmChallengeKeySubtleImpl::GetDeviceAttestationEnabled,
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

void TpmChallengeKeySubtleImpl::GetDeviceAttestationEnabledCallback(
    bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!enabled) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kDevicePolicyDisabledError));
    return;
  }

  PrepareKey();
}

void TpmChallengeKeySubtleImpl::PrepareKey() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CryptohomeClient::Get()->TpmAttestationIsPrepared(
      base::BindOnce(&TpmChallengeKeySubtleImpl::IsAttestationPreparedCallback,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::IsAttestationPreparedCallback(
    base::Optional<bool> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback_).Run(Result::MakeError(ResultCode::kDbusError));
    return;
  }

  if (!result.value()) {
    CryptohomeClient::Get()->TpmIsEnabled(base::BindOnce(
        &TpmChallengeKeySubtleImpl::PrepareKeyErrorHandlerCallback,
        weak_factory_.GetWeakPtr()));
    return;
  }

  // Attestation is available, see if the key we need already exists.
  CryptohomeClient::Get()->TpmAttestationDoesKeyExist(
      key_type_,
      cryptohome::CreateAccountIdentifierFromAccountId(GetAccountId()),
      key_name_,
      base::BindOnce(&TpmChallengeKeySubtleImpl::DoesKeyExistCallback,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::PrepareKeyErrorHandlerCallback(
    base::Optional<bool> is_tpm_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_tpm_enabled.has_value()) {
    std::move(callback_).Run(Result::MakeError(ResultCode::kDbusError));
    return;
  }

  if (is_tpm_enabled.value()) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kResetRequiredError));
  } else {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kAttestationUnsupportedError));
  }
}

void TpmChallengeKeySubtleImpl::DoesKeyExistCallback(
    base::Optional<bool> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    std::move(callback_).Run(Result::MakeError(ResultCode::kDbusError));
    return;
  }

  if (result.value()) {
    // The key exists. Do nothing more.
    GetPublicKey();
    return;
  }

  // The key does not exist. Create a new key and have it signed by PCA.
  if (IsUserConsentRequired()) {
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
      GetCertificateProfile(), GetAccountId(),
      /*request_origin=*/std::string(),  // Not used.
      /*force_new_key=*/true, key_name_,
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

  CryptohomeClient::Get()->TpmAttestationGetPublicKey(
      key_type_,
      cryptohome::CreateAccountIdentifierFromAccountId(GetAccountId()),
      key_name_,
      base::BindOnce(&TpmChallengeKeySubtleImpl::PrepareKeyFinished,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::PrepareKeyFinished(
    base::Optional<CryptohomeClient::TpmAttestationDataResult>
        prepare_key_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prepare_key_result.has_value() || !prepare_key_result->success) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kGetPublicKeyFailedError));
    return;
  }

  std::move(callback_).Run(Result::MakePublicKey(prepare_key_result->data));
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
  // Device key challenges are signed using a stable key.
  std::string key_name_for_challenge =
      (key_type_ == KEY_DEVICE) ? GetDefaultKeyName(key_type_) : key_name_;
  // Name of the key that will be included in SPKAC, it is used only when SPKAC
  // should be included for device key.
  std::string key_name_for_spkac =
      (will_register_key_ && key_type_ == KEY_DEVICE) ? key_name_
                                                      : std::string();

  // Everything is checked. Sign the challenge.
  cryptohome::AsyncMethodCaller::GetInstance()
      ->TpmAttestationSignEnterpriseChallenge(
          key_type_, cryptohome::Identification(GetAccountId()),
          key_name_for_challenge, GetEmail(),
          InstallAttributes::Get()->GetDeviceId(),
          will_register_key_ ? CHALLENGE_INCLUDE_SIGNED_PUBLIC_KEY
                             : CHALLENGE_OPTION_NONE,
          challenge, key_name_for_spkac,
          base::BindOnce(&TpmChallengeKeySubtleImpl::SignChallengeCallback,
                         weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::SignChallengeCallback(
    bool success,
    const std::string& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kSignChallengeFailedError));
    return;
  }

  std::move(callback_).Run(Result::MakeChallengeResponse(response));
}

void TpmChallengeKeySubtleImpl::StartRegisterKeyStep(
    TpmChallengeKeyCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(will_register_key_);

  callback_ = std::move(callback);

  cryptohome::AsyncMethodCaller::GetInstance()->TpmAttestationRegisterKey(
      key_type_, cryptohome::Identification(GetAccountId()), key_name_,
      base::BindOnce(&TpmChallengeKeySubtleImpl::RegisterKeyCallback,
                     weak_factory_.GetWeakPtr()));
}

void TpmChallengeKeySubtleImpl::RegisterKeyCallback(
    bool success,
    cryptohome::MountError return_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!success || return_code != cryptohome::MOUNT_ERROR_NONE) {
    std::move(callback_).Run(
        Result::MakeError(ResultCode::kKeyRegistrationFailedError));
    return;
  }

  std::move(callback_).Run(Result::MakeSuccess());
}

}  // namespace attestation
}  // namespace chromeos
