// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/file_system_watcher/arc_file_system_watcher_service.h"

#include <string.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcFileSystemWatcherServiceTest, AndroidSupportedMediaExtensionsSorted) {
  const auto less_comparator = [](const char* a, const char* b) {
    return strcmp(a, b) < 0;
  };
  EXPECT_TRUE(std::is_sorted(
      kAndroidSupportedMediaExtensions,
      kAndroidSupportedMediaExtensions + kAndroidSupportedMediaExtensionsSize,
      less_comparator));
}

TEST(ArcFileSystemWatcherServiceTest, HasAndroidSupportedMediaExtension) {
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.3g2"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.jpg"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.png"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.xmf"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.nef"))));

  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.JPEG"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.cute.jpg"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/.kitten.jpg"))));

  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.txt"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.jpg.exe"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten"))));
}

}  // namespace arc
