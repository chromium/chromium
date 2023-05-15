// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"

#include <array>

#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

struct AttestationResultTest {
  DTAttestationResult result;
  bool is_success;
};

TEST(CommonTypes, AttestationResults) {
  std::array<AttestationResultTest, 12> result_values = {{
      {DTAttestationResult::kMissingCoreSignals, /*is_success*/ false},
      {DTAttestationResult::kMissingSigningKey, /*is_success*/ false},
      {DTAttestationResult::kBadChallengeFormat, /*is_success*/ false},
      {DTAttestationResult::kBadChallengeSource, /*is_success*/ false},
      {DTAttestationResult::kFailedToSerializeKeyInfo, /*is_success*/ false},
      {DTAttestationResult::kFailedToGenerateResponse, /*is_success*/ false},
      {DTAttestationResult::kFailedToSignResponse, /*is_success*/ false},
      {DTAttestationResult::kFailedToSerializeResponse, /*is_success*/ false},
      {DTAttestationResult::kEmptySerializedResponse, /*is_success*/ false},
      {DTAttestationResult::kSuccess, /*is_success*/ true},
      {DTAttestationResult::kFailedToSerializeSignals, /*is_success*/ false},
      {DTAttestationResult::kSuccessNoSignature, /*is_success*/ true},
  }};

  for (const auto& test : result_values) {
    EXPECT_EQ(AttestationErrorToString(test.result).empty(), test.is_success);
    EXPECT_EQ(IsSuccessAttestationResult(test.result), test.is_success);
  }
}

}  // namespace enterprise_connectors
