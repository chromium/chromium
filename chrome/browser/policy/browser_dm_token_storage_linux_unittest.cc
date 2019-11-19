// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_linux.h"

#include <iostream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

const char kDmTokenBaseDir[] = FILE_PATH_LITERAL("Policy/Enrollment/");
const char kEnrollmentTokenFilename[] =
    FILE_PATH_LITERAL("enrollment/CloudManagementEnrollmentToken");
// TODO(crbug.com/907589): Remove once no longer in use.
const char kEnrollmentTokenOldFilename[] =
    FILE_PATH_LITERAL("enrollment/enrollment_token");

const char kMachineId[] = "a1254c624234b270985170c3549725f1";
const char kExpectedClientId[] =
    "JXduKRDItaY72B6vHikFl9U95m8";  // Corresponds to kMachineId.
const char kEnrollmentToken[] = "fake-enrollment-token";
const char kDMToken[] = "fake-dm-token";

}  // namespace

class MockBrowserDMTokenStorageLinux : public BrowserDMTokenStorageLinux {
 public:
  std::string ReadMachineIdFile() override { return kMachineId; }
};

class BrowserDMTokenStorageLinuxTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserDMTokenStorageLinuxTest, InitClientId) {
  MockBrowserDMTokenStorageLinux storage;
  EXPECT_EQ(kExpectedClientId, storage.InitClientId());
}

TEST_F(BrowserDMTokenStorageLinuxTest, InitEnrollmentToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_policy_dir;

  ASSERT_TRUE(fake_policy_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(chrome::DIR_POLICY_FILES,
                                                   fake_policy_dir.GetPath()));

  base::FilePath dir_policy_files_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_POLICY_FILES, &dir_policy_files_path));
  base::FilePath enrollment_token_file_path =
      dir_policy_files_path.Append(kEnrollmentTokenFilename);

  ASSERT_TRUE(base::CreateDirectory(enrollment_token_file_path.DirName()));

  int bytes_written =
      base::WriteFile(base::FilePath(enrollment_token_file_path),
                      kEnrollmentToken, strlen(kEnrollmentToken));
  ASSERT_EQ(static_cast<int>(strlen(kEnrollmentToken)), bytes_written);

  MockBrowserDMTokenStorageLinux storage;
  EXPECT_EQ(kEnrollmentToken, storage.InitEnrollmentToken());
}

// TODO(crbug.com/907589): Remove once no longer in use.
TEST_F(BrowserDMTokenStorageLinuxTest, InitOldEnrollmentToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_policy_dir;

  ASSERT_TRUE(fake_policy_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(chrome::DIR_POLICY_FILES,
                                                   fake_policy_dir.GetPath()));

  base::FilePath dir_policy_files_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_POLICY_FILES, &dir_policy_files_path));
  base::FilePath enrollment_token_file_path =
      dir_policy_files_path.Append(kEnrollmentTokenOldFilename);

  ASSERT_TRUE(base::CreateDirectory(enrollment_token_file_path.DirName()));

  int bytes_written =
      base::WriteFile(base::FilePath(enrollment_token_file_path),
                      kEnrollmentToken, strlen(kEnrollmentToken));
  ASSERT_EQ(static_cast<int>(strlen(kEnrollmentToken)), bytes_written);

  MockBrowserDMTokenStorageLinux storage;
  EXPECT_EQ(kEnrollmentToken, storage.InitEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageLinuxTest, InitDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_user_data_dir;

  ASSERT_TRUE(fake_user_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      chrome::DIR_USER_DATA, fake_user_data_dir.GetPath()));

  base::FilePath dir_user_data_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_USER_DATA, &dir_user_data_path));

  base::FilePath dm_token_dir_path = dir_user_data_path.Append(kDmTokenBaseDir);
  ASSERT_TRUE(base::CreateDirectory(dm_token_dir_path));

  base::FilePath dm_token_file_path =
      dm_token_dir_path.Append(kExpectedClientId);
  int bytes_written = base::WriteFile(base::FilePath(dm_token_file_path),
                                      kDMToken, strlen(kDMToken));
  ASSERT_EQ(static_cast<int>(strlen(kDMToken)), bytes_written);

  MockBrowserDMTokenStorageLinux storage;
  EXPECT_EQ(kDMToken, storage.InitDMToken());
}

TEST_F(BrowserDMTokenStorageLinuxTest, InitDMTokenWithoutDirectory) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_user_data_dir;

  ASSERT_TRUE(fake_user_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      chrome::DIR_USER_DATA, fake_user_data_dir.GetPath()));

  base::FilePath dm_token_dir_path =
      fake_user_data_dir.GetPath().Append(kDmTokenBaseDir);

  MockBrowserDMTokenStorageLinux storage;
  EXPECT_EQ(std::string(), storage.InitDMToken());

  EXPECT_FALSE(base::PathExists(dm_token_dir_path));
}

class TestStoreDMTokenDelegate {
 public:
  TestStoreDMTokenDelegate() : called_(false), success_(false) {}
  ~TestStoreDMTokenDelegate() {}

  void OnDMTokenStored(bool success) {
    run_loop_.Quit();
    called_ = true;
    success_ = success;
  }

  bool WasCalled() {
    bool was_called = called_;
    called_ = false;
    return was_called;
  }

  bool success() { return success_; }

  void Wait() { run_loop_.Run(); }

 private:
  bool called_;
  bool success_;
  base::RunLoop run_loop_;
};

TEST_F(BrowserDMTokenStorageLinuxTest, SaveDMToken) {
  TestStoreDMTokenDelegate delegate;
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_user_data_dir;

  ASSERT_TRUE(fake_user_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      chrome::DIR_USER_DATA, fake_user_data_dir.GetPath()));

  MockBrowserDMTokenStorageLinux storage;
  storage.StoreDMToken(
      kDMToken, base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenStored,
                               base::Unretained(&delegate)));

  delegate.Wait();
  ASSERT_TRUE(delegate.WasCalled());
  ASSERT_TRUE(delegate.success());

  base::FilePath dir_user_data_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_USER_DATA, &dir_user_data_path));
  base::FilePath dm_token_dir_path = dir_user_data_path.Append(kDmTokenBaseDir);
  base::FilePath dm_token_file_path =
      dm_token_dir_path.Append(kExpectedClientId);

  std::string dm_token;
  ASSERT_TRUE(base::ReadFileToString(dm_token_file_path, &dm_token));
  EXPECT_EQ(kDMToken, dm_token);
}

}  // namespace policy
