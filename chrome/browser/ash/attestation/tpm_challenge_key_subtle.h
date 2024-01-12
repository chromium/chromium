// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_
#define CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

class Profile;

namespace ash {
namespace attestation {

class MachineCertificateUploader;

//==================== TpmChallengeKeySubtleFactory ============================

class TpmChallengeKeySubtle;

class TpmChallengeKeySubtleFactory final {
 public:
  static std::unique_ptr<TpmChallengeKeySubtle> Create();

  // Recreates an object as it would be after |StartPrepareKeyStep| method call.
  // It is the caller's responsibility to guarantee that |StartPrepareKeyStep|
  // has successfully finished before and that only one call of
  // |StartSignChallengeStep| and/or |StartRegisterKeyStep| for a prepared key
  // pair will ever happen.
  // |profile| may be nullptr - then it is assumed that this is a device-wide
  // instance that is only intended to be used with machine keys.
  static std::unique_ptr<TpmChallengeKeySubtle> CreateForPreparedKey(
      ::attestation::VerifiedAccessFlow flow_type,
      bool will_register_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::string& public_key,
      Profile* profile);

  static void SetForTesting(std::unique_ptr<TpmChallengeKeySubtle> next_result);
  static bool WillReturnTestingInstance();

 private:
  static TpmChallengeKeySubtle* next_result_for_testing_;
};

//===================== TpmChallengeKeySubtle ==================================

using TpmChallengeKeyCallback =
    base::OnceCallback<void(const TpmChallengeKeyResult& result)>;

// Asynchronously runs the flow to challenge a key in the caller context.
// Consider using |TpmChallengeKey| class for simple cases.
// This class provides a detailed API for calculating Verified Access challenge
// response and manipulating keys that are used for that.
//
// The order of calling methods is important. Expected usage:
// 1. |StartPrepareKeyStep| should always be called first.
// 2. After that, if the object is destroyed, it can be recreated by using
// |TpmChallengeKeySubtleFactory::CreateForPreparedKey|.
// 3. |StartSignChallengeStep| allows to calculate challenge response, can be
// skipped.
// 4. As a last step, |StartRegisterKeyStep| allows change key type so it cannot
// sign challenges anymore, but can be used for general puprose cryptographic
// operations (via PlatformKeysService).
class TpmChallengeKeySubtle {
 public:
  TpmChallengeKeySubtle(const TpmChallengeKeySubtle&) = delete;
  TpmChallengeKeySubtle& operator=(const TpmChallengeKeySubtle&) = delete;
  virtual ~TpmChallengeKeySubtle() = default;

  // Checks that it is allowed to generate a VA challenge response and generates
  // a new key pair if necessary. Returns result via |callback|. In case of
  // success |TpmChallengeKeyResult::public_key| will be filled. If
  // |will_register_key| is true, challenge response will contain SPKAC and the
  // key can be registered using StartRegisterKeyStep method.
  virtual void StartPrepareKeyStep(
      ::attestation::VerifiedAccessFlow flow_type,
      bool will_register_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      Profile* profile,
      TpmChallengeKeyCallback callback,
      const std::optional<std::string>& signals) = 0;

  // Generates a VA challenge response using the key pair prepared by
  // |PrepareKey| method. Returns VA challenge response via |callback|. In case
  // of success |TpmChallengeKeyResult::challenge_response| will be filled.
  virtual void StartSignChallengeStep(const std::string& challenge,
                                      TpmChallengeKeyCallback callback) = 0;

  // Registers the key that makes it available for general purpose cryptographic
  // operations.
  virtual void StartRegisterKeyStep(TpmChallengeKeyCallback callback) = 0;

 protected:
  // Allow access to |RestorePrepareKeyResult| method.
  friend class TpmChallengeKeySubtleFactory;

  // Use TpmChallengeKeySubtleFactory for creation.
  TpmChallengeKeySubtle() = default;

  // Restores internal state of the object as if it would be after
  // |StartPrepareKeyStep|. |public_key| is required only if |will_register_key|
  // is true.
  virtual void RestorePreparedKeyState(
      ::attestation::VerifiedAccessFlow flow_type,
      bool will_register_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const std::string& public_key,
      Profile* profile) = 0;
};

//================= TpmChallengeKeySubtleImpl ==================================

class TpmChallengeKeySubtleImpl final : public TpmChallengeKeySubtle {
 public:
  // Use TpmChallengeKeySubtleFactory for creation.
  TpmChallengeKeySubtleImpl();
  // Use only for testing.
  TpmChallengeKeySubtleImpl(
      AttestationFlow* attestation_flow_for_testing,
      MachineCertificateUploader* certificate_uploader_for_testing);

  TpmChallengeKeySubtleImpl(const TpmChallengeKeySubtleImpl&) = delete;
  TpmChallengeKeySubtleImpl& operator=(const TpmChallengeKeySubtleImpl&) =
      delete;
  ~TpmChallengeKeySubtleImpl() override;

  // TpmChallengeKeySubtle
  void StartPrepareKeyStep(::attestation::VerifiedAccessFlow flow_type,
                           bool will_register_key,
                           ::attestation::KeyType key_crypto_type,
                           const std::string& key_name,
                           Profile* profile,
                           TpmChallengeKeyCallback callback,
                           const std::optional<std::string>& signals) override;
  void StartSignChallengeStep(const std::string& challenge,
                              TpmChallengeKeyCallback callback) override;
  void StartRegisterKeyStep(TpmChallengeKeyCallback callback) override;

 private:
  // TpmChallengeKeySubtle
  void RestorePreparedKeyState(::attestation::VerifiedAccessFlow flow_type,
                               bool will_register_key,
                               ::attestation::KeyType key_crypto_type,
                               const std::string& key_name,
                               const std::string& public_key,
                               Profile* profile) override;

  void PrepareEnterpriseUserFlow();
  void PrepareEnterpriseMachineFlow();
  void PrepareDeviceTrustConnectorFlow();

  // Returns true if the user is managed.
  // If this is a device-wide instance without a user-associated `profile_`,
  // returns false.
  bool IsUserManaged() const;

  // Returns true if the user is managed and is affiliated with the domain the
  // device is enrolled to.
  // If this is a device-wide instance without a user-associated |profile_|,
  // returns false.
  bool IsUserAffiliated() const;

  // Returns the user email (for user key) or an empty string (for machine key).
  std::string GetEmail() const;
  AttestationCertificateProfile GetCertificateProfile() const;
  // Returns the User* associated with |profile_|. May return nullptr (if there
  // is no |profile_| or if e.g. |profile_| is a sign-in profile).
  const user_manager::User* GetUser() const;
  // Returns the AccountId associated with |profile_|. Will return
  // EmptyAccountId() if GetUser() returns nullptr.
  AccountId GetAccountId() const;
  // Returns `GetAccountId()` if the flow type uses a user key, returns empty
  // `AccountId` if the flow type uses a device key.
  AccountId GetAccountIdForAttestationFlow() const;
  // Returns the account id in string if the flow type uses a user key, returns
  // an empty string if the flow type uses a device key.
  std::string GetUsernameForAttestationClient() const;
  // Returns whether or not the challenge response should include the
  // certificate of the signing key depending on the VA flow type.
  bool ShouldIncludeSigningKeyCertificate() const;
  // Returns whether or not the challenge response should include the device
  // management / enterprise obfuscated customer ID of the device.
  bool ShouldIncludeCustomerId() const;

  // Actually prepares a key after all checks are passed and if `can_continue`
  // is true.
  void PrepareKey(bool can_continue);
  // Returns a public key (or an error) via `callback_`.
  void PrepareKeyFinished(const ::attestation::GetKeyInfoReply& reply);

  void SignChallengeCallback(
      const ::attestation::SignEnterpriseChallengeReply& reply);

  void RegisterKeyCallback(
      const ::attestation::RegisterKeyWithChapsTokenReply& reply);
  void MarkCorporateKeyCallback(chromeos::platform_keys::Status status);

  void GetEnrollmentPreparationsCallback(
      const ::attestation::GetEnrollmentPreparationsReply& reply);
  void PrepareKeyErrorHandlerCallback(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);
  void DoesKeyExistCallback(const ::attestation::GetKeyInfoReply& reply);
  void AskForUserConsent(base::OnceCallback<void(bool)> callback) const;
  void AskForUserConsentCallback(bool result);
  void GetCertificateCallback(AttestationStatus status,
                              const std::string& pem_certificate_chain);
  void GetPublicKey();

  // Runs |callback_| and resets it. Resetting it in this function and checking
  // it in public functions prevents simultaneous calls on the same object.
  // |this| may be destructed during the |callback_| run.
  void RunCallback(const TpmChallengeKeyResult& result);

  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  raw_ptr<AttestationFlow, LeakedDanglingUntriaged> attestation_flow_ = nullptr;
  // Can be nullptr.
  raw_ptr<MachineCertificateUploader, LeakedDanglingUntriaged>
      machine_certificate_uploader_ = nullptr;

  TpmChallengeKeyCallback callback_;
  // |profile_| may be nullptr if this is an instance that is used device-wide
  // and only intended to work with machine keys.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  ::attestation::VerifiedAccessFlow flow_type_ =
      ::attestation::ENTERPRISE_MACHINE;
  bool will_register_key_ = false;
  ::attestation::KeyType key_crypto_type_ = ::attestation::KEY_TYPE_RSA;
  // See the comment for TpmChallengeKey::BuildResponse for more context about
  // different cases of using this variable.
  std::string key_name_;
  // In case the key is going to be registered, the public key is stored here
  // (after PrepareKeyFinished method is finished). It is used to mark the key
  // as corporate.
  std::string public_key_;
  // Signals from Context Aware Access.
  std::optional<std::string> signals_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TpmChallengeKeySubtleImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_SUBTLE_H_
