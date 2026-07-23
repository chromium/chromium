// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/twa_launch_queue_delegate.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/android/content_uri_test_utils.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

class TwaLaunchQueueDelegateTest : public testing::Test {
 protected:
  TwaLaunchQueueDelegate delegate_;
};

TEST_F(TwaLaunchQueueDelegateTest, GetPathInfo_LocalPath) {
  base::FilePath local_path("/path/to/local_file.txt");
  content::PathInfo path_info = delegate_.GetPathInfo(local_path);
  EXPECT_EQ(path_info.path, local_path);
  EXPECT_EQ(path_info.display_name, "local_file.txt");
}

TEST_F(TwaLaunchQueueDelegateTest, GetPathInfo_ContentUri) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("test_display_name.txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test"));

  std::optional<base::FilePath> content_uri =
      base::test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(
          file_path);
  ASSERT_TRUE(content_uri.has_value());

  content::PathInfo path_info = delegate_.GetPathInfo(*content_uri);
  EXPECT_EQ(path_info.path, *content_uri);
  EXPECT_EQ(path_info.display_name, "test_display_name.txt");
}

}  // namespace webapps
