// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_dlc_installer.h"

#include "base/memory/raw_ptr.h"
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

class AccessibilityDlcInstallerTest : public testing::Test {
 protected:
  void SetUp() override {
    installer_ = std::make_unique<AccessibilityDlcInstaller>();
  }

  void TearDown() override { installer_.reset(); }

  void MaybeInstallPumpkin() {
    installer_->MaybeInstall(
        AccessibilityDlcInstaller::DlcType::kPumpkin,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this)),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
        base::Unretained(this)));
  }

  void MaybeInstallPumpkinAndWait() {
    installer_->MaybeInstall(
        AccessibilityDlcInstaller::DlcType::kPumpkin,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this)),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
        base::Unretained(this)));
    task_environment_.RunUntilIdle();
  }

  bool IsPumpkinInstalled() {
    return installer_->IsPumpkinInstalled();
  }

  void OnInstalled(bool success, const std::string& root_path) {
    install_succeeded_ = success;
    dlc_root_path_ = root_path;
  }
  void OnProgress(double progress) {}
  void OnError(const std::string& error) {
    install_failed_ = true;
    last_error_ = error;
  }

  void SetDlcRootPath(const std::string& root_path) {
    fake_dlcservice_client_.set_install_root_path(root_path);
  }

  void SetInstallError() {
    fake_dlcservice_client_.set_install_error(dlcservice::kErrorNeedReboot);
  }

  void SetDlcAlreadyInstalled(const std::string& root_path) {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLED);
    dlc_state.set_root_path(root_path);
    fake_dlcservice_client_.set_dlc_state(dlc_state);
  }

  void SetDlcCurrentlyInstalling() {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLING);
    fake_dlcservice_client_.set_dlc_state(dlc_state);
  }

  void SetGetDlcStateError() {
    fake_dlcservice_client_.set_get_dlc_state_error("Test error");
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

  std::string dlc_root_path() { return dlc_root_path_; }

  std::string last_error() { return last_error_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AccessibilityDlcInstaller> installer_;
  FakeDlcserviceClient fake_dlcservice_client_;
  base::HistogramTester histogram_tester_;
  bool install_succeeded_ = false;
  bool install_failed_ = false;
  std::string dlc_root_path_ = std::string();
  std::string last_error_ = std::string();
};

// Verifies that AccessibilityDlcInstaller can successfully download the Pumpkin
// DLC.
TEST_F(AccessibilityDlcInstallerTest, Install) {
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());

  SetDlcRootPath("/fake/root/path");
  MaybeInstallPumpkinAndWait();

  ASSERT_TRUE(install_succeeded());
  ASSERT_TRUE(IsPumpkinInstalled());
  ASSERT_EQ(dlc_root_path(), "/fake/root/path");
  ASSERT_FALSE(install_failed());

  ExpectSuccessHistogramCount(1);
  ExpectFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC fails
// to download.
TEST_F(AccessibilityDlcInstallerTest, InstallError) {
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  SetInstallError();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ(dlcservice::kErrorNeedReboot, last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(1);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC is
// already installed.
TEST_F(AccessibilityDlcInstallerTest, AlreadyInstalled) {
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());

  SetDlcAlreadyInstalled("/fake/root/path");

  MaybeInstallPumpkinAndWait();
  ASSERT_TRUE(install_succeeded());
  ASSERT_TRUE(IsPumpkinInstalled());
  ASSERT_EQ(dlc_root_path(), "/fake/root/path");
  ASSERT_FALSE(install_failed());
  EXPECT_EQ("", last_error());

  // Pumpkin was already installed, so we shouldn't record any additional
  // metrics.
  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC is
// currently installing.
TEST_F(AccessibilityDlcInstallerTest, CurrentlyInstalling) {
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  SetDlcCurrentlyInstalling();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ("Pumpkin already installing.", last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where it can't
// retrieve the DLC state from the DLC service.
TEST_F(AccessibilityDlcInstallerTest, GetDlcError) {
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  SetGetDlcStateError();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  ASSERT_TRUE(install_failed());
  EXPECT_EQ("Test error", last_error());

  ExpectSuccessHistogramCount(0);
  ExpectFailureHistogramCount(0);
}

TEST_F(AccessibilityDlcInstallerTest, PendingDlcRequest) {
  // Ensures that an error occurs if `MaybeInstall` is called in rapid
  // succession.
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  MaybeInstallPumpkin();
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("", last_error());
  // Calling `MaybeInstall` again before the DlcserviceClient has responded
  // will cause an error.
  MaybeInstallPumpkin();
  ASSERT_FALSE(install_succeeded());
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("Cannot install Pumpkin, DLC request in progress.", last_error());
}

}  // namespace ash
