// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/safe_base_name.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SafeBaseNameTest, Basic) {
  std::optional<SafeBaseName> basename(SafeBaseName::Create(FilePath()));
  EXPECT_TRUE(basename.has_value());
  EXPECT_TRUE(basename->path().empty());

  std::optional<SafeBaseName> basename2(
      SafeBaseName::Create(FILE_PATH_LITERAL("bar")));
  EXPECT_TRUE(basename2);
  EXPECT_EQ(basename2->path(), FilePath(FILE_PATH_LITERAL("bar")));
}

#if defined(FILE_PATH_USES_WIN_SEPARATORS)
TEST(SafeBaseNameTest, WinRoot) {
  std::optional<SafeBaseName> basename(
      SafeBaseName::Create(FILE_PATH_LITERAL("C:\\foo\\bar")));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL("bar")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("C:\\"));
  EXPECT_FALSE(basename.has_value());
}
#else
TEST(SafeBaseNameTest, Root) {
  std::optional<SafeBaseName> basename(
      SafeBaseName::Create(FilePath(FILE_PATH_LITERAL("/"))));
  EXPECT_FALSE(basename.has_value());
}
#endif  // FILE_PATH_USES_WIN_SEPARATORS

TEST(SafeBaseNameTest, Separators) {
  std::optional<SafeBaseName> basename(
      SafeBaseName::Create(FILE_PATH_LITERAL("/foo/bar")));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL("bar")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("/a/b/c/"));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL("c")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("/a/b/c/."));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL(".")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("/a/b/c/.."));
  EXPECT_FALSE(basename.has_value());

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("a/b/c"));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL("c")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("a/b/."));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL(".")));

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("a/b/.."));
  EXPECT_FALSE(basename.has_value());

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("a/../"));
  EXPECT_FALSE(basename.has_value());

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("a/.."));
  EXPECT_FALSE(basename.has_value());

  basename = SafeBaseName::Create(FILE_PATH_LITERAL("../bar"));
  EXPECT_TRUE(basename.has_value());
  EXPECT_EQ(basename->path(), FilePath(FILE_PATH_LITERAL("bar")));
}

}  // namespace base