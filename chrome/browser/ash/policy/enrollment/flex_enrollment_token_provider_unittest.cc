// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/flex_enrollment_token_provider.h"

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

class FlexEnrollmentTokenProviderTest : public testing::Test {
 protected:
  base::test::ScopedCommandLine command_line_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  test::EnrollmentTestHelper enrollment_test_helper_{&command_line_,
                                                     &statistics_provider_};
};

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest, NotChromeBrandedReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  enrollment_test_helper_.SetUpFlexDevice();

  ASSERT_FALSE(
      GetFlexEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest,
       NoOobeConfigurationReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(GetFlexEnrollmentToken(nullptr).has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NotOnFlexReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  ASSERT_FALSE(
      GetFlexEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NoFlexTokenReturnsEmptyOptional) {
  enrollment_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(
      GetFlexEnrollmentToken(enrollment_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, ReturnsTokenWhenSet) {
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  enrollment_test_helper_.SetUpFlexDevice();

  std::optional<std::string> flex_enrollment_token =
      GetFlexEnrollmentToken(enrollment_test_helper_.oobe_configuration());

  ASSERT_TRUE(flex_enrollment_token.has_value());
  ASSERT_EQ(*flex_enrollment_token, test::kEnrollmentToken);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace policy
