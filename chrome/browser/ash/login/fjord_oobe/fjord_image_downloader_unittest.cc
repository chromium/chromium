// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_image_downloader.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/dissidia/dissidia_client.h"
#include "chromeos/dbus/dissidia/fake_dissidia_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dissidia/dbus-constants.h"

namespace ash {

class FjordImageDownloaderTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::DissidiaClient::InitializeFake();
    fake_dissidia_client_ = static_cast<chromeos::FakeDissidiaClient*>(
        chromeos::DissidiaClient::Get());
  }

  void TearDown() override {
    fake_dissidia_client_ = nullptr;
    chromeos::DissidiaClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  raw_ptr<chromeos::FakeDissidiaClient> fake_dissidia_client_ = nullptr;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FjordImageDownloaderTest, PerformUpdateSucceeds) {
  base::HistogramTester histogram_tester;
  fake_dissidia_client_->set_update_status(
      dissidia::PerformUpdateStatus::kUpdateStarted);

  FjordImageDownloader downloader;
  bool result = false;

  downloader.RunDissidia(
      FjordImageDownloader::ImageType::kNoctis,
      base::BindOnce([](bool* out, bool success,
                        const std::string& error_message) { *out = success; },
                     &result));

  // PerformUpdate returned kStarted, so we're waiting for the Completed signal.
  // Simulate the Completed signal.
  fake_dissidia_client_->NotifyCompleted(true,
                                         dissidia::CompletedErrorCode::kSuccess,
                                         "Update completed successfully");

  EXPECT_TRUE(result);
  EXPECT_EQ("noctis", fake_dissidia_client_->last_target());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  histogram_tester.ExpectUniqueSample(
      "OOBE.FjordImageDownloader.Result",
      FjordImageDownloader::ImageDownloadResult::kSuccess, 1);
}

TEST_F(FjordImageDownloaderTest, PerformUpdateFailsOnError) {
  base::HistogramTester histogram_tester;
  fake_dissidia_client_->set_update_status(
      dissidia::PerformUpdateStatus::kError);
  fake_dissidia_client_->set_update_message("System error");

  FjordImageDownloader downloader;
  bool result = true;
  std::string error_msg;

  downloader.RunDissidia(FjordImageDownloader::ImageType::kSelphie,
                         base::BindOnce(
                             [](bool* out, std::string* err_out, bool success,
                                const std::string& error_message) {
                               *out = success;
                               *err_out = error_message;
                             },
                             &result, &error_msg));

  EXPECT_FALSE(result);
  EXPECT_EQ(error_msg, "Error 4: System error");
  EXPECT_EQ("selphie", fake_dissidia_client_->last_target());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);
  histogram_tester.ExpectUniqueSample(
      "OOBE.FjordImageDownloader.Result",
      FjordImageDownloader::ImageDownloadResult::kPerformUpdateError, 1);
}

TEST_F(FjordImageDownloaderTest, PerformUpdateAlreadyOnImage) {
  base::HistogramTester histogram_tester;
  fake_dissidia_client_->set_update_status(
      dissidia::PerformUpdateStatus::kAlreadyOnRequestedImage);
  fake_dissidia_client_->set_update_message("Already on requested image");

  FjordImageDownloader downloader;
  bool result = true;

  downloader.RunDissidia(
      FjordImageDownloader::ImageType::kNoctis,
      base::BindOnce([](bool* out, bool success,
                        const std::string& error_message) { *out = success; },
                     &result));

  EXPECT_FALSE(result);
  histogram_tester.ExpectUniqueSample(
      "OOBE.FjordImageDownloader.Result",
      FjordImageDownloader::ImageDownloadResult::kAlreadyOnRequestedImage, 1);
}

TEST_F(FjordImageDownloaderTest, CompletedSignalWithFailure) {
  base::HistogramTester histogram_tester;
  fake_dissidia_client_->set_update_status(
      dissidia::PerformUpdateStatus::kUpdateStarted);

  FjordImageDownloader downloader;
  bool result = true;
  std::string error_msg;

  downloader.RunDissidia(FjordImageDownloader::ImageType::kSelphie,
                         base::BindOnce(
                             [](bool* out, std::string* err_out, bool success,
                                const std::string& error_message) {
                               *out = success;
                               *err_out = error_message;
                             },
                             &result, &error_msg));

  // Simulate a failed Completed signal.
  fake_dissidia_client_->NotifyCompleted(
      false, dissidia::CompletedErrorCode::kDownloadFailed, "Download failed");

  EXPECT_FALSE(result);
  EXPECT_EQ(error_msg, "Error 2: Download failed");
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);
  histogram_tester.ExpectUniqueSample(
      "OOBE.FjordImageDownloader.Result",
      FjordImageDownloader::ImageDownloadResult::kDownloadFailed, 1);
}

TEST_F(FjordImageDownloaderTest, RejectsSecondRequestWhileRunning) {
  fake_dissidia_client_->set_update_status(
      dissidia::PerformUpdateStatus::kUpdateStarted);

  FjordImageDownloader downloader;
  bool first_result = false;
  bool second_result = true;

  downloader.RunDissidia(
      FjordImageDownloader::ImageType::kNoctis,
      base::BindOnce([](bool* out, bool success,
                        const std::string& error_message) { *out = success; },
                     &first_result));

  // Try a second request while the first is in progress.
  downloader.RunDissidia(
      FjordImageDownloader::ImageType::kSelphie,
      base::BindOnce([](bool* out, bool success,
                        const std::string& error_message) { *out = success; },
                     &second_result));

  EXPECT_FALSE(second_result);
  EXPECT_EQ(fake_dissidia_client_->perform_update_call_count(), 1);

  // Complete the first request.
  fake_dissidia_client_->NotifyCompleted(
      true, dissidia::CompletedErrorCode::kSuccess, "Done");
  EXPECT_TRUE(first_result);
}

}  // namespace ash
