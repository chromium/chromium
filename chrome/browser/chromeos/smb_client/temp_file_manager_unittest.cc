// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {

namespace {

// Helper method to get the current offset of a file descriptor |fd|. Returns
// the offset from the beginning of the file.
int64_t GetCurrentOffset(int32_t fd) {
  return lseek(fd, 0, SEEK_CUR);
}

}  // namespace

class TempFileManagerTest : public testing::Test {
 public:
  TempFileManagerTest() = default;
  TempFileManagerTest(const TempFileManagerTest&) = delete;
  TempFileManagerTest& operator=(const TempFileManagerTest&) = delete;
  ~TempFileManagerTest() override = default;
};

// Should properly create a path when the object is created.
TEST_F(TempFileManagerTest, ShouldCreatePath) {
  TempFileManager file_manager;
  const base::FilePath& path = file_manager.GetTempDirectoryPath();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(base::PathExists(path));
}

// Path should be deleted after TempFileManager is cleaned up.
TEST_F(TempFileManagerTest, PathShouldBeDeleted) {
  std::string path;

  {
    TempFileManager file_manager;
    path = file_manager.GetTempDirectoryPath().value();
    EXPECT_TRUE(base::PathExists(base::FilePath(path)));
  }

  EXPECT_FALSE(base::PathExists(base::FilePath(path)));
}

// Should properly create a file with data and return a valid ScopedFD.
TEST_F(TempFileManagerTest, ShouldCreateFile) {
  std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6};
  TempFileManager file_manager;
  base::ScopedFD fd = file_manager.CreateTempFile(data);

  EXPECT_TRUE(fd.is_valid());
}

// Should properly unlink directory path even if a file descriptor is open.
TEST_F(TempFileManagerTest, FileShouldBeUnlinked) {
  std::string path;

  {
    std::vector<uint8_t> data = {0, 1, 2, 3, 4, 5, 6};
    TempFileManager file_manager;
    path = file_manager.GetTempDirectoryPath().value();
    base::ScopedFD fd = file_manager.CreateTempFile(data);
  }

  // Directory should be unlinked.
  EXPECT_FALSE(base::PathExists(base::FilePath(path)));
}

// CreateFile should properly write the data into a file and seek back to the
// beginning of the file.
TEST_F(TempFileManagerTest, WriteFileSucceeds) {
  std::vector<uint8_t> expected = {0, 1, 2, 3, 4, 5, 6};
  TempFileManager file_manager;
  base::ScopedFD fd = file_manager.CreateTempFile(expected);

  // Expect the current offset to be at the beginning of the file.
  EXPECT_EQ(0, GetCurrentOffset(fd.get()));

  // Read back the data written and ensure that it is the same.
  std::vector<uint8_t> actual(expected.size());
  EXPECT_TRUE(base::ReadFromFD(fd.get(), reinterpret_cast<char*>(actual.data()),
                               actual.size()));
  EXPECT_EQ(expected, actual);
}

}  // namespace smb_client
}  // namespace chromeos
