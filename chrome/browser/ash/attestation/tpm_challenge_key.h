// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_H_
#define CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/keystore.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

class Profile;
class AttestationFlow;

namespace ash {
namespace attestation {

// Prefix for naming machine keys used for SignedPublicKeyAndChallenge when
// challenging the EMK with register=true.
const char kEnterpriseMachineKeyForSpkacPrefix[] = "attest-ent-machine-";

//========================= TpmChallengeKeyFactory =============================

class TpmChallengeKey;

class TpmChallengeKeyFactory final {
 public:
  static std::unique_ptr<TpmChallengeKey> Create();
  static void SetForTesting(std::unique_ptr<TpmChallengeKey> next_result);

 private:
  static TpmChallengeKey* next_result_for_testing_;
};

//=========================== TpmChallengeKey ==================================

// Asynchronously runs the flow to challenge a key in the caller context. This
// is a wrapper around TpmChallengeKeySubtle with an easier-to-use interface.
// TpmChallengeKeySubtle can be used directly to get more control over main
// steps to build the response.
class TpmChallengeKey {
 public:
  TpmChallengeKey(const TpmChallengeKey&) = delete;
  TpmChallengeKey& operator=(const TpmChallengeKey&) = delete;
  virtual ~TpmChallengeKey() = default;

  // Should be called only once for every instance. |TpmChallengeKey| object
  // should live as long as response from |BuildResponse| function via
  // |callback| is expected. On destruction it stops challenge process and
  // silently discards callback.
  // The response consists of up to two parts: 1) a response to the challenge
  // and optionally 2) an SPKAC. They can be generated using different keys:
  // A) ENTERPRISE_MACHINE && !register_key
  // => 1) Stable device key + 2) Empty
  // B) ENTERPRISE_MACHINE && register_key
  // => 1) Stable device key + 2) Key(key_name)
  // C) ENTERPRISE_USER && !register_key
  // => 1) Key(key_name) + 2) Empty
  // D) ENTERPRISE_USER && register_key
  // => 1) Key(key_name) + 2) Key(key_name)
  // E) DEVICE_TRUST_CONNECTOR && !register_key
  // => 1) Key(key_name) + 2) Empty
  // In case B) |key_name| cannot be empty. In case C), D) some default name
  // will be used if |key_name| is empty.
  // When using DEVICE_TRUST_CONNECTOR, `register_key` is not supported and
  // `key_name` cannot be empty.
  // The response can also contain |signals| which consist of a set of
  // information about the device that is given to the IdP after the challenge
  // response has been verified. These signals can be used as input to an AuthN
  // decision. Signals are collected in a dictionary and are JSON stringified.
  // The signals are optional since they can be null when no signals are set on
  // the response, empty when no signals were collected (i.e empty signals
  // dictionary), or non empty. More information on signals collection can be
  // found in the |SignalsService|.

  virtual void BuildResponse(::attestation::VerifiedAccessFlow flow_type,
                             Profile* profile,
                             TpmChallengeKeyCallback callback,
                             const std::string& challenge,
                             bool register_key,
                             ::attestation::KeyType key_crypto_type,
                             const std::string& key_name,
                             const std::optional<std::string>& signals) = 0;

 protected:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKey() = default;
};

//=========================== TpmChallengeKeyImpl ==============================

class TpmChallengeKeyImpl final : public TpmChallengeKey {
 public:
  // Use TpmChallengeKeyFactory for creation.
  TpmChallengeKeyImpl();
  // Use only for testing.
  explicit TpmChallengeKeyImpl(
      AttestationFlow* attestation_flow_for_testing,
      MachineCertificateUploader* certificate_uploader_for_testing);
  TpmChallengeKeyImpl(const TpmChallengeKeyImpl&) = delete;
  TpmChallengeKeyImpl& operator=(const TpmChallengeKeyImpl&) = delete;
  ~TpmChallengeKeyImpl() override;

  // TpmChallengeKey
  void BuildResponse(::attestation::VerifiedAccessFlow flow_type,
                     Profile* profile,
                     TpmChallengeKeyCallback callback,
                     const std::string& challenge,
                     bool register_key,
                     ::attestation::KeyType key_crypto_type,
                     const std::string& key_name,
                     const std::optional<std::string>& signals) override;

 private:
  void OnPrepareKeyDone(const TpmChallengeKeyResult& prepare_key_result);
  void OnSignChallengeDone(const TpmChallengeKeyResult& sign_challenge_result);
  void OnRegisterKeyDone(const TpmChallengeKeyResult& challenge_response,
                         const TpmChallengeKeyResult& register_key_result);

  bool register_key_ = false;
  std::string challenge_;
  TpmChallengeKeyResult challenge_response_;
  TpmChallengeKeyCallback callback_;
  std::unique_ptr<TpmChallengeKeySubtle> tpm_challenge_key_subtle_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TpmChallengeKeyImpl> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_H_
