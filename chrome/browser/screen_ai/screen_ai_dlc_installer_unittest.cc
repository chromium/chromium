// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash {

class ScreenAIDlcInstallerTest
    : public testing::Test,
      public testing::WithParamInterface<std::string> {
 protected:
  void SetUp() override {
    install_state_ = screen_ai::ScreenAIInstallState::Create();
    base_retry_delay_in_seconds =
        screen_ai::dlc_installer::base_retry_delay_in_seconds_for_testing();
    max_install_retries =
        screen_ai::dlc_installer::max_install_retries_for_testing();
  }

  void InstallAndWait() {
    screen_ai::dlc_installer::Install();
    task_environment_.RunUntilIdle();
  }

  void UninstallAndWait() {
    screen_ai::dlc_installer::Uninstall();
    task_environment_.RunUntilIdle();
  }

  void WaitForDelay(int delay_in_seconds) {
    task_environment_.AdvanceClock(base::Seconds(delay_in_seconds));
    task_environment_.RunUntilIdle();
  }

  void SetInstallError(const std::string& error_code) {
    fake_dlcservice_client_.set_install_error(error_code);
  }

  void ExpectSuccessHistogramCount(const std::string& histogram_name,
                                   int expected_count,
                                   int expected_total_count) {
    histogram_tester_.ExpectBucketCount(histogram_name, true, expected_count);
    histogram_tester_.ExpectTotalCount(histogram_name, expected_total_count);
  }

  void ExpectFailureHistogramCount(int expected_count,
                                   int expected_total_count) {
    histogram_tester_.ExpectBucketCount(
        "Accessibility.ScreenAI.Component.Install", false, expected_count);
    histogram_tester_.ExpectTotalCount(
        "Accessibility.ScreenAI.Component.Install", expected_total_count);
  }

  int base_retry_delay_in_seconds;
  int max_install_retries;

 private:
  FakeDlcserviceClient fake_dlcservice_client_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<screen_ai::ScreenAIInstallState> install_state_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ScreenAIDlcInstallerTest, InstallSuccess) {
  InstallAndWait();
  ExpectSuccessHistogramCount("Accessibility.ScreenAI.Component.Install",
                              /*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorNeedReboot) {
  SetInstallError(dlcservice::kErrorNeedReboot);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/0, /*expected_total_count=*/0);

  int delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest,
       InstallFailureWithDlcErrorBusyAndRetrySuccess) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  int delay_in_seconds = base_retry_delay_in_seconds;
  WaitForDelay(delay_in_seconds);
  ExpectFailureHistogramCount(/*expected_count=*/0, /*expected_total_count=*/0);

  SetInstallError(dlcservice::kErrorNone);
  delay_in_seconds =
      screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
          delay_in_seconds);
  WaitForDelay(delay_in_seconds);
  ExpectSuccessHistogramCount("Accessibility.ScreenAI.Component.Install",
                              /*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailuresRepeatedWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/0, /*expected_total_count=*/0);
  int delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);

  InstallAndWait();
  delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectFailureHistogramCount(/*expected_count=*/2, /*expected_total_count=*/2);
}

TEST_F(ScreenAIDlcInstallerTest, UninstallSuccess) {
  UninstallAndWait();
  ExpectSuccessHistogramCount("Accessibility.ScreenAI.Component.Uninstall",
                              /*expected_count=*/1, /*expected_total_count=*/1);
}

// TODO(b/289009784): Write tests to check installation and uninstallation
// triggered in `screen_ai::dlc_installer::ManageInstallation()`. For those
// tests, need to create a temp binary to trigger uninstallation successfully.

}  // namespace ash
