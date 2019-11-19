// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/file_utils.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace fuchsia {

class OpenDirectoryTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(
        base::FilePath(kPersistedDataDirectoryPath)));
  }

  ScopedTempDir temp_dir;
};

TEST_F(OpenDirectoryTest, Open) {
  auto dir = OpenDirectory(temp_dir.GetPath());
  ASSERT_TRUE(dir);
}

// OpenDirectory() should fail when opening a directory that doesn't exist.
TEST_F(OpenDirectoryTest, OpenNonExistent) {
  auto dir = OpenDirectory(temp_dir.GetPath().AppendASCII("non_existent"));
  ASSERT_FALSE(dir);
}

// OpenDirectory() should open only directories.
TEST_F(OpenDirectoryTest, OpenFile) {
  auto file_path = temp_dir.GetPath().AppendASCII("test_file");
  ASSERT_TRUE(WriteFile(file_path, "foo", 3));
  auto dir = OpenDirectory(file_path);
  ASSERT_FALSE(dir);
}

}  // namespace fuchsia
}  // namespace base