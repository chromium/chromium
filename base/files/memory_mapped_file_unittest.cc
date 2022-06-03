// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {

namespace {

// Create a temporary buffer and fill it with a watermark sequence.
std::unique_ptr<uint8_t[]> CreateTestBuffer(size_t size, size_t offset) {
  std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
  for (size_t i = 0; i < size; ++i)
    buf.get()[i] = static_cast<uint8_t>((offset + i) % 253);
  return buf;
}

// Check that the watermark sequence is consistent with the |offset| provided.
bool CheckBufferContents(const uint8_t* data, size_t size, size_t offset) {
  std::unique_ptr<uint8_t[]> test_data(CreateTestBuffer(size, offset));
  return memcmp(test_data.get(), data, size) == 0;
}

class MemoryMappedFileTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    CreateTemporaryFile(&temp_file_path_);
  }

  void TearDown() override { EXPECT_TRUE(DeleteFile(temp_file_path_)); }

  void CreateTemporaryTestFile(size_t size) {
    File file(temp_file_path_,
              File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE);
    EXPECT_TRUE(file.IsValid());

    std::unique_ptr<uint8_t[]> test_data(CreateTestBuffer(size, 0));
    size_t bytes_written =
        file.Write(0, reinterpret_cast<char*>(test_data.get()), size);
    EXPECT_EQ(size, bytes_written);
    file.Close();
  }

  const FilePath temp_file_path() const { return temp_file_path_; }

 private:
  FilePath temp_file_path_;
};

TEST_F(MemoryMappedFileTest, MapWholeFileByPath) {
  const size_t kFileSize = 68 * 1024;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;
  ASSERT_TRUE(map.Initialize(temp_file_path()));
  ASSERT_EQ(kFileSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));
}

TEST_F(MemoryMappedFileTest, MapWholeFileByFD) {
  const size_t kFileSize = 68 * 1024;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;
  ASSERT_TRUE(map.Initialize(
      File(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ)));
  ASSERT_EQ(kFileSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));
}

TEST_F(MemoryMappedFileTest, MapSmallFile) {
  const size_t kFileSize = 127;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;
  ASSERT_TRUE(map.Initialize(temp_file_path()));
  ASSERT_EQ(kFileSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));
}

TEST_F(MemoryMappedFileTest, MapWholeFileUsingRegion) {
  const size_t kFileSize = 157 * 1024;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;

  File file(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ);
  ASSERT_TRUE(
      map.Initialize(std::move(file), MemoryMappedFile::Region::kWholeFile));
  ASSERT_EQ(kFileSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));
}

TEST_F(MemoryMappedFileTest, MapPartialRegionAtBeginning) {
  const size_t kFileSize = 157 * 1024;
  const size_t kPartialSize = 4 * 1024 + 32;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;

  File file(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ);
  MemoryMappedFile::Region region = {0, kPartialSize};
  ASSERT_TRUE(map.Initialize(std::move(file), region));
  ASSERT_EQ(kPartialSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kPartialSize, 0));
}

TEST_F(MemoryMappedFileTest, MapPartialRegionAtEnd) {
  const size_t kFileSize = 157 * 1024;
  const size_t kPartialSize = 5 * 1024 - 32;
  const size_t kOffset = kFileSize - kPartialSize;
  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;

  File file(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ);
  MemoryMappedFile::Region region = {kOffset, kPartialSize};
  ASSERT_TRUE(map.Initialize(std::move(file), region));
  ASSERT_EQ(kPartialSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kPartialSize, kOffset));
}

TEST_F(MemoryMappedFileTest, MapSmallPartialRegionInTheMiddle) {
  const size_t kFileSize = 157 * 1024;
  const size_t kOffset = 1024 * 5 + 32;
  const size_t kPartialSize = 8;

  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;

  File file(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ);
  MemoryMappedFile::Region region = {kOffset, kPartialSize};
  ASSERT_TRUE(map.Initialize(std::move(file), region));
  ASSERT_EQ(kPartialSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kPartialSize, kOffset));
}

TEST_F(MemoryMappedFileTest, MapLargePartialRegionInTheMiddle) {
  const size_t kFileSize = 157 * 1024;
  const size_t kOffset = 1024 * 5 + 32;
  const size_t kPartialSize = 16 * 1024 - 32;

  CreateTemporaryTestFile(kFileSize);
  MemoryMappedFile map;

  File file(temp_file_path(), File::FLAG_OPEN | File::FLAG_READ);
  MemoryMappedFile::Region region = {kOffset, kPartialSize};
  ASSERT_TRUE(map.Initialize(std::move(file), region));
  ASSERT_EQ(kPartialSize, map.length());
  ASSERT_TRUE(map.data() != nullptr);
  EXPECT_TRUE(map.IsValid());
  ASSERT_TRUE(CheckBufferContents(map.data(), kPartialSize, kOffset));
}

TEST_F(MemoryMappedFileTest, WriteableFile) {
  const size_t kFileSize = 127;
  CreateTemporaryTestFile(kFileSize);

  {
    MemoryMappedFile map;
    ASSERT_TRUE(map.Initialize(temp_file_path(), MemoryMappedFile::READ_WRITE));
    ASSERT_EQ(kFileSize, map.length());
    ASSERT_TRUE(map.data() != nullptr);
    EXPECT_TRUE(map.IsValid());
    ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));

    uint8_t* bytes = map.data();
    bytes[0] = 'B';
    bytes[1] = 'a';
    bytes[2] = 'r';
    bytes[kFileSize - 1] = '!';
    EXPECT_FALSE(CheckBufferContents(map.data(), kFileSize, 0));
    EXPECT_TRUE(CheckBufferContents(map.data() + 3, kFileSize - 4, 3));
  }

  int64_t file_size;
  ASSERT_TRUE(GetFileSize(temp_file_path(), &file_size));
  EXPECT_EQ(static_cast<int64_t>(kFileSize), file_size);

  std::string contents;
  ASSERT_TRUE(ReadFileToString(temp_file_path(), &contents));
  EXPECT_EQ("Bar", contents.substr(0, 3));
  EXPECT_EQ("!", contents.substr(kFileSize - 1, 1));
}

TEST_F(MemoryMappedFileTest, ExtendableFile) {
  const size_t kFileSize = 127;
  const size_t kFileExtend = 100;
  CreateTemporaryTestFile(kFileSize);

  {
    File file(temp_file_path(),
              File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE);
    MemoryMappedFile::Region region = {0, kFileSize + kFileExtend};
    MemoryMappedFile map;
    ASSERT_TRUE(map.Initialize(std::move(file), region,
                               MemoryMappedFile::READ_WRITE_EXTEND));
    EXPECT_EQ(kFileSize + kFileExtend, map.length());
    ASSERT_TRUE(map.data() != nullptr);
    EXPECT_TRUE(map.IsValid());
    ASSERT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));

    uint8_t* bytes = map.data();
    EXPECT_EQ(0, bytes[kFileSize + 0]);
    EXPECT_EQ(0, bytes[kFileSize + 1]);
    EXPECT_EQ(0, bytes[kFileSize + 2]);
    bytes[kFileSize + 0] = 'B';
    bytes[kFileSize + 1] = 'A';
    bytes[kFileSize + 2] = 'Z';
    EXPECT_TRUE(CheckBufferContents(map.data(), kFileSize, 0));
  }

  int64_t file_size;
  ASSERT_TRUE(GetFileSize(temp_file_path(), &file_size));
  EXPECT_LE(static_cast<int64_t>(kFileSize + 3), file_size);
  EXPECT_GE(static_cast<int64_t>(kFileSize + kFileExtend), file_size);

  std::string contents;
  ASSERT_TRUE(ReadFileToString(temp_file_path(), &contents));
  EXPECT_EQ("BAZ", contents.substr(kFileSize, 3));
}

}  // namespace

}  // namespace base
