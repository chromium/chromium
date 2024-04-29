// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <sstream>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) PA_FILE_PATH_LITERAL(x)

// This macro constructs strings which can contain NULs.
#define FPS(x) FilePath::StringType(FPL(x), std::size(FPL(x)) - 1)

namespace partition_alloc::internal::base {

struct UnaryTestData {
  FilePath::StringType input;
  FilePath::StringType expected;
};

struct UnaryBooleanTestData {
  FilePath::StringType input;
  bool expected;
};

struct BinaryTestData {
  FilePath::StringType inputs[2];
  FilePath::StringType expected;
};

struct BinaryBooleanTestData {
  FilePath::StringType inputs[2];
  bool expected;
};

struct BinaryIntTestData {
  FilePath::StringType inputs[2];
  int expected;
};

TEST(PartitionAllocBaseFilePathTest, Append) {
  const struct BinaryTestData cases[] = {
    {{FPL(""), FPL("cc")}, FPL("cc")},
    {{FPL("."), FPL("ff")}, FPL("ff")},
    {{FPL("."), FPL("")}, FPL(".")},
    {{FPL("/"), FPL("cc")}, FPL("/cc")},
    {{FPL("/aa"), FPL("")}, FPL("/aa")},
    {{FPL("/aa/"), FPL("")}, FPL("/aa")},
    {{FPL("//aa"), FPL("")}, FPL("//aa")},
    {{FPL("//aa/"), FPL("")}, FPL("//aa")},
    {{FPL("//"), FPL("aa")}, FPL("//aa")},
#if defined(PA_FILE_PATH_USES_DRIVE_LETTERS)
    {{FPL("c:"), FPL("a")}, FPL("c:a")},
    {{FPL("c:"), FPL("")}, FPL("c:")},
    {{FPL("c:/"), FPL("a")}, FPL("c:/a")},
    {{FPL("c://"), FPL("a")}, FPL("c://a")},
    {{FPL("c:///"), FPL("a")}, FPL("c:/a")},
#endif  // PA_FILE_PATH_USES_DRIVE_LETTERS
#if defined(PA_FILE_PATH_USES_WIN_SEPARATORS)
    // Append introduces the default separator character, so these test cases
    // need to be defined with different expected results on platforms that use
    // different default separator characters.
    {{FPL("\\"), FPL("cc")}, FPL("\\cc")},
    {{FPL("\\aa"), FPL("")}, FPL("\\aa")},
    {{FPL("\\aa\\"), FPL("")}, FPL("\\aa")},
    {{FPL("\\\\aa"), FPL("")}, FPL("\\\\aa")},
    {{FPL("\\\\aa\\"), FPL("")}, FPL("\\\\aa")},
    {{FPL("\\\\"), FPL("aa")}, FPL("\\\\aa")},
    {{FPL("/aa/bb"), FPL("cc")}, FPL("/aa/bb\\cc")},
    {{FPL("/aa/bb/"), FPL("cc")}, FPL("/aa/bb\\cc")},
    {{FPL("aa/bb/"), FPL("cc")}, FPL("aa/bb\\cc")},
    {{FPL("aa/bb"), FPL("cc")}, FPL("aa/bb\\cc")},
    {{FPL("a/b"), FPL("c")}, FPL("a/b\\c")},
    {{FPL("a/b/"), FPL("c")}, FPL("a/b\\c")},
    {{FPL("//aa"), FPL("bb")}, FPL("//aa\\bb")},
    {{FPL("//aa/"), FPL("bb")}, FPL("//aa\\bb")},
    {{FPL("\\aa\\bb"), FPL("cc")}, FPL("\\aa\\bb\\cc")},
    {{FPL("\\aa\\bb\\"), FPL("cc")}, FPL("\\aa\\bb\\cc")},
    {{FPL("aa\\bb\\"), FPL("cc")}, FPL("aa\\bb\\cc")},
    {{FPL("aa\\bb"), FPL("cc")}, FPL("aa\\bb\\cc")},
    {{FPL("a\\b"), FPL("c")}, FPL("a\\b\\c")},
    {{FPL("a\\b\\"), FPL("c")}, FPL("a\\b\\c")},
    {{FPL("\\\\aa"), FPL("bb")}, FPL("\\\\aa\\bb")},
    {{FPL("\\\\aa\\"), FPL("bb")}, FPL("\\\\aa\\bb")},
#if defined(PA_FILE_PATH_USES_DRIVE_LETTERS)
    {{FPL("c:\\"), FPL("a")}, FPL("c:\\a")},
    {{FPL("c:\\\\"), FPL("a")}, FPL("c:\\\\a")},
    {{FPL("c:\\\\\\"), FPL("a")}, FPL("c:\\a")},
    {{FPL("c:\\"), FPL("")}, FPL("c:\\")},
    {{FPL("c:\\a"), FPL("b")}, FPL("c:\\a\\b")},
    {{FPL("c:\\a\\"), FPL("b")}, FPL("c:\\a\\b")},
#endif  // PA_FILE_PATH_USES_DRIVE_LETTERS
#else   // PA_FILE_PATH_USES_WIN_SEPARATORS
    {{FPL("/aa/bb"), FPL("cc")}, FPL("/aa/bb/cc")},
    {{FPL("/aa/bb/"), FPL("cc")}, FPL("/aa/bb/cc")},
    {{FPL("aa/bb/"), FPL("cc")}, FPL("aa/bb/cc")},
    {{FPL("aa/bb"), FPL("cc")}, FPL("aa/bb/cc")},
    {{FPL("a/b"), FPL("c")}, FPL("a/b/c")},
    {{FPL("a/b/"), FPL("c")}, FPL("a/b/c")},
    {{FPL("//aa"), FPL("bb")}, FPL("//aa/bb")},
    {{FPL("//aa/"), FPL("bb")}, FPL("//aa/bb")},
#if defined(PA_FILE_PATH_USES_DRIVE_LETTERS)
    {{FPL("c:/"), FPL("a")}, FPL("c:/a")},
    {{FPL("c:/"), FPL("")}, FPL("c:/")},
    {{FPL("c:/a"), FPL("b")}, FPL("c:/a/b")},
    {{FPL("c:/a/"), FPL("b")}, FPL("c:/a/b")},
#endif  // PA_FILE_PATH_USES_DRIVE_LETTERS
#endif  // PA_FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    FilePath root(cases[i].inputs[0]);
    FilePath::StringType leaf(cases[i].inputs[1]);
    FilePath observed_str = root.Append(leaf);
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_str.value())
        << "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
    FilePath observed_path = root.Append(FilePath(leaf));
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_path.value())
        << "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
  }
}

TEST(PartitionAllocBaseFilePathTest, ConstructWithNUL) {
  // Assert FPS() works.
  ASSERT_EQ(3U, FPS("a\0b").length());

  // Test constructor strips '\0'
  FilePath path(FPS("a\0b"));
  EXPECT_EQ(1U, path.value().length());
  EXPECT_EQ(FPL("a"), path.value());
}

TEST(PartitionAllocBaseFilePathTest, AppendWithNUL) {
  // Assert FPS() works.
  ASSERT_EQ(3U, FPS("b\0b").length());

  // Test Append() strips '\0'
  FilePath path(FPL("a"));
  path = path.Append(FPS("b\0b"));
  EXPECT_EQ(3U, path.value().length());
#if defined(PA_FILE_PATH_USES_WIN_SEPARATORS)
  EXPECT_EQ(FPL("a\\b"), path.value());
#else
  EXPECT_EQ(FPL("a/b"), path.value());
#endif
}

}  // namespace partition_alloc::internal::base
