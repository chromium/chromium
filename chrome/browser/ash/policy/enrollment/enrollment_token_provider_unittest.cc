// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_token_provider.h"

#include <optional>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class EnrollmentTokenProviderTest : public testing::Test {
 protected:
  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  test::EnrollmentTestHelper enrollment_test_helper_{&command_line_,
                                                     &statistics_provider_};
};

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EnrollmentTokenProviderTest, NotChromeBrandedReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  enrollment_test_helper_.SetUpFlexDevice();

  ASSERT_FALSE(
      GetEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EnrollmentTokenProviderTest,
       NoOobeConfigurationReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(GetEnrollmentToken(nullptr).has_value());
}

TEST_F(EnrollmentTokenProviderTest, NotOnFlexReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ASSERT_FALSE(
      GetEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(EnrollmentTokenProviderTest, NoEnrollmentTokenReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(
      GetEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(EnrollmentTokenProviderTest, ReturnsTokenWhenSet) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  enrollment_test_helper_.SetUpFlexDevice();

  std::optional<std::string> enrollment_token =
      GetEnrollmentToken(enrollment_test_helper_.oobe_configuration());

  ASSERT_TRUE(enrollment_token.has_value());
  ASSERT_EQ(*enrollment_token, test::kEnrollmentToken);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace policy
