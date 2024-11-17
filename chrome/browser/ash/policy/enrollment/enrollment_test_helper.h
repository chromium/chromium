// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_TEST_HELPER_H_

#include <string>

#include "base/test/scoped_command_line.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"

namespace policy::test {

extern const char kEnrollmentToken[];
extern const char kEnrollmentTokenOobeConfig[];

class EnrollmentTestHelper {
 public:
  EnrollmentTestHelper(
      base::test::ScopedCommandLine* command_line,
      ash::system::FakeStatisticsProvider* statistics_provider);
  ~EnrollmentTestHelper();

  EnrollmentTestHelper(const EnrollmentTestHelper&) = delete;
  EnrollmentTestHelper& operator=(const EnrollmentTestHelper&) = delete;

  // Pretends that the device is not a Chrome ("nonchrome") device.
  void SetUpNonchromeDevice();
  // Pretends that the device is a ChromeOS Flex device.
  void SetUpFlexDevice();
  // Configures OobeConfiguration with an enrollment token for testing.
  void SetUpEnrollmentTokenConfig(
      const char config[] = kEnrollmentTokenOobeConfig);
  // Ensures that FRE (Forced Re-Enrollment) is disabled on Flex.
  void DisableFREOnFlex();
  // Ensures that FRE (Forced Re-Enrollment) is enabled on Flex.
  void EnableFREOnFlex();
  // Obtains the enrollment token set in OOBE configuration, returning nullptr
  // if not present.
  const std::string* GetEnrollmentTokenFromOobeConfiguration();

  ash::OobeConfiguration* oobe_configuration() { return &oobe_configuration_; }

 private:
  ash::OobeConfiguration oobe_configuration_;
  raw_ptr<base::test::ScopedCommandLine> command_line_;
  raw_ptr<ash::system::FakeStatisticsProvider> statistics_provider_;
};

}  // namespace policy::test

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_TEST_HELPER_H_
