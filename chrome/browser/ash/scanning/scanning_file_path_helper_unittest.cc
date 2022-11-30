// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scanning_file_path_helper.h"

#include "base/files/file_path.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// "root" is appended to the user's Google Drive directory to form the
// complete path.
constexpr char kRoot[] = "root";

}  // namespace

class ScanningFilePathHelperTest : public testing::Test {
 public:
  ScanningFilePathHelperTest()
      : drive_path_(base::FilePath("drive/root")),
        my_files_path_(base::FilePath("MyFiles")),
        file_path_helper_(ScanningFilePathHelper(drive_path_, my_files_path_)) {
  }
  ~ScanningFilePathHelperTest() override = default;

 protected:
  base::FilePath drive_path_;
  base::FilePath my_files_path_;
  ScanningFilePathHelper file_path_helper_;
};

// Validates that the correct My Files path is returned.
TEST_F(ScanningFilePathHelperTest, GetMyFilesPath) {
  EXPECT_EQ(my_files_path_, file_path_helper_.GetMyFilesPath());
}

// Validates that the correct base name is returned from a generic filepath.
TEST_F(ScanningFilePathHelperTest, BaseNameFromGenericPath) {
  EXPECT_EQ("directory", file_path_helper_.GetBaseNameFromPath(
                             base::FilePath("/test/directory")));
}

// Validates that the correct base name is returned from a generic filepath
// when the Google Drive path is empty.
TEST_F(ScanningFilePathHelperTest,
       BaseNameFromGenericPathWithGoogleDriveAbsent) {
  drive_path_ = base::FilePath();
  file_path_helper_ = ScanningFilePathHelper(drive_path_, my_files_path_);
  EXPECT_EQ("directory", file_path_helper_.GetBaseNameFromPath(
                             base::FilePath("/test/directory")));
}

// Validates that the correct base name is returned from the file paths under
// the Google Drive root.
TEST_F(ScanningFilePathHelperTest, BaseNameFromGoogleDrivePath) {
  EXPECT_EQ("Sub Folder", file_path_helper_.GetBaseNameFromPath(
                              drive_path_.Append(kRoot).Append("Sub Folder")));
  EXPECT_EQ("My Drive",
            file_path_helper_.GetBaseNameFromPath(drive_path_.Append(kRoot)));
}

// Validates that the correct base name is returned from file paths under the
// 'My Files' path.
TEST_F(ScanningFilePathHelperTest, BaseNameFromMyFilesPath) {
  EXPECT_EQ("Sub Folder", file_path_helper_.GetBaseNameFromPath(
                              my_files_path_.Append("Sub Folder")));
  EXPECT_EQ("My files", file_path_helper_.GetBaseNameFromPath(my_files_path_));
}

// Validates that passing a file path that is a child of the MyFiles path
// returns true for IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathSupportedMyFilesChild) {
  EXPECT_TRUE(file_path_helper_.IsFilePathSupported(
      my_files_path_.Append("test_file.png")));
}

// Validates that passing a file path that is a child of the Drive path returns
// true for IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathSupportedGoogleDrivePathChild) {
  EXPECT_TRUE(file_path_helper_.IsFilePathSupported(
      drive_path_.Append("test_file.png")));
}

// Validates that passing an unsupported path returns false for
// IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathNotSupported) {
  ASSERT_FALSE(file_path_helper_.IsFilePathSupported(
      base::FilePath("/wrong/file/path/file.png")));
}

// Validates that passing a file path with a reference returns false for
// IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathReferencesNotSupported) {
  ASSERT_FALSE(file_path_helper_.IsFilePathSupported(
      my_files_path_.Append("../MyFiles/test_file.png")));
}

// Validates that passing a file path that is a child of a removable media
// returns true for IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathSupportedRemovableMedia) {
  ASSERT_TRUE(file_path_helper_.IsFilePathSupported(
      base::FilePath("/media/removable/STATE/test_file.png")));
}

// Validates that passing a file path that is a child of the MyFiles path
// returns true when Google Drive path is empty for IsFilePathSupported().
TEST_F(ScanningFilePathHelperTest, FilePathSupportedGoogleDriveAbsent) {
  drive_path_ = base::FilePath();
  file_path_helper_ = ScanningFilePathHelper(drive_path_, my_files_path_);
  EXPECT_TRUE(file_path_helper_.IsFilePathSupported(
      my_files_path_.Append("test_file.png")));
}

}  // namespace ash
