// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cert_provisioning {
namespace {

TEST(CertProvisioningCommonTest, ParseProtocolVersion) {
  EXPECT_EQ(ParseProtocolVersion(absl::nullopt), ProtocolVersion::kStatic);
  EXPECT_EQ(ParseProtocolVersion(1), ProtocolVersion::kStatic);
  EXPECT_EQ(ParseProtocolVersion(2), ProtocolVersion::kDynamic);
  EXPECT_EQ(ParseProtocolVersion(3), absl::nullopt);
}

TEST(CertProvisioningCommonTest, ProtocolVersionStableValues) {
  EXPECT_EQ(static_cast<int>(ProtocolVersion::kStatic), 1);
  EXPECT_EQ(static_cast<int>(ProtocolVersion::kDynamic), 2);
}

struct MakeFromValueTestCase {
  std::string name;
  std::string input;
  absl::optional<CertProfile> expected_output;
};

class CertProfileMakeFromValueTest
    : public testing::TestWithParam<MakeFromValueTestCase> {
 public:
  CertProfileMakeFromValueTest() = default;
  ~CertProfileMakeFromValueTest() override = default;
};

TEST_P(CertProfileMakeFromValueTest, ParseAndCheck) {
  absl::optional<CertProfile> cert_profile =
      CertProfile::MakeFromValue(base::test::ParseJsonDict(GetParam().input));
  EXPECT_EQ(cert_profile, GetParam().expected_output);
}

const MakeFromValueTestCase kMakeFromValueTests[] = {
    {"MinimalDict",
     R"({
           "policy_version": "cert_profile_version_1",
           "cert_profile_id": "cert_profile_1"
         })",
     CertProfile(/*profile_id=*/"cert_profile_1",
                 /*name=*/std::string(),
                 /*policy_version=*/"cert_profile_version_1",
                 /*is_va_enabled=*/true,
                 /*renewal_period=*/base::Seconds(0),
                 /*protocol_version=*/ProtocolVersion::kStatic)},
    {"MissingPolicyVersion",
     R"({
           "cert_profile_id": "cert_profile_1"
         })",
     absl::nullopt},
    {"MissingCertProfileId",
     R"({
           "policy_version": "cert_profile_version_1"
         })",
     absl::nullopt},
    {"AllFields",
     R"({
           "policy_version": "cert_profile_version_1",
           "name": "test_name",
           "renewal_period_seconds": 10,
           "cert_profile_id": "cert_profile_1",
           "protocol_version": 2,
           "enable_remote_attestation_check": false
         })",
     CertProfile(/*profile_id=*/"cert_profile_1",
                 /*name=*/"test_name",
                 /*policy_version=*/"cert_profile_version_1",
                 /*is_va_enabled=*/false,
                 /*renewal_period=*/base::Seconds(10),
                 /*protocol_version=*/ProtocolVersion::kDynamic)},
    {"BadProtocolVersion",
     R"({
           "policy_version": "cert_profile_version_1",
           "cert_profile_id": "cert_profile_1",
           "protocol_version": 3,
         })",
     absl::nullopt}};

INSTANTIATE_TEST_SUITE_P(
    All,
    CertProfileMakeFromValueTest,
    ::testing::ValuesIn(kMakeFromValueTests),
    [](const ::testing::TestParamInfo<CertProfileMakeFromValueTest::ParamType>&
           info) { return info.param.name; });

}  // namespace
}  // namespace ash::cert_provisioning
