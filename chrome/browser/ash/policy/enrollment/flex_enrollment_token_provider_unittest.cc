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
#include "chrome/browser/ash/policy/enrollment/flex_enrollment_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class FlexEnrollmentTokenProviderTest : public testing::Test {
 protected:
  base::test::ScopedCommandLine command_line_;
  test::FlexEnrollmentTestHelper flex_token_test_helper_{&command_line_};
};

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest, NotChromeBrandedReturnsEmptyOptional) {
  flex_token_test_helper_.SetUpFlexEnrollmentTokenConfig();
  flex_token_test_helper_.SetUpFlexDevice();

  ASSERT_FALSE(
      GetFlexEnrollmentToken(flex_token_test_helper_.oobe_configuration())
          .has_value());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest,
       NoOobeConfigurationReturnsEmptyOptional) {
  flex_token_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(GetFlexEnrollmentToken(nullptr).has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NotOnFlexReturnsEmptyOptional) {
  flex_token_test_helper_.SetUpFlexEnrollmentTokenConfig();
  ASSERT_FALSE(
      GetFlexEnrollmentToken(flex_token_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NoFlexTokenReturnsEmptyOptional) {
  flex_token_test_helper_.SetUpFlexDevice();
  ASSERT_FALSE(
      GetFlexEnrollmentToken(flex_token_test_helper_.oobe_configuration())
          .has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, ReturnsTokenWhenSet) {
  flex_token_test_helper_.SetUpFlexEnrollmentTokenConfig();
  flex_token_test_helper_.SetUpFlexDevice();

  std::optional<std::string> flex_enrollment_token =
      GetFlexEnrollmentToken(flex_token_test_helper_.oobe_configuration());

  ASSERT_TRUE(flex_enrollment_token.has_value());
  ASSERT_EQ(*flex_enrollment_token, test::kFlexEnrollmentToken);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace policy
