// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"

#include <string.h>

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcFileSystemWatcherUtilTest, AppendRelativePathForRemovableMedia) {
  base::FilePath android_path(FILE_PATH_LITERAL("/storage"));
  EXPECT_TRUE(AppendRelativePathForRemovableMedia(
      base::FilePath(FILE_PATH_LITERAL("/media/removable/UNTITLED/foo.jpg")),
      &android_path));
  EXPECT_EQ("/storage/removable_UNTITLED/foo.jpg", android_path.value());

  android_path = base::FilePath(FILE_PATH_LITERAL("/storage"));
  EXPECT_TRUE(AppendRelativePathForRemovableMedia(
      base::FilePath(
          FILE_PATH_LITERAL("/media/removable/UNTITLED/foo/bar/baz.mp4")),
      &android_path));
  EXPECT_EQ("/storage/removable_UNTITLED/foo/bar/baz.mp4",
            android_path.value());

  android_path = base::FilePath(FILE_PATH_LITERAL("/"));
  EXPECT_TRUE(AppendRelativePathForRemovableMedia(
      base::FilePath(FILE_PATH_LITERAL("/media/removable/UNTITLED/foo.jpg")),
      &android_path));
  EXPECT_EQ("/removable_UNTITLED/foo.jpg", android_path.value());

  // Error: |cros_path| is not under /media/removable.
  android_path = base::FilePath(FILE_PATH_LITERAL("/storage"));
  EXPECT_FALSE(AppendRelativePathForRemovableMedia(
      base::FilePath(FILE_PATH_LITERAL("/foo/bar/UNTITLED/foo.jpg")),
      &android_path));
  EXPECT_EQ("/storage", android_path.value());

  // Error: |cros_path| is a parent of /media/removable.
  android_path = base::FilePath(FILE_PATH_LITERAL("/storage"));
  EXPECT_FALSE(AppendRelativePathForRemovableMedia(
      base::FilePath(FILE_PATH_LITERAL("/media")), &android_path));
  EXPECT_EQ("/storage", android_path.value());

  // Error: |cros_path| does not contain a component for device label.
  android_path = base::FilePath(FILE_PATH_LITERAL("/storage"));
  EXPECT_FALSE(AppendRelativePathForRemovableMedia(
      base::FilePath(FILE_PATH_LITERAL("/media/removable")), &android_path));
  EXPECT_EQ("/storage", android_path.value());
}

TEST(ArcFileSystemWatcherUtilTest, GetAndroidPath) {
  // Tests if the function is able to convert a nested removable media path
  // correctly.
  EXPECT_EQ(
      "/storage/removable_UNTITLED/foo/bar/baz.mp4",
      GetAndroidPath(base::FilePath(FILE_PATH_LITERAL(
                         "/media/removable/UNTITLED/foo/bar/baz.mp4")),
                     base::FilePath(FILE_PATH_LITERAL("/media/removable")),
                     base::FilePath(FILE_PATH_LITERAL("/storage")))
          .value());

  // Tests if the function is able to convert a single top level file path for
  // removable media device correctly.
  EXPECT_EQ(
      "/storage/removable_UNTITLED/foo.jpg",
      GetAndroidPath(base::FilePath(FILE_PATH_LITERAL(
                         "/media/removable/UNTITLED/foo.jpg")),
                     base::FilePath(FILE_PATH_LITERAL("/media/removable")),
                     base::FilePath(FILE_PATH_LITERAL("/storage")))
          .value());

  // Tests whether the function is able to convert an arbitrary chrome path to
  // any Android path.
  EXPECT_EQ("/android/dir/foo/bar/baz.mp4",
            GetAndroidPath(
                base::FilePath(FILE_PATH_LITERAL("/cros/dir/foo/bar/baz.mp4")),
                base::FilePath(FILE_PATH_LITERAL("/cros/dir")),
                base::FilePath(FILE_PATH_LITERAL("/android/dir")))
                .value());

  // Tests if the function is able to handle the case where |android_dir| is
  // "/".
  EXPECT_EQ("/foo/bar/baz.mp4",
            GetAndroidPath(
                base::FilePath(FILE_PATH_LITERAL("/cros/dir/foo/bar/baz.mp4")),
                base::FilePath(FILE_PATH_LITERAL("/cros/dir")),
                base::FilePath(FILE_PATH_LITERAL("/")))
                .value());

  // Tests if the function is able to handle the case where |cros_dir| is "/".
  EXPECT_EQ(
      "/android/dir/foo/bar/baz.mp4",
      GetAndroidPath(base::FilePath(FILE_PATH_LITERAL("/foo/bar/baz.mp4")),
                     base::FilePath(FILE_PATH_LITERAL("/")),
                     base::FilePath(FILE_PATH_LITERAL("/android/dir")))
          .value());

  // Tests if the function is able to handle the case where both |cros_dir| and
  // |android_dir| are "/".
  EXPECT_EQ(
      "/foo/bar/baz.mp4",
      GetAndroidPath(base::FilePath(FILE_PATH_LITERAL("/foo/bar/baz.mp4")),
                     base::FilePath(FILE_PATH_LITERAL("/")),
                     base::FilePath(FILE_PATH_LITERAL("/")))
          .value());

  // Error: |cros_path| is not /media/removable.
  EXPECT_EQ("",
            GetAndroidPath(
                base::FilePath(FILE_PATH_LITERAL("/cros/path/foo/bar/baz.mp4")),
                base::FilePath(FILE_PATH_LITERAL("/media/removable")),
                base::FilePath(FILE_PATH_LITERAL("/storage")))
                .value());
}

TEST(ArcFileSystemWatcherUtilTest, HasAndroidSupportedMediaExtension) {
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
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.txt"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.pdf"))));

  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.JPEG"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.cute.jpg"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/.kitten.jpg"))));

  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.zip"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.jpg.exe"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten"))));
}

}  // namespace arc
