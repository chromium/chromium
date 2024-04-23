// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TEST_HELPER_H_

#include "base/test/scoped_command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"

namespace policy::test {

extern const char kFlexEnrollmentToken[];
extern const char kFlexEnrollmentTokenOobeConfig[];

class FlexEnrollmentTestHelper {
 public:
  FlexEnrollmentTestHelper(
      base::test::ScopedCommandLine* command_line,
      ash::system::FakeStatisticsProvider* statistics_provider);
  ~FlexEnrollmentTestHelper();

  FlexEnrollmentTestHelper(const FlexEnrollmentTestHelper&) = delete;
  FlexEnrollmentTestHelper& operator=(const FlexEnrollmentTestHelper&) = delete;

  // Configures ash::switches::IsRevenBranding() checks to pass.
  void SetUpFlexDevice();
  // Configures OobeConfiguration with a Flex enrollment token for testing.
  void SetUpFlexEnrollmentTokenConfig(
      const char config[] = kFlexEnrollmentTokenOobeConfig);
  // Forces FRE (Forced Re-Enrollment) to be enabled on Flex via command line
  // switch.
  void EnableFREOnFlex();

  ash::OobeConfiguration* oobe_configuration() { return &oobe_configuration_; }

 private:
  ash::OobeConfiguration oobe_configuration_;
  raw_ptr<base::test::ScopedCommandLine> command_line_;
  raw_ptr<ash::system::FakeStatisticsProvider> statistics_provider_;
};

}  // namespace policy::test

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FLEX_ENROLLMENT_TEST_HELPER_H_
