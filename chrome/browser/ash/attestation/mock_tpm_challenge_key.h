// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_

#include <string>

#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace attestation {

class MockTpmChallengeKey : public TpmChallengeKey {
 public:
  MockTpmChallengeKey();
  ~MockTpmChallengeKey() override;

  void EnableFake();

  void EnableFakeError(TpmChallengeKeyResultCode error_code);

  MOCK_METHOD(void,
              BuildResponse,
              (::attestation::VerifiedAccessFlow flow_type,
               Profile* profile,
               TpmChallengeKeyCallback callback,
               const std::string& challenge,
               bool register_key,
               ::attestation::KeyType key_crypto_type,
               const std::string& key_name_for_spkac,
               const std::optional<std::string>& signals),
              (override));

  void FakeBuildResponseSuccess(TpmChallengeKeyCallback callback);
  void FakeBuildResponseError(TpmChallengeKeyCallback callback,
                              TpmChallengeKeyResultCode error_code);
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MOCK_TPM_CHALLENGE_KEY_H_
