// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_

#include <string>

#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace attestation {

class MockTpmChallengeKeySubtle : public TpmChallengeKeySubtle {
 public:
  MockTpmChallengeKeySubtle();
  MockTpmChallengeKeySubtle(const MockTpmChallengeKeySubtle&) = delete;
  MockTpmChallengeKeySubtle& operator=(const MockTpmChallengeKeySubtle&) =
      delete;
  ~MockTpmChallengeKeySubtle() override;

  MOCK_METHOD(void,
              StartPrepareKeyStep,
              (::attestation::VerifiedAccessFlow flow_type,
               bool will_register_key,
               ::attestation::KeyType key_crypto_type,
               const std::string& key_name,
               Profile* profile,
               TpmChallengeKeyCallback callback,
               const std::optional<std::string>& signals),
              (override));

  MOCK_METHOD(void,
              StartSignChallengeStep,
              (const std::string& challenge, TpmChallengeKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              StartRegisterKeyStep,
              (TpmChallengeKeyCallback callback),
              (override));

  MOCK_METHOD(void,
              RestorePreparedKeyState,
              (::attestation::VerifiedAccessFlow flow_type,
               bool will_register_key,
               ::attestation::KeyType key_crypto_type,
               const std::string& key_name,
               const std::string& public_key,
               Profile* profile),
              (override));
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_SUBTLE_H_
