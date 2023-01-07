// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/subtree_set.h"

#include "testing/gtest/include/gtest/gtest.h"

#define FPL(path) base::FilePath(FILE_PATH_LITERAL(path))

namespace sync_file_system {

TEST(SubtreeSetTest, InsertAndErase) {
  SubtreeSet subtrees;

  EXPECT_EQ(0u, subtrees.size());
  EXPECT_TRUE(subtrees.insert(FPL("/a/b/c")));
  EXPECT_FALSE(subtrees.insert(FPL("/a/b")));
  EXPECT_FALSE(subtrees.insert(FPL("/a/b/c")));
  EXPECT_FALSE(subtrees.insert(FPL("/a/b/c/d")));
  EXPECT_TRUE(subtrees.insert(FPL("/a/b/d")));
  EXPECT_FALSE(subtrees.insert(FPL("/")));

  EXPECT_EQ(2u, subtrees.size());

  EXPECT_FALSE(subtrees.erase(FPL("/")));
  EXPECT_FALSE(subtrees.erase(FPL("/a")));
  EXPECT_TRUE(subtrees.erase(FPL("/a/b/c")));

  EXPECT_EQ(1u, subtrees.size());

  EXPECT_TRUE(subtrees.insert(FPL("/a/b/c/d")));

  EXPECT_EQ(2u, subtrees.size());
}

}  // namespace sync_file_system
