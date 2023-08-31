// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller_lacros.h"

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpFilesControllerLacrosTest
    : public DlpFilesTestBase,
      public ::testing::WithParamInterface<
          std::tuple<std::string, absl::optional<data_controls::Component>>> {
 public:
  DlpFilesControllerLacrosTest(const DlpFilesControllerLacrosTest&) = delete;
  DlpFilesControllerLacrosTest& operator=(const DlpFilesControllerLacrosTest&) =
      delete;

 protected:
  DlpFilesControllerLacrosTest() = default;
  ~DlpFilesControllerLacrosTest() override = default;

  void SetUp() override {
    DlpFilesTestBase::SetUp();
    files_controller_ =
        std::make_unique<DlpFilesControllerLacros>(*rules_manager_);

    base::PathService::Get(base::DIR_HOME, &my_files_dir_);
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));

    drivefs_ = my_files_dir_.Append(FILE_PATH_LITERAL("drive"));
    removable_media_dir_ = my_files_dir_.Append(FILE_PATH_LITERAL("USB"));
    android_files_dir_ = my_files_dir_.Append(FILE_PATH_LITERAL("android"));
    linux_files_dir_ = my_files_dir_.Append(FILE_PATH_LITERAL("linux"));
    documents_dir_ = my_files_dir_.Append(FILE_PATH_LITERAL("Documents"));
    downloads_dir_ = my_files_dir_.Append(FILE_PATH_LITERAL("Downloads"));
    chrome::SetLacrosDefaultPaths(
        documents_dir_, downloads_dir_, drivefs_, removable_media_dir_,
        android_files_dir_, linux_files_dir_, ash_resources_dir_,
        share_cache_dir_, preinstalled_web_app_config_dir_,
        preinstalled_web_app_extra_config_dir_);
  }

  std::unique_ptr<DlpFilesControllerLacros> files_controller_;

  base::FilePath my_files_dir_;

  base::FilePath documents_dir_;
  base::FilePath downloads_dir_;
  base::FilePath drivefs_;
  base::FilePath removable_media_dir_;
  base::FilePath android_files_dir_;
  base::FilePath linux_files_dir_;
  base::FilePath ash_resources_dir_;
  base::FilePath share_cache_dir_;
  base::FilePath preinstalled_web_app_config_dir_;
  base::FilePath preinstalled_web_app_extra_config_dir_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesControllerLacrosTest,
    ::testing::Values(std::make_tuple("/android/path/filename",
                                      data_controls::Component::kArc),
                      std::make_tuple("/USB/path/filename",
                                      data_controls::Component::kUsb),
                      std::make_tuple("/linux/path/filename",
                                      data_controls::Component::kCrostini),
                      std::make_tuple("/drive/path/filename",
                                      data_controls::Component::kDrive),
                      std::make_tuple("/Downloads", absl::nullopt)));
TEST_P(DlpFilesControllerLacrosTest, MapFilePathToPolicyComponentTest) {
  auto [path, expected_component] = GetParam();
  EXPECT_EQ(files_controller_->MapFilePathToPolicyComponent(
                profile_, base::FilePath(my_files_dir_.value() + path)),
            expected_component);
}

}  // namespace policy
