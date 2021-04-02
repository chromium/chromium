// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"

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

namespace ash {

namespace {

// "root" is appended to the user's Google Drive directory to form the
// complete path.
constexpr char kRoot[] = "root";

}  // namespace

class ChromeScanningAppDelegateTest : public testing::Test {
 public:
  ChromeScanningAppDelegateTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ChromeScanningAppDelegateTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile1@gmail.com");

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    drive_path_ = temp_dir_.GetPath().Append("drive/root");
    EXPECT_TRUE(base::CreateDirectory(drive_path_));
    my_files_path_ = temp_dir_.GetPath().Append("MyFiles");
    EXPECT_TRUE(base::CreateDirectory(my_files_path_));

    chrome_scanning_app_delegate_ =
        std::make_unique<ChromeScanningAppDelegate>(web_ui_.get());
    chrome_scanning_app_delegate_->SetGoogleDrivePathForTesting(drive_path_);
    chrome_scanning_app_delegate_->SetMyFilesPathForTesting(my_files_path_);
  }

 protected:
  TestingProfile* profile_;
  std::unique_ptr<ChromeScanningAppDelegate> chrome_scanning_app_delegate_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_path_;
  base::FilePath drive_path_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Validates that the correct base name is returned from a generic filepath.
TEST_F(ChromeScanningAppDelegateTest, BaseNameFromGenericPath) {
  EXPECT_EQ("directory", chrome_scanning_app_delegate_->GetBaseNameFromPath(
                             base::FilePath("/test/directory")));
}

// Validates that the correct base name is returned from the file paths under
// the Google Drive root.
TEST_F(ChromeScanningAppDelegateTest, BaseNameFromGoogleDrivePath) {
  EXPECT_EQ("Sub Folder", chrome_scanning_app_delegate_->GetBaseNameFromPath(
                              drive_path_.Append(kRoot).Append("Sub Folder")));
  EXPECT_EQ("My Drive", chrome_scanning_app_delegate_->GetBaseNameFromPath(
                            drive_path_.Append(kRoot)));
}

// Validates that the correct base name is returned from file paths under the
// 'My Files' path.
TEST_F(ChromeScanningAppDelegateTest, BaseNameFromMyFilesPath) {
  EXPECT_EQ("Sub Folder", chrome_scanning_app_delegate_->GetBaseNameFromPath(
                              my_files_path_.Append("Sub Folder")));
  EXPECT_EQ("My files",
            chrome_scanning_app_delegate_->GetBaseNameFromPath(my_files_path_));
}

// Validates that passing a file path that exists and is a child of the MyFiles
// path returns true for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppMyFilesChild) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  EXPECT_TRUE(chrome_scanning_app_delegate_->ShowFileInFilesApp(test_file));
}

// Validates that passing a file path that exists and is a child of the Drive
// path returns true for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppGoogleDrivePathChild) {
  const base::FilePath test_file = drive_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  EXPECT_TRUE(chrome_scanning_app_delegate_->ShowFileInFilesApp(test_file));
}

// Validates that passing a non-existent file returns false for showing the
// Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppFileNotFound) {
  const base::FilePath missing_my_file =
      my_files_path_.Append("missing_my_file.png");
  ASSERT_FALSE(
      chrome_scanning_app_delegate_->ShowFileInFilesApp(missing_my_file));

  const base::FilePath missing_drive_file =
      drive_path_.Append("missing_drive_file.png");
  ASSERT_FALSE(
      chrome_scanning_app_delegate_->ShowFileInFilesApp(missing_drive_file));
}

// Validates that passing a unsupported path returns false for showing the Files
// app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppPathNotSupported) {
  ASSERT_FALSE(chrome_scanning_app_delegate_->ShowFileInFilesApp(
      base::FilePath("/wrong/file/path/file.png")));
}

// Validates that passing file paths with references returns false for showing
// the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppReferencesNotSupported) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_FALSE(chrome_scanning_app_delegate_->ShowFileInFilesApp(
      my_files_path_.Append("../MyFiles/test_file.png")));
}

}  // namespace ash
