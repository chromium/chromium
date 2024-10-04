// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/process_requirement.h"

#include <Kernel/kern/cs_blobs.h>
#include <Security/Security.h>
#include <stdint.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/mac/code_signature_spi.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_byteorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace base::mac {
namespace {

constexpr std::string kTeamId = "ABCDEFG";
constexpr std::string_view kExpectedDeveloperIdRequirementString =
    "certificate leaf[subject.OU] = ABCDEFG and anchor apple generic and "
    "certificate 1[field.1.2.840.113635.100.6.2.6] /* exists */ and "
    "certificate leaf[field.1.2.840.113635.100.6.1.13] /* exists */";
constexpr std::string_view kExpectedAppStoreRequirementString =
    "certificate leaf[subject.OU] = ABCDEFG and anchor apple generic and "
    "certificate leaf[field.1.2.840.113635.100.6.1.9] /* exists */";
constexpr std::string_view kExpectedDevelopmentRequirementString =
    "certificate leaf[subject.OU] = ABCDEFG and anchor apple generic "
    "and certificate 1[field.1.2.840.113635.100.6.2.1] /* exists */";

struct CSOpsSystemCallProviderForTesting
    : ProcessRequirement::CSOpsSystemCallProvider {
  // Dispatch from the system call interface to helper functions that can be
  // mocked in a more natural fashion.
  int csops(pid_t pid,
            unsigned int ops,
            void* useraddr,
            size_t usersize) override {
    switch (ops) {
      case CS_OPS_TEAMID: {
        struct TeamIdResult {
          uint32_t type;
          uint32_t length;
          char identifier[CS_MAX_TEAMID_LEN + 1];
        };
        CHECK(usersize >= sizeof(TeamIdResult));
        TeamIdResult* team_id_result = static_cast<TeamIdResult*>(useraddr);
        int result = GetTeamIdentifier(team_id_result->identifier);
        errno = result;
        if (!result) {
          team_id_result->type = 0;
          team_id_result->length =
              HostToNet32(strlen(team_id_result->identifier));
        }
        return result ? -1 : 0;
      }
      case CS_OPS_VALIDATION_CATEGORY: {
        CHECK(usersize >= sizeof(unsigned int));
        int result =
            GetValidationCategory(static_cast<unsigned int*>(useraddr));
        errno = result;
        return result ? -1 : 0;
      }
      default:
        NOTREACHED();
    }
  }

  bool SupportsValidationCategory() const override {
    // This test implementation supports returning the validation category on
    // all macOS versions.
    return true;
  }

  void SetTeamIdentifier(std::string team_id) {
    ON_CALL(*this, GetTeamIdentifier)
        .WillByDefault([team_id = std::move(team_id)](span<char> out_team_id) {
          out_team_id.copy_prefix_from(team_id);
          out_team_id[team_id.size()] = 0;
          return 0;
        });
  }

  void SetValidationCategory(unsigned int validation_category) {
    ON_CALL(*this, GetValidationCategory)
        .WillByDefault(
            [validation_category](unsigned int* out_validation_category) {
              *out_validation_category = validation_category;
              return 0;
            });
  }

  MOCK_METHOD(int, GetTeamIdentifier, ((base::span<char>)));
  MOCK_METHOD(int, GetValidationCategory, (unsigned int*));
};

class ProcessRequirementTest : public testing::Test {
 public:
  void SetUp() override {
    ProcessRequirement::SetCSOpsSystemCallProviderForTesting(&csops_provider_);

    // Have all `csops` system calls fail with `ENOTSUP` by default.
    ON_CALL(csops_provider_, GetTeamIdentifier).WillByDefault(Return(ENOTSUP));
    ON_CALL(csops_provider_, GetValidationCategory)
        .WillByDefault(Return(ENOTSUP));
  }

  void TearDown() override {
    ProcessRequirement::SetCSOpsSystemCallProviderForTesting(nullptr);
  }

 protected:
  testing::NiceMock<CSOpsSystemCallProviderForTesting> csops_provider_;
};

std::optional<std::string> AsRequirementString(
    const ProcessRequirement& requirement) {
  apple::ScopedCFTypeRef<SecRequirementRef> requirement_ref =
      requirement.AsSecRequirement();
  apple::ScopedCFTypeRef<CFStringRef> requirement_string;
  OSStatus status =
      SecRequirementCopyString(requirement_ref.get(), kSecCSDefaultFlags,
                               requirement_string.InitializeInto());
  if (status != errSecSuccess) {
    return std::nullopt;
  }

  return SysCFStringRefToUTF8(requirement_string.get());
}

TEST_F(ProcessRequirementTest, FailedSystemCalls) {
  EXPECT_FALSE(ProcessRequirement::Builder().HasSameCertificateType().Build());
  EXPECT_FALSE(ProcessRequirement::Builder().HasSameTeamIdentifier().Build());
  EXPECT_FALSE(ProcessRequirement::Builder().SignedWithSameIdentity().Build());
}

TEST_F(ProcessRequirementTest, DeveloperId) {
  csops_provider_.SetTeamIdentifier(kTeamId);

  // Explicitly specify the certificate type.
  std::optional<ProcessRequirement> requirement =
      ProcessRequirement::Builder()
          .HasSameTeamIdentifier()
          .DeveloperIdCertificateType()
          .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedDeveloperIdRequirementString);

  // Same certificate type where current process is signed with Developer ID.
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_DEVELOPER_ID);
  requirement = ProcessRequirement::Builder()
                    .HasSameTeamIdentifier()
                    .HasSameCertificateType()
                    .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedDeveloperIdRequirementString);
}

TEST_F(ProcessRequirementTest, AppStore) {
  csops_provider_.SetTeamIdentifier(kTeamId);

  // Explicitly specify the certificate type.
  std::optional<ProcessRequirement> requirement = ProcessRequirement::Builder()
                                                      .HasSameTeamIdentifier()
                                                      .AppStoreCertificateType()
                                                      .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedAppStoreRequirementString);

  // Same certificate type where current process is signed with an App Store
  // certificate.
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_APP_STORE);
  requirement = ProcessRequirement::Builder()
                    .HasSameTeamIdentifier()
                    .HasSameCertificateType()
                    .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedAppStoreRequirementString);
}

TEST_F(ProcessRequirementTest, Development) {
  csops_provider_.SetTeamIdentifier(kTeamId);

  // Explicitly specify the certificate type.
  std::optional<ProcessRequirement> requirement =
      ProcessRequirement::Builder()
          .HasSameTeamIdentifier()
          .DevelopmentCertificateType()
          .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedDevelopmentRequirementString);

  // Same certificate type where current process is signed with a development
  // certificate.
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_DEVELOPMENT);
  requirement = ProcessRequirement::Builder()
                    .HasSameTeamIdentifier()
                    .HasSameCertificateType()
                    .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            kExpectedDevelopmentRequirementString);
}

TEST_F(ProcessRequirementTest, Identifier) {
  std::optional<ProcessRequirement> requirement =
      ProcessRequirement::Builder()
          .Identifier("com.example.Application")
          .TeamIdentifier(kTeamId)
          .DeveloperIdCertificateType()
          .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            "identifier \"com.example.Application\" and " +
                std::string(kExpectedDeveloperIdRequirementString));

  requirement = ProcessRequirement::Builder()
                    .IdentifierIsOneOf({
                        "com.example.ApplicationA",
                        "com.example.ApplicationB",
                        "com.example.ApplicationC",
                    })
                    .TeamIdentifier(kTeamId)
                    .DeveloperIdCertificateType()
                    .Build();
  EXPECT_TRUE(requirement);
  EXPECT_EQ(AsRequirementString(*requirement),
            "(identifier \"com.example.ApplicationA\" or identifier "
            "\"com.example.ApplicationB\" or identifier "
            "\"com.example.ApplicationC\") and " +
                std::string(kExpectedDeveloperIdRequirementString));
}

TEST_F(ProcessRequirementTest, AdHocOrUnsigned) {
  // CS_OPS_TEAMID returns ENOENT for an ad-hoc signed or unsigned process.
  ON_CALL(csops_provider_, GetTeamIdentifier).WillByDefault(Return(ENOENT));
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_NONE);

  std::optional<ProcessRequirement> requirement = ProcessRequirement::Builder()
                                                      .HasSameTeamIdentifier()
                                                      .HasSameCertificateType()
                                                      .Build();
  EXPECT_TRUE(requirement);
  EXPECT_FALSE(requirement->AsSecRequirement());
}

TEST_F(ProcessRequirementTest, ValidationCategoryDetectionFallback) {
  // Older versions of macOS do not support `CS_OPS_VALIDATION_CATEGORY` and
  // will return EINVAL. ProcessRequirement falls back to inferring the
  // validation category by checking the current process's code signature
  // against code signing requirements. There is not a good way to intercept
  // those for testing at the moment.
}

TEST_F(ProcessRequirementTest, KernelFailuresAreNotFatal) {
  // An empty team ID with a valid validation category is not a valid
  // combination, but should not be treated as programmer error if the empty
  // team ID came from the kernel.
  csops_provider_.SetTeamIdentifier("");
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_DEVELOPER_ID);
  std::optional<ProcessRequirement> requirement =
      ProcessRequirement::Builder().SignedWithSameIdentity().Build();
  EXPECT_FALSE(requirement);

  // A validation category of none with a non-empty team ID is not a valid
  // combination, but should not be treated as programmer error if the
  // validation category came from the kernel.
  csops_provider_.SetTeamIdentifier(kTeamId);
  csops_provider_.SetValidationCategory(CS_VALIDATION_CATEGORY_NONE);
  requirement = ProcessRequirement::Builder().SignedWithSameIdentity().Build();
  EXPECT_FALSE(requirement);
}

TEST_F(ProcessRequirementTest, InvalidCombinations) {
  EXPECT_DEATH(
      {
        // A requirement that includes a team identifier but no certificate type
        // will assert because it could be matched by a self-signed certificate
        // type and is likely to be an error.
        csops_provider_.SetTeamIdentifier(kTeamId);
        ProcessRequirement::Builder().HasSameTeamIdentifier().Build();
      },
      "without specifying a certificate type is unsafe");

  EXPECT_DEATH(
      {
        // A requirement that includes a certificate type but no team identifier
        // will assert because it will match any signing identity of that type
        // and is likely to be an error.
        ProcessRequirement::Builder().DeveloperIdCertificateType().Build();
      },
      "without a team identifier is unsafe");
}

}  // namespace
}  // namespace base::mac
