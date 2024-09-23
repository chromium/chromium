// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"

#include <string_view>

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

using screen_ai::dlc_installer::DlcInstallResult;

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

  void SetInstallError(std::string_view error_code) {
    fake_dlcservice_client_.set_install_error(error_code);
  }

  void ExpectNoDlcInstallHistogramCount() {
    histogram_tester_.ExpectTotalCount(
        "Accessibility.ScreenAI.DlcInstallResult", 0);
  }

  void ExpectDlcInstallHistogramCount(DlcInstallResult install_result,
                                      int expected_count) {
    histogram_tester_.ExpectTotalCount(
        "Accessibility.ScreenAI.DlcInstallResult", expected_count);
    histogram_tester_.ExpectBucketCount(
        "Accessibility.ScreenAI.DlcInstallResult", install_result,
        expected_count);
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
  ExpectDlcInstallHistogramCount(DlcInstallResult::kSuccess,
                                 /*expected_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorNeedReboot) {
  SetInstallError(dlcservice::kErrorNeedReboot);
  InstallAndWait();
  ExpectDlcInstallHistogramCount(DlcInstallResult::kErrorNeedReboot,
                                 /*expected_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectNoDlcInstallHistogramCount();

  int delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectDlcInstallHistogramCount(DlcInstallResult::kErrorBusy,
                                 /*expected_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest,
       InstallFailureWithDlcErrorBusyAndRetrySuccess) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  int delay_in_seconds = base_retry_delay_in_seconds;
  WaitForDelay(delay_in_seconds);
  ExpectNoDlcInstallHistogramCount();

  SetInstallError(dlcservice::kErrorNone);
  delay_in_seconds =
      screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
          delay_in_seconds);
  WaitForDelay(delay_in_seconds);
  ExpectDlcInstallHistogramCount(DlcInstallResult::kSuccess,
                                 /*expected_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailuresRepeatedWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectNoDlcInstallHistogramCount();
  int delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectDlcInstallHistogramCount(DlcInstallResult::kErrorBusy,
                                 /*expected_count=*/1);

  InstallAndWait();
  delay_in_seconds = base_retry_delay_in_seconds;
  for (int i = 0; i < max_install_retries; ++i) {
    WaitForDelay(delay_in_seconds);
    delay_in_seconds =
        screen_ai::dlc_installer::CalculateNextDelayInSecondsForTesting(
            delay_in_seconds);
  }
  ExpectDlcInstallHistogramCount(DlcInstallResult::kErrorBusy,
                                 /*expected_count=*/2);
}

TEST_F(ScreenAIDlcInstallerTest, UninstallSuccess) {
  UninstallAndWait();
  ExpectNoDlcInstallHistogramCount();
}

}  // namespace ash
