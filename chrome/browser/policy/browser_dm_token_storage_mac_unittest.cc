// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_mac.h"

#include <iostream>
#include <memory>

#include "base/apple/foundation_util.h"
#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
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

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment");

constexpr char kDMToken[] = "fake-dm-token";

}  // namespace

class BrowserDMTokenStorageMacTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserDMTokenStorageMacTest, InitClientId) {
  BrowserDMTokenStorageMac storage;
  EXPECT_FALSE(storage.InitClientId().empty());
}

class TestStoreDMTokenDelegate {
 public:
  TestStoreDMTokenDelegate() : called_(false), success_(true) {}
  ~TestStoreDMTokenDelegate() {}

  void OnDMTokenUpdated(bool success) {
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

TEST_F(BrowserDMTokenStorageMacTest, SaveDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  TestStoreDMTokenDelegate callback_delegate;
  BrowserDMTokenStorageMac storage_delegate;
  auto task = storage_delegate.SaveDMTokenTask(kDMToken,
                                               storage_delegate.InitClientId());
  auto reply = base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                              base::Unretained(&callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));

  callback_delegate.Wait();
  ASSERT_TRUE(callback_delegate.success());

  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);

  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);

  std::string dm_token;
  ASSERT_TRUE(base::ReadFileToString(dm_token_file_path, &dm_token));
  EXPECT_EQ(kDMToken, dm_token);
}

TEST_F(BrowserDMTokenStorageMacTest, DeleteDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  // Creating the DMToken file.
  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);
  ASSERT_TRUE(base::CreateDirectory(dm_token_dir_path));

  std::string filename;
  BrowserDMTokenStorageMac storage_delegate;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);
  ASSERT_TRUE(base::WriteFile(base::FilePath(dm_token_file_path), kDMToken));
  ASSERT_TRUE(base::PathExists(dm_token_file_path));

  // Deleting the saved DMToken.
  TestStoreDMTokenDelegate delete_callback_delegate;
  auto delete_task =
      storage_delegate.DeleteDMTokenTask(storage_delegate.InitClientId());
  auto delete_reply =
      base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                     base::Unretained(&delete_callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(delete_task), std::move(delete_reply));

  delete_callback_delegate.Wait();
  ASSERT_TRUE(delete_callback_delegate.WasCalled());
  ASSERT_TRUE(delete_callback_delegate.success());

  ASSERT_FALSE(base::PathExists(dm_token_file_path));
}

TEST_F(BrowserDMTokenStorageMacTest, DeleteEmptyDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  BrowserDMTokenStorageMac storage_delegate;
  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);
  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(storage_delegate.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);

  ASSERT_FALSE(base::PathExists(dm_token_file_path));

  TestStoreDMTokenDelegate callback_delegate;
  auto delete_task =
      storage_delegate.DeleteDMTokenTask(storage_delegate.InitClientId());
  auto delete_reply =
      base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenUpdated,
                     base::Unretained(&callback_delegate));
  storage_delegate.SaveDMTokenTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(delete_task), std::move(delete_reply));

  callback_delegate.Wait();
  ASSERT_TRUE(callback_delegate.WasCalled());
  ASSERT_TRUE(callback_delegate.success());

  ASSERT_FALSE(base::PathExists(dm_token_file_path));
}

TEST_F(BrowserDMTokenStorageMacTest, InitDMTokenWithoutDirectory) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override = std::make_unique<base::ScopedPathOverride>(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath());

  TestStoreDMTokenDelegate delegate;
  BrowserDMTokenStorageMac storage;

  base::FilePath dm_token_dir_path =
      fake_app_data_dir.GetPath().Append(kDmTokenBaseDir);

  EXPECT_EQ(std::string(), storage.InitDMToken());
  EXPECT_FALSE(base::PathExists(dm_token_dir_path));
}

}  // namespace policy
