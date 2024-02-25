// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/authenticator.h"

#include <map>
#include <optional>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace parent_access {

AccessCodeConfig GetZeroClockDriftConfig() {
  return AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                          base::Minutes(0));
}

class ParentAccessCodeAuthenticatorTest : public testing::Test {
 public:
  ParentAccessCodeAuthenticatorTest(const ParentAccessCodeAuthenticatorTest&) =
      delete;
  ParentAccessCodeAuthenticatorTest& operator=(
      const ParentAccessCodeAuthenticatorTest&) = delete;

 protected:
  ParentAccessCodeAuthenticatorTest() = default;
  ~ParentAccessCodeAuthenticatorTest() override = default;

  // Verifies that |code| is valid for the given |timestamp|.
  void Verify(std::optional<AccessCode> code, base::Time timestamp) {
    ASSERT_TRUE(code.has_value());
    EXPECT_GE(timestamp, code->valid_from());
    EXPECT_LE(timestamp, code->valid_to());
  }
};

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateHardcodedCodeValues) {
  // Test generation against Parent Access Code values generated in Family
  // Link Android app.
  std::map<base::Time, std::string> test_values;
  ASSERT_NO_FATAL_FAILURE(GetTestAccessCodeValues(&test_values));

  Authenticator gen(GetDefaultTestConfig());
  for (const auto& it : test_values) {
    std::optional<AccessCode> code = gen.Generate(it.first);
    ASSERT_NO_FATAL_FAILURE(Verify(code, it.first));
    EXPECT_EQ(it.second, code->code());
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateInTheSameTimeBucket) {
  // Test that the same code is generated for whole time bucket defined by code
  // validity period.
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:00:00 PST", &timestamp));
  const AccessCodeConfig config = GetDefaultTestConfig();

  Authenticator gen(GetDefaultTestConfig());
  std::optional<AccessCode> first_code = gen.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(first_code, timestamp));

  int range = base::ClampFloor(config.code_validity() /
                               Authenticator::kAccessCodeGranularity) -
              1;
  for (int i = 0; i < range; ++i) {
    timestamp += Authenticator::kAccessCodeGranularity;
    std::optional<AccessCode> code = gen.Generate(timestamp);
    ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
    EXPECT_EQ(*first_code, *code);
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateInDifferentTimeBuckets) {
  // Test that the different codes are generated for different time buckets
  // defined by code validity period.
  base::Time initial_timestamp;
  ASSERT_TRUE(
      base::Time::FromString("14 Jan 2019 15:00:00 PST", &initial_timestamp));

  Authenticator gen(GetDefaultTestConfig());
  std::optional<AccessCode> first_code = gen.Generate(initial_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(first_code, initial_timestamp));

  for (int i = 1; i < 10; ++i) {
    // "Earlier" time bucket.
    {
      const base::Time timestamp = initial_timestamp - i * kDefaultCodeValidity;
      std::optional<AccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
    // "Later" time bucket.
    {
      const base::Time timestamp = initial_timestamp + i * kDefaultCodeValidity;
      std::optional<AccessCode> code = gen.Generate(timestamp);
      ASSERT_NO_FATAL_FAILURE(Verify(code, timestamp));
      EXPECT_NE(*first_code, *code);
    }
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithSameTimestamp) {
  // Test that codes generated with the same timestamp and config are the same.
  const AccessCodeConfig config = GetDefaultTestConfig();
  const base::Time timestamp = base::Time::Now();

  Authenticator gen1(GetDefaultTestConfig());
  std::optional<AccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  Authenticator gen2(GetDefaultTestConfig());
  std::optional<AccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithDifferentSharedSecret) {
  // Test that codes generated with the different secrets are not the same.
  const base::Time timestamp = base::Time::Now();

  Authenticator gen1(AccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAA", kDefaultCodeValidity, kDefaultClockDrift));
  std::optional<AccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  Authenticator gen2(AccessCodeConfig(
      "AAAAAAAAAAAAAAAAAAB", kDefaultCodeValidity, kDefaultClockDrift));
  std::optional<AccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, GenerateWithDifferentCodeValidity) {
  // Test that codes generated with the different validity are not the same.
  const base::Time timestamp = base::Time::Now();

  Authenticator gen1(AccessCodeConfig(kTestSharedSecret, base::Minutes(1),
                                      kDefaultClockDrift));
  std::optional<AccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  Authenticator gen2(AccessCodeConfig(kTestSharedSecret, base::Minutes(3),
                                      kDefaultClockDrift));
  std::optional<AccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_NE(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       GenerateWihtDifferentClockDriftTolerance) {
  // Test that clock drift tolerance does not affect code generation.
  const base::Time timestamp = base::Time::Now();

  Authenticator gen1(AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                                      base::Minutes(1)));
  std::optional<AccessCode> code1 = gen1.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code1, timestamp));

  Authenticator gen2(AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                                      base::Minutes(10)));
  std::optional<AccessCode> code2 = gen2.Generate(timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(code2, timestamp));

  EXPECT_EQ(*code1, *code2);
}

TEST_F(ParentAccessCodeAuthenticatorTest, ValidateHardcodedCodeValues) {
  // Test validation against Parent Access Code values generated in Family Link
  // Android app.
  std::map<base::Time, std::string> test_values;
  ASSERT_NO_FATAL_FAILURE(GetTestAccessCodeValues(&test_values));

  Authenticator gen(AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                                     base::Minutes(0)));
  for (const auto& it : test_values) {
    std::optional<AccessCode> code = gen.Validate(it.second, it.first);
    ASSERT_NO_FATAL_FAILURE(Verify(code, it.first));
    EXPECT_EQ(it.second, code->code());
  }
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       ValidationAndGenerationOnDifferentAuthenticators) {
  // Test validation against codes generated by separate
  // Authenticator object in and outside of the valid time
  // bucket.
  const AccessCodeConfig config(GetZeroClockDriftConfig());
  Authenticator generator(GetZeroClockDriftConfig());
  Authenticator validator(GetZeroClockDriftConfig());

  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 00:00:00 PST",
                                     &generation_timestamp));

  std::optional<AccessCode> generated_code =
      generator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Before valid period.
  std::optional<AccessCode> validated_code = validator.Validate(
      generated_code->code(), generation_timestamp - base::Seconds(1));
  EXPECT_FALSE(validated_code);

  // In valid period.
  int range = base::ClampFloor(config.code_validity() /
                               Authenticator::kAccessCodeGranularity);
  for (int i = 0; i < range; ++i) {
    validated_code = validator.Validate(
        generated_code->code(),
        generation_timestamp + i * Authenticator::kAccessCodeGranularity);
    ASSERT_TRUE(validated_code);
    EXPECT_EQ(*generated_code, *validated_code);
  }

  // After valid period.
  validated_code = validator.Validate(
      generated_code->code(), generation_timestamp + config.code_validity());
  EXPECT_FALSE(validated_code);
}

TEST_F(ParentAccessCodeAuthenticatorTest,
       ValidationAndGenerationOnSameAuthenticator) {
  // Test generation and validation on the same Authenticator
  // object in and outside of the valid time bucket.
  const AccessCodeConfig config(GetZeroClockDriftConfig());
  Authenticator authenticator(GetZeroClockDriftConfig());

  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 00:00:00 PST",
                                     &generation_timestamp));

  std::optional<AccessCode> generated_code =
      authenticator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Before valid period.
  std::optional<AccessCode> validated_code = authenticator.Validate(
      generated_code->code(), generation_timestamp - base::Seconds(1));
  EXPECT_FALSE(validated_code);

  // In valid period.
  int range = base::ClampFloor(config.code_validity() /
                               Authenticator::kAccessCodeGranularity);
  for (int i = 0; i < range; ++i) {
    validated_code = authenticator.Validate(
        generated_code->code(),
        generation_timestamp + i * Authenticator::kAccessCodeGranularity);
    ASSERT_TRUE(validated_code);
    EXPECT_EQ(*generated_code, *validated_code);
  }

  // After valid period.
  validated_code = authenticator.Validate(
      generated_code->code(), generation_timestamp + config.code_validity());
  EXPECT_FALSE(validated_code);
}

TEST_F(ParentAccessCodeAuthenticatorTest, ValidationWithClockDriftTolerance) {
  // Test validation with clock drift tolerance.
  Authenticator generator(GetDefaultTestConfig());
  Authenticator validator_with_tolerance(GetDefaultTestConfig());
  Authenticator validator_no_tolerance(AccessCodeConfig(
      kTestSharedSecret, kDefaultCodeValidity, base::Minutes(0)));

  // By default code will be valid [15:30:00-15:40:00).
  // With clock drift tolerance code will be valid [15:25:00-15:45:00).
  base::Time generation_timestamp;
  ASSERT_TRUE(base::Time::FromString("15 Jan 2019 15:30:00 PST",
                                     &generation_timestamp));

  std::optional<AccessCode> generated_code =
      generator.Generate(generation_timestamp);
  ASSERT_NO_FATAL_FAILURE(Verify(generated_code, generation_timestamp));

  // Both validators accept the code in valid period.
  int range = base::ClampFloor(kDefaultCodeValidity /
                               Authenticator::kAccessCodeGranularity);
  base::Time timestamp;
  std::optional<AccessCode> validated_code_no_tolerance;
  std::optional<AccessCode> validated_code_with_tolerance;
  for (int i = 0; i < range; ++i) {
    timestamp =
        generation_timestamp + i * Authenticator::kAccessCodeGranularity;

    validated_code_no_tolerance =
        validator_no_tolerance.Validate(generated_code->code(), timestamp);
    ASSERT_TRUE(validated_code_no_tolerance);

    validated_code_with_tolerance =
        validator_with_tolerance.Validate(generated_code->code(), timestamp);
    ASSERT_TRUE(validated_code_with_tolerance);

    EXPECT_EQ(*validated_code_no_tolerance, *validated_code_with_tolerance);
  }

  // Validator's device clock late by tolerated drift.
  timestamp = generation_timestamp - kDefaultClockDrift / 2;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_TRUE(validated_code_with_tolerance);

  // Validator's device clock late outside of tolerated drift.
  timestamp = generation_timestamp - kDefaultClockDrift - base::Seconds(1);
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_with_tolerance);

  // Validator's device clock ahead by tolerated drift.
  timestamp =
      generation_timestamp + kDefaultCodeValidity + kDefaultClockDrift / 2;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_TRUE(validated_code_with_tolerance);

  // Validator's device clock ahead outside of tolerated drift.
  timestamp = generation_timestamp + kDefaultCodeValidity + kDefaultClockDrift;
  validated_code_no_tolerance =
      validator_no_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_no_tolerance);

  validated_code_with_tolerance =
      validator_with_tolerance.Validate(generated_code->code(), timestamp);
  EXPECT_FALSE(validated_code_with_tolerance);
}

TEST_F(ParentAccessCodeAuthenticatorTest, UnixEpoch) {
  // Test authenticator with Unix Epoch timestamp.
  const base::Time unix_epoch = base::Time::UnixEpoch();

  Authenticator authenticator(GetDefaultTestConfig());
  std::optional<AccessCode> generated = authenticator.Generate(unix_epoch);
  ASSERT_NO_FATAL_FAILURE(Verify(generated, unix_epoch));
  std::optional<AccessCode> validated =
      authenticator.Validate(generated->code(), unix_epoch);
  ASSERT_NO_FATAL_FAILURE(Verify(validated, unix_epoch));
  EXPECT_EQ(generated, validated);
}

}  // namespace parent_access
}  // namespace ash
