// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/file_system_status.h"

#include <string.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class FileSystemStatusTest : public testing::Test {
 public:
  FileSystemStatusTest() = default;
  ~FileSystemStatusTest() override = default;

  FileSystemStatusTest(const FileSystemStatusTest&) = delete;
  FileSystemStatusTest& operator=(const FileSystemStatusTest&) = delete;

  void SetUp() override { ASSERT_TRUE(dir_.CreateUniqueTempDir()); }

 protected:
  static bool IsSystemImageExtFormat(const base::FilePath& path) {
    return FileSystemStatus::IsSystemImageExtFormatForTesting(path);
  }

  const base::FilePath& GetTempDir() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_FileMissing) {
  EXPECT_FALSE(IsSystemImageExtFormat(base::FilePath("/nonexistent")));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_FileSizeTooSmall) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  std::vector<uint8_t> data(100, 0);
  base::WriteFile(file, data);

  EXPECT_FALSE(IsSystemImageExtFormat(file));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_MagicNumberDoesNotMatch) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  std::vector<uint8_t> data(2048, 0);
  base::WriteFile(file, data);

  EXPECT_FALSE(IsSystemImageExtFormat(file));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_MagicNumberMatches) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  std::vector<uint8_t> data(2048, 0);
  // Magic signature (0xEF53) is in little-endian order.
  data[0x400 + 0x38] = 0x53;
  data[0x400 + 0x39] = 0xEF;
  base::WriteFile(file, data);

  EXPECT_TRUE(IsSystemImageExtFormat(file));
}

}  // namespace
}  // namespace arc
