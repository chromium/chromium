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
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kFlexEnrollmentToken[] = "test_flex_token";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

const char kFlexEnrollmentTokenOobeConfig[] = R"({
  "flexToken": "test_flex_token"
})";

}  // namespace

class FlexEnrollmentTokenProviderTest : public testing::Test {
 protected:
  FlexEnrollmentTokenProviderTest() {
    ash::OobeConfigurationClient::InitializeFake();
    command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kRevenBranding);
  }
  ~FlexEnrollmentTokenProviderTest() override {
    ash::OobeConfigurationClient::Shutdown();
  }

  void SetUpFlexEnrollmentToken() {
    static_cast<ash::FakeOobeConfigurationClient*>(
        ash::OobeConfigurationClient::Get())
        ->SetConfiguration(kFlexEnrollmentTokenOobeConfig);
    oobe_configuration_.CheckConfiguration();
  }

  ash::OobeConfiguration oobe_configuration_;
  base::test::ScopedCommandLine command_line_;
};

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest, NotChromeBrandedReturnsEmptyOptional) {
  SetUpFlexEnrollmentToken();

  ASSERT_FALSE(GetFlexEnrollmentToken(&oobe_configuration_).has_value());
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(FlexEnrollmentTokenProviderTest,
       NoOobeConfigurationReturnsEmptyOptional) {
  ASSERT_FALSE(GetFlexEnrollmentToken(nullptr).has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NotOnFlexReturnsEmptyOptional) {
  SetUpFlexEnrollmentToken();
  command_line_.GetProcessCommandLine()->RemoveSwitch(
      ash::switches::kRevenBranding);

  ASSERT_FALSE(GetFlexEnrollmentToken(&oobe_configuration_).has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, NoFlexTokenReturnsEmptyOptional) {
  ASSERT_FALSE(GetFlexEnrollmentToken(&oobe_configuration_).has_value());
}

TEST_F(FlexEnrollmentTokenProviderTest, ReturnsTokenWhenSet) {
  SetUpFlexEnrollmentToken();

  std::optional<std::string> flex_enrollment_token =
      GetFlexEnrollmentToken(&oobe_configuration_);

  ASSERT_TRUE(flex_enrollment_token.has_value());
  ASSERT_EQ(*flex_enrollment_token, kFlexEnrollmentToken);
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}  // namespace policy
