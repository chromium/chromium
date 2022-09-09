// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"

#include <utility>

using ::testing::Invoke;
using ::testing::WithArgs;

namespace ash {
namespace attestation {

MockTpmChallengeKey::MockTpmChallengeKey() = default;
MockTpmChallengeKey::~MockTpmChallengeKey() = default;

void MockTpmChallengeKey::EnableFake() {
  ON_CALL(*this, BuildResponse)
      .WillByDefault(WithArgs<2>(
          Invoke(this, &MockTpmChallengeKey::FakeBuildResponseSuccess)));
}

void MockTpmChallengeKey::EnableFakeError(
    TpmChallengeKeyResultCode error_code) {
  ON_CALL(*this, BuildResponse)
      .WillByDefault(WithArgs<2>(
          Invoke([this, error_code](TpmChallengeKeyCallback callback) {
            MockTpmChallengeKey::FakeBuildResponseError(std::move(callback),
                                                        error_code);
          })));
}

void MockTpmChallengeKey::FakeBuildResponseSuccess(
    TpmChallengeKeyCallback callback) {
  std::move(callback).Run(
      TpmChallengeKeyResult::MakeChallengeResponse("response"));
}

void MockTpmChallengeKey::FakeBuildResponseError(
    TpmChallengeKeyCallback callback,
    TpmChallengeKeyResultCode error_code) {
  std::move(callback).Run(TpmChallengeKeyResult::MakeError(error_code));
}

}  // namespace attestation
}  // namespace ash
