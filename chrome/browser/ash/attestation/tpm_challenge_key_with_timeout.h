// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_WITH_TIMEOUT_H_
#define CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_WITH_TIMEOUT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"

namespace ash {
namespace attestation {

// Adds timeout restriction to |TpmChallengeKey| class. Can be safely deleted
// to cancel both timeout and challenge key tasks.
class TpmChallengeKeyWithTimeout final {
 public:
  // This object should live as long as result from |BuildResponse| via callback
  // is expected.
  TpmChallengeKeyWithTimeout();
  TpmChallengeKeyWithTimeout(const TpmChallengeKeyWithTimeout&) = delete;
  TpmChallengeKeyWithTimeout& operator=(const TpmChallengeKeyWithTimeout&) =
      delete;
  ~TpmChallengeKeyWithTimeout();

  // Tries to build a response for the |challenge|. Returns either timeout
  // error or result from |TpmChallengeKey::BuildResponse| via |callback|.
  void BuildResponse(base::TimeDelta timeout,
                     ::attestation::VerifiedAccessFlow flow_type,
                     Profile* profile,
                     TpmChallengeKeyCallback callback,
                     const std::string& challenge,
                     bool register_key,
                     ::attestation::KeyType key_crypto_type,
                     const std::string& key_name_for_spkac,
                     const std::optional<std::string>& signals);

 private:
  void ResolveCallback(const TpmChallengeKeyResult& result);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<TpmChallengeKey> challenger_;
  TpmChallengeKeyCallback callback_;
  base::WeakPtrFactory<TpmChallengeKeyWithTimeout> weak_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_WITH_TIMEOUT_H_
