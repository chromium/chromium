// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/mock_tpm_challenge_key.h"

#include <utility>

using ::testing::Invoke;
using ::testing::WithArgs;

namespace chromeos {
namespace attestation {

MockTpmChallengeKey::MockTpmChallengeKey() = default;
MockTpmChallengeKey::~MockTpmChallengeKey() = default;

void MockTpmChallengeKey::EnableFake() {
  ON_CALL(*this, BuildResponse)
      .WillByDefault(WithArgs<2>(
          Invoke(this, &MockTpmChallengeKey::FakeBuildResponseSuccess)));
}

void MockTpmChallengeKey::FakeBuildResponseSuccess(
    TpmChallengeKeyCallback callback) {
  std::move(callback).Run(TpmChallengeKeyResult::MakeResult("response"));
}

}  // namespace attestation
}  // namespace chromeos
