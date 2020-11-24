// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scanning_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ScanningUtilTest : public testing::Test {
 public:
  ScanningUtilTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ScanningUtilTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile1@gmail.com");
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_path_ = temp_dir_.GetPath().Append("drive/root");
    ASSERT_TRUE(base::CreateDirectory(drive_path_));
    my_files_path_ = temp_dir_.GetPath().Append("MyFiles");
    ASSERT_TRUE(base::CreateDirectory(my_files_path_));
  }

 protected:
  std::unique_ptr<content::TestWebUI> web_ui_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_path_;
  base::FilePath drive_path_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* profile_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Tests that passing a file path that exists and is a child of the MyFiles path
// returns true.
TEST_F(ScanningUtilTest, MyFilePathChild) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(scanning::ShowFileInFilesApp(drive_path_, my_files_path_,
                                           web_ui_.get(), test_file));
}

// Tests that passing a file path that exists and is a child of the Drive path
// returns true.
TEST_F(ScanningUtilTest, DrivePathChild) {
  const base::FilePath test_file = drive_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_TRUE(scanning::ShowFileInFilesApp(drive_path_, my_files_path_,
                                           web_ui_.get(), test_file));
}

// Tests that passing a non-existent file returns false.
TEST_F(ScanningUtilTest, FileNotFound) {
  const base::FilePath missing_my_file =
      my_files_path_.Append("missing_my_file.png");
  ASSERT_FALSE(scanning::ShowFileInFilesApp(drive_path_, my_files_path_,
                                            web_ui_.get(), missing_my_file));

  const base::FilePath missing_drive_file =
      drive_path_.Append("missing_drive_file.png");
  ASSERT_FALSE(scanning::ShowFileInFilesApp(drive_path_, my_files_path_,
                                            web_ui_.get(), missing_drive_file));
}

// Tests that passing a non-supported path returns false.
TEST_F(ScanningUtilTest, PathNotSupported) {
  ASSERT_FALSE(scanning::ShowFileInFilesApp(
      drive_path_, my_files_path_, web_ui_.get(),
      base::FilePath("/wrong/file/path/file.png")));
}

// Tests that passing file paths with references returns false.
TEST_F(ScanningUtilTest, ReferencesNotSupported) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_FALSE(scanning::ShowFileInFilesApp(
      drive_path_, my_files_path_, web_ui_.get(),
      my_files_path_.Append("../MyFiles/test_file.png")));
}

}  // namespace chromeos.
