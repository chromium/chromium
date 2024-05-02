// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_dlc_installer.h"

#include <optional>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kFaceGazeAssetsInstallDurationMetric[] =
    "Accessibility.DlcInstallerFaceGazeAssetsInstallationDuration";

constexpr char kFaceGazeAssetsInstallationMetric[] =
    "Accessibility.DlcInstallerFaceGazeAssetsSuccess";

constexpr char kPumpkinInstallationMetric[] =
    "PumpkinInstaller.InstallationSuccess";

constexpr char kPumpkinInstallDurationMetric[] =
    "Accessibility.DlcInstallerPumpkinInstallationDuration";
}  // namespace

namespace ash {

using DlcType = AccessibilityDlcInstaller::DlcType;

class AccessibilityDlcInstallerTest : public testing::Test {
 protected:
  struct InstallData {
    bool success = false;
    std::string dlc_root_path = std::string();
    std::string last_error = std::string();
  };

  void SetUp() override {
    installer_ = std::make_unique<AccessibilityDlcInstaller>();
  }

  void TearDown() override { installer_.reset(); }

  void MaybeInstallPumpkin() {
    DlcType type = DlcType::kPumpkin;

    install_data_.insert_or_assign(type, InstallData());
    installer_->MaybeInstall(
        type,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this), type),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
                       base::Unretained(this), type));
  }

  void MaybeInstallPumpkinAndWait() {
    DlcType type = DlcType::kPumpkin;

    install_data_.insert_or_assign(type, InstallData());
    installer_->MaybeInstall(
        type,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this), type),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
                       base::Unretained(this), type));
    RunUntilIdle();
  }

  void MaybeInstallFaceGazeAssets() {
    DlcType type = DlcType::kFaceGazeAssets;

    install_data_.insert_or_assign(type, InstallData());
    installer_->MaybeInstall(
        type,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this), type),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
                       base::Unretained(this), type));
  }

  void MaybeInstallFaceGazeAssetsAndWait() {
    DlcType type = DlcType::kFaceGazeAssets;

    install_data_.insert_or_assign(type, InstallData());
    installer_->MaybeInstall(
        type,
        base::BindOnce(&AccessibilityDlcInstallerTest::OnInstalled,
                       base::Unretained(this), type),
        base::BindRepeating(&AccessibilityDlcInstallerTest::OnProgress,
                            base::Unretained(this)),
        base::BindOnce(&AccessibilityDlcInstallerTest::OnError,
                       base::Unretained(this), type));
    RunUntilIdle();
  }

  bool IsFaceGazeAssetsInstalled() {
    return installer_->IsFaceGazeAssetsInstalled();
  }

  bool IsPumpkinInstalled() {
    return installer_->IsPumpkinInstalled();
  }

  void OnInstalled(DlcType type, bool success, const std::string& root_path) {
    install_data_[type].success = success;
    install_data_[type].dlc_root_path = root_path;
  }
  void OnProgress(double progress) {}
  void OnError(DlcType type, std::string_view error) {
    install_data_[type].success = false;
    install_data_[type].last_error = error;
  }

  void SetDlcRootPath(std::string_view root_path) {
    fake_dlcservice_client_.set_install_root_path(root_path);
  }

  void SetInstallError() {
    fake_dlcservice_client_.set_install_error(dlcservice::kErrorNeedReboot);
  }

  void SetPumpkinAlreadyInstalled(std::string_view root_path) {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLED);
    dlc_state.set_root_path(std::string(root_path));
    fake_dlcservice_client_.set_dlc_state("pumpkin", dlc_state);
  }

  void SetPumpkinCurrentlyInstalling() {
    dlcservice::DlcState dlc_state;
    dlc_state.set_state(dlcservice::DlcState_State_INSTALLING);
    fake_dlcservice_client_.set_dlc_state("pumpkin", dlc_state);
  }

  void SetPumpkinDlcError() {
    fake_dlcservice_client_.set_get_dlc_state_error("pumpkin", "Test error");
  }

  void ExpectPumpkinSuccessHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(/*name=*/kPumpkinInstallationMetric,
                                        /*sample=*/true, expected_count);
  }

  void ExpectPumpkinFailureHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(/*name=*/kPumpkinInstallationMetric,
                                        /*sample=*/false, expected_count);
  }

  void ExpectTotalPumpkinDurationSamples(int expected_count) {
    histogram_tester_.ExpectTotalCount(/*name=*/kPumpkinInstallDurationMetric,
                                       expected_count);
  }

  void ExpectFaceGazeSuccessHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(
        /*name=*/kFaceGazeAssetsInstallationMetric,
        /*sample=*/true, expected_count);
  }

  void ExpectFaceGazeFailureHistogramCount(int expected_count) {
    histogram_tester_.ExpectBucketCount(
        /*name=*/kFaceGazeAssetsInstallationMetric,
        /*sample=*/false, expected_count);
  }

  void ExpectTotalFaceGazeDurationSamples(int expected_count) {
    histogram_tester_.ExpectTotalCount(
        /*name=*/kFaceGazeAssetsInstallDurationMetric, expected_count);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  bool GetInstallSuccess(DlcType type) { return install_data_[type].success; }

  std::string GetDlcRootPath(DlcType type) {
    return install_data_[type].dlc_root_path;
  }

  std::string GetLastError(DlcType type) {
    return install_data_[type].last_error;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<AccessibilityDlcInstaller> installer_;
  FakeDlcserviceClient fake_dlcservice_client_;
  base::HistogramTester histogram_tester_;
  base::flat_map<DlcType, InstallData> install_data_;
};

// Verifies that AccessibilityDlcInstaller can successfully download the Pumpkin
// DLC.
TEST_F(AccessibilityDlcInstallerTest, InstallPumpkin) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());

  SetDlcRootPath("/fake/root/path");
  MaybeInstallPumpkinAndWait();

  ASSERT_TRUE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_TRUE(IsPumpkinInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kPumpkin), "/fake/root/path");

  ExpectPumpkinSuccessHistogramCount(1);
  ExpectPumpkinFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC fails
// to download.
TEST_F(AccessibilityDlcInstallerTest, PumpkinInstallError) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  SetInstallError();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ(dlcservice::kErrorNeedReboot, GetLastError(DlcType::kPumpkin));

  ExpectPumpkinSuccessHistogramCount(0);
  ExpectPumpkinFailureHistogramCount(1);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC is
// already installed.
TEST_F(AccessibilityDlcInstallerTest, PumpkinAlreadyInstalled) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());

  SetPumpkinAlreadyInstalled("/fake/root/path");

  MaybeInstallPumpkinAndWait();
  ASSERT_TRUE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_TRUE(IsPumpkinInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kPumpkin), "/fake/root/path");
  EXPECT_EQ("", GetLastError(DlcType::kPumpkin));

  // Pumpkin was already installed, so we shouldn't record any additional
  // metrics.
  ExpectPumpkinSuccessHistogramCount(0);
  ExpectPumpkinFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where the DLC is
// currently installing.
TEST_F(AccessibilityDlcInstallerTest, PumpkinCurrentlyInstalling) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  SetPumpkinCurrentlyInstalling();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("pumpkin already installing.", GetLastError(DlcType::kPumpkin));

  ExpectPumpkinSuccessHistogramCount(0);
  ExpectPumpkinFailureHistogramCount(0);
}

// Verifies that AccessibilityDlcInstaller handles the case where it can't
// retrieve the DLC state from the DLC service.
TEST_F(AccessibilityDlcInstallerTest, GetDlcError) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  SetPumpkinDlcError();
  MaybeInstallPumpkinAndWait();
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("Test error", GetLastError(DlcType::kPumpkin));

  ExpectPumpkinSuccessHistogramCount(0);
  ExpectPumpkinFailureHistogramCount(0);
}

TEST_F(AccessibilityDlcInstallerTest, PumpkinPendingDlcRequest) {
  // Ensures that an error occurs if `MaybeInstall` is called in rapid
  // succession.
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  MaybeInstallPumpkin();

  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("", GetLastError(DlcType::kPumpkin));
  // Calling `MaybeInstall` again before the DlcserviceClient has responded
  // will cause an error.
  MaybeInstallPumpkin();
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsPumpkinInstalled());
  EXPECT_EQ("Cannot install pumpkin, DLC request in progress.",
            GetLastError(DlcType::kPumpkin));
}

TEST_F(AccessibilityDlcInstallerTest, InstallFaceGazeAssets) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_FALSE(IsFaceGazeAssetsInstalled());

  SetDlcRootPath("/fake/root/path");
  MaybeInstallFaceGazeAssetsAndWait();

  ASSERT_TRUE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_TRUE(IsFaceGazeAssetsInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kFaceGazeAssets), "/fake/root/path");

  // We should record FaceGaze metrics.
  ExpectFaceGazeSuccessHistogramCount(1);
  ExpectFaceGazeFailureHistogramCount(0);
  ExpectTotalFaceGazeDurationSamples(1);

  // We shouldn't record Pumpkin metrics if we didn't install it.
  ExpectPumpkinSuccessHistogramCount(0);
  ExpectPumpkinFailureHistogramCount(0);
  ExpectTotalPumpkinDurationSamples(0);
}

// Verifies that multiple installs can be handled simultaneously.
TEST_F(AccessibilityDlcInstallerTest, InstallMultipleDlcs) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsFaceGazeAssetsInstalled());
  ASSERT_FALSE(IsPumpkinInstalled());

  SetDlcRootPath("/fake/root/path");
  MaybeInstallFaceGazeAssets();
  MaybeInstallPumpkin();

  ASSERT_FALSE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_FALSE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_FALSE(IsFaceGazeAssetsInstalled());
  ASSERT_FALSE(IsPumpkinInstalled());
  RunUntilIdle();

  ASSERT_TRUE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_TRUE(GetInstallSuccess(DlcType::kPumpkin));
  ASSERT_TRUE(IsFaceGazeAssetsInstalled());
  ASSERT_TRUE(IsPumpkinInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kFaceGazeAssets), "/fake/root/path");
  ASSERT_EQ(GetDlcRootPath(DlcType::kPumpkin), "/fake/root/path");

  // Assert metrics are properly recorded.
  ExpectFaceGazeSuccessHistogramCount(1);
  ExpectFaceGazeFailureHistogramCount(0);
  ExpectTotalFaceGazeDurationSamples(1);

  ExpectPumpkinSuccessHistogramCount(1);
  ExpectPumpkinFailureHistogramCount(0);
  ExpectTotalPumpkinDurationSamples(1);
}

TEST_F(AccessibilityDlcInstallerTest, InstallFaceGazeAssetsTwice) {
  ASSERT_FALSE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_FALSE(IsFaceGazeAssetsInstalled());
  SetDlcRootPath("/fake/root/path");
  MaybeInstallFaceGazeAssetsAndWait();

  ASSERT_TRUE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_TRUE(IsFaceGazeAssetsInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kFaceGazeAssets), "/fake/root/path");

  ExpectFaceGazeSuccessHistogramCount(1);
  ExpectFaceGazeFailureHistogramCount(0);

  // Call this codepath again to verify that it can be called multiple times
  // without failing.
  MaybeInstallFaceGazeAssetsAndWait();
  ASSERT_TRUE(GetInstallSuccess(DlcType::kFaceGazeAssets));
  ASSERT_TRUE(IsFaceGazeAssetsInstalled());
  ASSERT_EQ(GetDlcRootPath(DlcType::kFaceGazeAssets), "/fake/root/path");

  ExpectFaceGazeSuccessHistogramCount(2);
  ExpectFaceGazeFailureHistogramCount(0);
}

}  // namespace ash
