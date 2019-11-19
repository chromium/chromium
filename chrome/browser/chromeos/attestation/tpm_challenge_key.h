// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_result.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace attestation {

//========================= TpmChallengeKeyFactory =============================

class TpmChallengeKey;

class TpmChallengeKeyFactory {
 public:
  static std::unique_ptr<TpmChallengeKey> Create();
  static void SetForTesting(std::unique_ptr<TpmChallengeKey> next_result);

 private:
  static TpmChallengeKey* next_result_for_testing_;
};

//=========================== TpmChallengeKey ==================================

using TpmChallengeKeyCallback =
    base::OnceCallback<void(const TpmChallengeKeyResult& result)>;

// Asynchronously run the flow to challenge a key in the caller context.
class TpmChallengeKey {
 public:
  TpmChallengeKey(const TpmChallengeKey&) = delete;
  TpmChallengeKey& operator=(const TpmChallengeKey&) = delete;
  virtual ~TpmChallengeKey() = default;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Should be called only once for every instance. |TpmChallengeKey| object
  // should live as long as response from |BuildResponse| function via
  // |callback| is expected. On destruction it stops challenge process and
  // silently discards callback. |key_name_for_spkac| the name of the key used
  // for SignedPublicKeyAndChallenge when sending a challenge machine key
  // request with |registerKey|=true.
  virtual void BuildResponse(AttestationKeyType key_type,
                             Profile* profile,
                             TpmChallengeKeyCallback callback,
                             const std::string& challenge,
                             bool register_key,
                             const std::string& key_name_for_spkac) = 0;

 protected:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKey() = default;
};

//=========================== TpmChallengeKeyImpl ==============================

class TpmChallengeKeyImpl : public TpmChallengeKey {
 public:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKeyImpl();
  // Use only for testing.
  explicit TpmChallengeKeyImpl(AttestationFlow* attestation_flow_for_testing);
  TpmChallengeKeyImpl(const TpmChallengeKeyImpl&) = delete;
  TpmChallengeKeyImpl& operator=(const TpmChallengeKeyImpl&) = delete;
  ~TpmChallengeKeyImpl() override;

  // TpmChallengeKey
  void BuildResponse(AttestationKeyType key_type,
                     Profile* profile,
                     TpmChallengeKeyCallback callback,
                     const std::string& challenge,
                     bool register_key,
                     const std::string& key_name_for_spkac) override;

 private:
  void ChallengeUserKey();
  void ChallengeMachineKey();

  // Returns true if the user is managed and is affiliated with the domain the
  // device is enrolled to.
  bool IsUserAffiliated() const;
  // Returns true if remote attestation is allowed and the setting is managed.
  bool IsRemoteAttestationEnabledForUser() const;

  // Returns the enterprise domain the device is enrolled to or user email.
  std::string GetEmail() const;
  const char* GetKeyName() const;
  AttestationCertificateProfile GetCertificateProfile() const;
  std::string GetKeyNameForRegister() const;
  const user_manager::User* GetUser() const;
  AccountId GetAccountId() const;

  // Prepares the key for signing. It will first check if a new key should be
  // generated, i.e. |key_name_for_spkac_| is not empty or the key doesn't exist
  // and, if necessary, call AttestationFlow::GetCertificate() to get a new one.
  // If |IsUserConsentRequired()| is true, it will explicitly ask for user
  // consent before calling GetCertificate().
  void PrepareKey();
  void PrepareKeyFinished();

  void SignChallengeCallback(bool register_key,
                             bool success,
                             const std::string& response);
  void RegisterKeyCallback(const std::string& response,
                           bool success,
                           cryptohome::MountError return_code);
  // Returns a trusted value from CrosSettings indicating if the device
  // attestation is enabled.
  void GetDeviceAttestationEnabled(
      const base::RepeatingCallback<void(bool)>& callback);
  void GetDeviceAttestationEnabledCallback(bool enabled);

  void IsAttestationPreparedCallback(base::Optional<bool> result);
  void PrepareKeyErrorHandlerCallback(base::Optional<bool> is_tpm_enabled);
  void DoesKeyExistCallback(base::Optional<bool> result);
  void AskForUserConsent(base::OnceCallback<void(bool)> callback) const;
  void AskForUserConsentCallback(bool result);
  void GetCertificateCallback(AttestationStatus status,
                              const std::string& pem_certificate_chain);

  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  AttestationFlow* attestation_flow_ = nullptr;

  TpmChallengeKeyCallback callback_;
  Profile* profile_ = nullptr;

  AttestationKeyType key_type_ = AttestationKeyType::KEY_DEVICE;
  std::string challenge_;

  bool register_key_ = false;
  std::string key_name_for_spkac_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TpmChallengeKeyImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_H_
