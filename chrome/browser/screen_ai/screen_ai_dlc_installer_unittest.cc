// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
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
    DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ =
        static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());
    install_state_ = screen_ai::ScreenAIInstallState::Create();
  }

  void TearDown() override { DlcserviceClient::Shutdown(); }

  void InstallAndWait() {
    screen_ai::dlc_installer::Install();
    task_environment_.RunUntilIdle();
  }

  void SetInstallError(const std::string& error_code) {
    fake_dlcservice_client_->set_install_error(error_code);
  }

  void ExpectSuccessHistogramCount(int expected_count,
                                   int expected_total_count) {
    histogram_tester_.ExpectBucketCount(
        "Accessibility.ScreenAI.Component.Install", true, expected_count);
    histogram_tester_.ExpectTotalCount(
        "Accessibility.ScreenAI.Component.Install", expected_total_count);
  }

  void ExpectFailureHistogramCount(int expected_count,
                                   int expected_total_count) {
    histogram_tester_.ExpectBucketCount(
        "Accessibility.ScreenAI.Component.Install", false, expected_count);
    histogram_tester_.ExpectTotalCount(
        "Accessibility.ScreenAI.Component.Install", expected_total_count);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<FakeDlcserviceClient, DanglingUntriaged | ExperimentalAsh>
      fake_dlcservice_client_;
  std::unique_ptr<screen_ai::ScreenAIInstallState> install_state_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ScreenAIDlcInstallerTest, InstallSuccess) {
  InstallAndWait();
  ExpectSuccessHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorNeedReboot) {
  SetInstallError(dlcservice::kErrorNeedReboot);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailureWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
}

TEST_F(ScreenAIDlcInstallerTest,
       InstallFailureWithDlcErrorBusyAndRetrySuccess) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);

  SetInstallError(dlcservice::kErrorNone);
  InstallAndWait();
  ExpectSuccessHistogramCount(/*expected_count=*/1, /*expected_total_count=*/2);
}

TEST_F(ScreenAIDlcInstallerTest, InstallFailuresRepeatedWithDlcErrorBusy) {
  SetInstallError(dlcservice::kErrorBusy);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/1, /*expected_total_count=*/1);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/2, /*expected_total_count=*/2);
  InstallAndWait();
  ExpectFailureHistogramCount(/*expected_count=*/3, /*expected_total_count=*/3);
}

}  // namespace ash
