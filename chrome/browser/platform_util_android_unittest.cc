// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/android/content_uri_test_utils.h"
#include "base/test/test_future.h"
#include "chrome/browser/platform_util_internal.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_util {
namespace {

class PlatformUtilAndroidTest : public testing::Test {
 protected:
  void SetUp() override {
    internal::DisableShellOperationsForTesting();

    base::FilePath cache_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_CACHE, &cache_dir));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDirUnderPath(cache_dir));

    ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().AppendASCII("sub")));
    ASSERT_TRUE(
        base::WriteFile(temp_dir_.GetPath().AppendASCII("file.txt"), "hello"));

    std::optional<base::FilePath> saf_dir =
        base::test::android::GetVirtualDocumentPathFromCacheDirDirectory(
            temp_dir_.GetPath());
    ASSERT_TRUE(saf_dir.has_value());
    saf_root_ = *saf_dir;
  }

  OpenOperationResult CallOpenItem(const base::FilePath& path,
                                   OpenItemType item_type) {
    base::test::TestFuture<OpenOperationResult> future;
    OpenItem(/*profile=*/nullptr, path, item_type, future.GetCallback());
    return future.Get();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath saf_root_;
};

TEST_F(PlatformUtilAndroidTest, OpenFolderVirtualDocumentPath) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(saf_root_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_SUCCEEDED,
            CallOpenItem(saf_root_.AsEndingWithSeparator(), OPEN_FOLDER));
  EXPECT_EQ(OPEN_SUCCEEDED,
            CallOpenItem(saf_root_.AppendASCII("sub"), OPEN_FOLDER));
}

TEST_F(PlatformUtilAndroidTest, OpenFolderOnFileReturnsInvalidType) {
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(saf_root_.AppendASCII("file.txt"), OPEN_FOLDER));
}

TEST_F(PlatformUtilAndroidTest, OpenFolderOnMissingPathReturnsNotFound) {
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(saf_root_.AppendASCII("missing"), OPEN_FOLDER));
}

}  // namespace
}  // namespace platform_util
