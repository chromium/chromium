// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_mac.h"

#include <iostream>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/mac/foundation_util.h"
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

TEST_F(BrowserDMTokenStorageMacTest, SaveDMToken) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath()));

  TestStoreDMTokenDelegate delegate;
  BrowserDMTokenStorageMac storage;
  storage.StoreDMToken(
      kDMToken, base::BindOnce(&TestStoreDMTokenDelegate::OnDMTokenStored,
                               base::Unretained(&delegate)));

  delegate.Wait();
  ASSERT_TRUE(delegate.success());

  base::FilePath app_data_dir_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_path));
  base::FilePath dm_token_dir_path = app_data_dir_path.Append(kDmTokenBaseDir);

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(storage.InitClientId()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);

  base::FilePath dm_token_file_path = dm_token_dir_path.Append(filename);

  std::string dm_token;
  ASSERT_TRUE(base::ReadFileToString(dm_token_file_path, &dm_token));
  EXPECT_EQ(kDMToken, dm_token);
}

TEST_F(BrowserDMTokenStorageMacTest, InitDMTokenWithoutDirectory) {
  std::unique_ptr<base::ScopedPathOverride> path_override;
  base::ScopedTempDir fake_app_data_dir;

  ASSERT_TRUE(fake_app_data_dir.CreateUniqueTempDir());
  path_override.reset(new base::ScopedPathOverride(
      base::DIR_APP_DATA, fake_app_data_dir.GetPath()));

  TestStoreDMTokenDelegate delegate;
  BrowserDMTokenStorageMac storage;

  base::FilePath dm_token_dir_path =
      fake_app_data_dir.GetPath().Append(kDmTokenBaseDir);

  EXPECT_EQ(std::string(), storage.InitDMToken());
  EXPECT_FALSE(base::PathExists(dm_token_dir_path));
}

}  // namespace policy
