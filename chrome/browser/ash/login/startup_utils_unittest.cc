// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/startup_utils.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/337093489): Add more unit tests for MarkDeviceRegistered and all the
// other utility functions in StartupUtils.
class StartupUtilsTest : public testing::Test {
 protected:
  StartupUtilsTest() {
    RegisterLocalState(fake_local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&fake_local_state_);
  }

  ~StartupUtilsTest() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple fake_local_state_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  base::test::ScopedCommandLine command_line_;
  policy::test::EnrollmentTestHelper enrollment_test_helper_{
      &command_line_, &statistics_provider_};
};

TEST_F(StartupUtilsTest, MarkDeviceRegisteredDeletesFlexConfig) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();

  ash::StartupUtils::MarkDeviceRegistered(base::DoNothing());

  const std::string* enrollment_token =
      enrollment_test_helper_.GetEnrollmentTokenFromOobeConfiguration();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_EQ(enrollment_token, nullptr);
#else
  // The enrollment token from the Flex OOBE config is completely ignored
  // everywhere if the build isn't chrome-branded, and so the deletion is a
  // no-op here as well.
  EXPECT_EQ(*enrollment_token, policy::test::kEnrollmentToken);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
