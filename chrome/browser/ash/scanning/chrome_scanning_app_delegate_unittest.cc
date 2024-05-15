// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kExpectedScanSettings[] = R"({
    "lastUsedScannerName": "Brother MFC-J497DW",
    "scanToPath": "path/to/file",
    "scanners": [
      {
        "name": "Brother MFC-J497DW",
        "lastScanDate": "2021-04-16T02:45:26.768Z",
        "sourceName": "ADF",
        "fileType": 2,
        "colorMode": 1,
        "pageSize": 2,
        "resolutionDpi": 100
      }
    ]
  })";

// "root" is appended to the user's Google Drive directory to form the
// complete path.
constexpr char kRoot[] = "root";

// The name of the sticky settings pref.
constexpr char kScanningStickySettingsPref[] =
    "scanning.scanning_sticky_settings";

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
    removable_media_path_ = temp_dir_.GetPath().Append("removable/media");
    EXPECT_TRUE(base::CreateDirectory(removable_media_path_));

    chrome_scanning_app_delegate_ =
        std::make_unique<ChromeScanningAppDelegate>(web_ui_.get());
    chrome_scanning_app_delegate_->SetValidPaths(drive_path_, my_files_path_);
    chrome_scanning_app_delegate_->SetRemoveableMediaPathForTesting(
        removable_media_path_);
  }

  void TearDown() override { web_contents_.reset(); }

  void DidFilesAppOpen(bool expected_value, bool files_app_opened) {
    EXPECT_EQ(expected_value, files_app_opened);
    std::move(run_loop_closure_).Run();
  }

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<ChromeScanningAppDelegate> chrome_scanning_app_delegate_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  base::ScopedTempDir temp_dir_;
  base::FilePath my_files_path_;
  base::FilePath drive_path_;
  base::FilePath removable_media_path_;
  base::OnceClosure run_loop_closure_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Validates that the correct MyFiles path is returned.
TEST_F(ChromeScanningAppDelegateTest, GetMyFilesPath) {
  EXPECT_EQ(my_files_path_, chrome_scanning_app_delegate_->GetMyFilesPath());
}

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
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      test_file,
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/true));
  run_loop.Run();
}

// Validates that passing a file path that exists and is a child of the Drive
// path returns true for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppGoogleDrivePathChild) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath test_file = drive_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      test_file,
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/true));
  run_loop.Run();
}

// Validates that passing a file path that exists and is a child of a removable
// media returns true for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppRemovableMediaPathChild) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath test_file =
      removable_media_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      test_file,
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/true));
  run_loop.Run();
}

// Validates that passing a non-existent file in the MyFiles path returns false
// for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppMyFilesFileNotFound) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath missing_my_file =
      my_files_path_.Append("missing_my_file.png");
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      missing_my_file,
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/false));
  run_loop.Run();
}

// Validates that passing a non-existent file in the Drive path returns false
// for showing the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppGoogleDriveFileNotFound) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath missing_drive_file =
      drive_path_.Append("missing_drive_file.png");
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      missing_drive_file,
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/false));
  run_loop.Run();
}

// Validates that passing a unsupported path returns false for showing the Files
// app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppPathNotSupported) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      base::FilePath("/wrong/file/path/file.png"),
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/false));
  run_loop.Run();
}

// Validates that passing file paths with references returns false for showing
// the Files app.
TEST_F(ChromeScanningAppDelegateTest, ShowFilesAppReferencesNotSupported) {
  base::RunLoop run_loop;
  run_loop_closure_ = run_loop.QuitWhenIdleClosure();

  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  chrome_scanning_app_delegate_->ShowFileInFilesApp(
      my_files_path_.Append("../MyFiles/test_file.png"),
      base::BindOnce(&ChromeScanningAppDelegateTest::DidFilesAppOpen,
                     base::Unretained(this), /*expected_value=*/false));
  run_loop.Run();
}

// Validates that scan settings are saved to the Pref service.
TEST_F(ChromeScanningAppDelegateTest, SaveScanSettings) {
  chrome_scanning_app_delegate_->SaveScanSettingsToPrefs(kExpectedScanSettings);
  EXPECT_EQ(kExpectedScanSettings,
            profile_->GetPrefs()->GetString(kScanningStickySettingsPref));
}

// Validates that scan settings can be retrieved from the Pref service.
TEST_F(ChromeScanningAppDelegateTest, GetScanSettings) {
  // No pref should be found for |kScanningStickySettingsPref| yet.
  EXPECT_EQ(std::string(),
            chrome_scanning_app_delegate_->GetScanSettingsFromPrefs());

  profile_->GetPrefs()->SetString(kScanningStickySettingsPref,
                                  kExpectedScanSettings);
  EXPECT_EQ(kExpectedScanSettings,
            chrome_scanning_app_delegate_->GetScanSettingsFromPrefs());
}

// Validates that passing a file path that is a child of the MyFiles path
// returns true for IsFilePathSupported().
TEST_F(ChromeScanningAppDelegateTest, FilePathSupportedMyFilesChild) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  EXPECT_TRUE(chrome_scanning_app_delegate_->IsFilePathSupported(test_file));
}

// Validates that passing a file path that is a child of the Drive path returns
// true for IsFilePathSupported().
TEST_F(ChromeScanningAppDelegateTest, FilePathSupportedGoogleDrivePathChild) {
  const base::FilePath test_file = drive_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  EXPECT_TRUE(chrome_scanning_app_delegate_->IsFilePathSupported(test_file));
}

// Validates that passing an unsupported path returns false for
// IsFilePathSupported().
TEST_F(ChromeScanningAppDelegateTest, FilePathNotSupported) {
  ASSERT_FALSE(chrome_scanning_app_delegate_->IsFilePathSupported(
      base::FilePath("/wrong/file/path/file.png")));
}

// Validates that passing a file path with a reference returns false for
// IsFilePathSupported().
TEST_F(ChromeScanningAppDelegateTest, FilePathReferencesNotSupported) {
  const base::FilePath test_file = my_files_path_.Append("test_file.png");
  base::File(test_file, base::File::FLAG_CREATE | base::File::FLAG_READ);
  ASSERT_FALSE(chrome_scanning_app_delegate_->IsFilePathSupported(
      my_files_path_.Append("../MyFiles/test_file.png")));
}

}  // namespace ash
