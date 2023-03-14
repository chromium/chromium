// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/pumpkin_installer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kInstallationMetricName[] =
    "PumpkinInstaller.InstallationSuccess";
}  // namespace

namespace ash {

class PumpkinInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    installer_ = std::make_unique<PumpkinInstaller>();

    DlcserviceClient::InitializeFake();
    fake_dlcservice_client_ =
        static_cast<FakeDlcserviceClient*>(DlcserviceClient::Get());
  }

  void TearDown() override {
    installer_.reset();
    DlcserviceClient::Shutdown();
  }

  void MaybeInstall() {
    installer_->MaybeInstall(
        base::BindOnce(&PumpkinInstallerTest::OnInstalled,
                       base::Unretained(this)),
        base::BindRepeating(&PumpkinInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&PumpkinInstallerTest::OnError, base::Unretained(this)));
  }

  void MaybeInstallAndWait() {
    installer_->MaybeInstall(
        base::BindOnce(&PumpkinInstallerTest::OnInstalled,
                       base::Unretained(this)),
        base::BindRepeating(&PumpkinInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&PumpkinInstallerTest::OnError, base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  void OnInstalled(bool success) { install_succeeded_ = success; }
  void OnProgress(double progress) {}
  void OnError(const std::string& error) {
    install_failed_ = true;
    last_error_ = error;
  }

  void SetInstallError() {
    fake_dlcservice_client_->set_install_error(dlcservice::kErrorNeedReboot);
  }

  void SetPumpkinAlreadyInstalled() {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLED);
    fake_dlcservice_client_->set_dlc_state(dlc_state);
  }

  void SetPumpkinCurrentlyInstalling() {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLING);
    fake_dlcservice_client_->set_dlc_state(dlc_state);
  }

  void SetGetDlcStateError() {
    fake_dlcservice_client_->set_get_dlc_state_error("Test error");
  }

  void ExpectSuccessHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(/*name=*/kInstallationMetricName,
                                        /*sample=*/true, expected_count);
  }

  void ExpectFailureHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(/*name=*/kInstallationMetricName,
                                        /*sample=*/false, expected_count);
  }

  bool install_succeeded() { return install_succeeded_; }

  bool install_failed() { return install_failed_; }

  std::string last_error() { return last_error_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<PumpkinInstaller> installer_;
  FakeDlcserviceClient* fake_dlcservice_client_;
  base::HistogramTester histogram_tester_;
  bool install_succeeded_ = false;
  bool install_failed_ = false;
  std::string last_error_ = std::string();
};

// Verifies that PumpkinInstaller can successfully download the Pumpkin DLC.
TEST_F(PumpkinInstallerTest, Install) {
  ASSERT_FALSE(install_succeeded());
  MaybeInstallAndWait();
  ASSERT_TRUE(install_succeeded());
  ASSERT_FALSE(install_failed());

  ExpectSuccessHistogramCount(1);
  ExpectFailureHistogramCount(0);
}

// Verifies that PumpkinInstaller handles the case where the DLC fails to
// download.
TEST_F(PumpkinInstallerTest, InstallError) {
  ASSERT_FALSE(install_succeeded());
  SetInstallError();
  MaybeInstallAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ(dlcservice::kErrorNeedReboot, last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(1);
}

// Verifies that PumpkinInstaller handles the case where the DLC is already
// installed.
TEST_F(PumpkinInstallerTest, AlreadyInstalled) {
  ASSERT_FALSE(install_succeeded());
  SetPumpkinAlreadyInstalled();
  MaybeInstallAndWait();
  ASSERT_TRUE(install_succeeded());
  ASSERT_FALSE(install_failed());
  EXPECT_EQ("", last_error());

  // Pumpkin was already installed, so we shouldn't record any additional
  // metrics.
  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

// Verifies that PumpkinInstaller handles the case where the DLC is currently
// installing.
TEST_F(PumpkinInstallerTest, CurrentlyInstalling) {
  ASSERT_FALSE(install_succeeded());
  SetPumpkinCurrentlyInstalling();
  MaybeInstallAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ("Pumpkin already installing.", last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

// Verifies that PumpkinInstaller handles the case where it can't retrieve the
// DLC state from the DLC service.
TEST_F(PumpkinInstallerTest, GetDlcError) {
  ASSERT_FALSE(install_succeeded());
  SetGetDlcStateError();
  MaybeInstallAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ("Test error", last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

TEST_F(PumpkinInstallerTest, PendingDlcRequest) {
  // Ensures that an error occurs if `MaybeInstall` is called in rapid
  // succession.
  ASSERT_FALSE(install_succeeded());
  MaybeInstall();
  ASSERT_FALSE(install_succeeded());
  EXPECT_EQ("", last_error());
  // Calling `MaybeInstall` again before the DlcserviceClient has responded
  // will cause an error.
  MaybeInstall();
  ASSERT_FALSE(install_succeeded());
  EXPECT_EQ("Cannot install Pumpkin, DLC request in progress.", last_error());
}

}  // namespace ash
