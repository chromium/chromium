// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/file_system_access/aw_file_system_access_permission_context.h"

#include "base/base_paths_android.h"
#include "base/base_paths_posix.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

using HandleType = AwFileSystemAccessPermissionContext::HandleType;
using UserAction = AwFileSystemAccessPermissionContext::UserAction;

class AwFileSystemAccessPermissionContextTest : public testing::Test {
 public:
  AwFileSystemAccessPermissionContextTest() = default;

 protected:
  bool IsBlocked(const base::FilePath& path) {
    base::test::TestFuture<
        AwFileSystemAccessPermissionContext::SensitiveEntryResult>
        future;
    permission_context_.ConfirmSensitiveEntryAccess(
        kTestOrigin, content::PathInfo(path), HandleType::kFile,
        UserAction::kOpen, content::GlobalRenderFrameHostId(),
        future.GetCallback());
    auto result = future.Get();
    return result ==
           AwFileSystemAccessPermissionContext::SensitiveEntryResult::kAbort;
  }

  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));

 private:
  content::BrowserTaskEnvironment task_environment_;
  AwFileSystemAccessPermissionContext permission_context_;
};

TEST_F(AwFileSystemAccessPermissionContextTest, BlockedPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath app_data_dir = temp_dir.GetPath().AppendASCII("app_data");
  base::ScopedPathOverride app_data_override(base::DIR_ANDROID_APP_DATA,
                                             app_data_dir, true, true);

  // The android app data directory, and paths inside should not be allowed.
  EXPECT_TRUE(IsBlocked(app_data_dir));
  EXPECT_TRUE(IsBlocked(app_data_dir.AppendASCII("foo")));

  base::FilePath cache_dir = temp_dir.GetPath().AppendASCII("cache");
  base::ScopedPathOverride cache_override(base::DIR_CACHE, cache_dir, true,
                                          true);
  // The android cache directory, and paths inside should not be allowed.
  EXPECT_TRUE(IsBlocked(cache_dir));
  EXPECT_TRUE(IsBlocked(cache_dir.AppendASCII("foo")));
}

}  // namespace android_webview
