// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository.h"

#include "ash/test/ash_test_base.h"
#include "base/base64.h"

namespace {

std::string Base64Decode(const std::string& encoded) {
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

}  // namespace

namespace ash::quick_pair {

class FastPairRepositoryTest : public AshTestBase {};

TEST_F(FastPairRepositoryTest, TestSHA256HashFunction) {
  const char kTestClassicAddress[] = "04:CB:88:1E:56:19";
  const char kBase64ExpectedSha256Hash[] =
      "gVzzRtZjwYv8lO8xwWnWW2uw/stV6RdEUhv3cIN3nH4=";
  const char kBase64AccountKey[] = "BAcDiEH56/Mq3hW7OKUctA==";

  EXPECT_EQ(FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
                Base64Decode(kBase64AccountKey), kTestClassicAddress),
            Base64Decode(kBase64ExpectedSha256Hash));
}

}  // namespace ash::quick_pair
