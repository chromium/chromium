// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_chrome_service_delegate_impl.h"

#include "base/files/file_path.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class LacrosChromeServiceDelegateImplTest : public testing::Test {
 public:
  LacrosChromeServiceDelegateImplTest() = default;
  LacrosChromeServiceDelegateImplTest(
      const LacrosChromeServiceDelegateImplTest&) = delete;
  LacrosChromeServiceDelegateImplTest& operator=(
      const LacrosChromeServiceDelegateImplTest&) = delete;
  ~LacrosChromeServiceDelegateImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(chrome::GetUserDocumentsDirectory(&old_documents_dir_));
    ASSERT_TRUE(chrome::GetUserDownloadsDirectory(&old_downloads_dir_));
  }

  void TearDown() override {
    chrome::SetLacrosDefaultPaths(old_documents_dir_, old_downloads_dir_);
  }

 private:
  base::test::ScopedRunningOnChromeOS running_on_chromeos_;
  base::FilePath old_documents_dir_;
  base::FilePath old_downloads_dir_;
};

TEST_F(LacrosChromeServiceDelegateImplTest, DefaultPaths) {
  LacrosChromeServiceDelegateImpl delegate_impl;

  // Simulate ash sending some paths at startup.
  auto default_paths = crosapi::mojom::DefaultPaths::New();
  default_paths->documents = base::FilePath("/test/documents");
  default_paths->downloads = base::FilePath("/test/downloads");
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->default_paths = std::move(default_paths);
  delegate_impl.OnInitialized(*init_params);

  // Paths are overridden. We test via the chrome functions because
  // PathService::Get() requires some paths to exist on disk.
  base::FilePath path;
  ASSERT_TRUE(chrome::GetUserDocumentsDirectory(&path));
  EXPECT_EQ(path.AsUTF8Unsafe(), "/test/documents");
  ASSERT_TRUE(chrome::GetUserDownloadsDirectory(&path));
  EXPECT_EQ(path.AsUTF8Unsafe(), "/test/downloads");
}

// TODO(https://crbug.com/1150702): Delete this test after Lacros drops
// support for Chrome OS M89.
TEST_F(LacrosChromeServiceDelegateImplTest, DefaultPathsWithLegacyAsh) {
  LacrosChromeServiceDelegateImpl delegate_impl;

  // Simulate ash not sending paths at startup.
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  ASSERT_TRUE(init_params->default_paths.is_null());
  delegate_impl.OnInitialized(*init_params);

  // Paths are overridden. We test via the chrome functions because
  // PathService::Get() requires some paths to exist on disk.
  base::FilePath path;
  ASSERT_TRUE(chrome::GetUserDocumentsDirectory(&path));
  EXPECT_EQ(path.AsUTF8Unsafe(), "/home/chronos/user/MyFiles");
  ASSERT_TRUE(chrome::GetUserDownloadsDirectory(&path));
  EXPECT_EQ(path.AsUTF8Unsafe(), "/home/chronos/user/MyFiles/Downloads");
}

}  // namespace
