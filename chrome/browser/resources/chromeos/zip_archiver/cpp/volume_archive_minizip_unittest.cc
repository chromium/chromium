// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive_minizip.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/minizip/src/ioapi.h"

namespace {

class TestVolumeReader : public VolumeReader {
 public:
  explicit TestVolumeReader(base::FilePath path)
      : file_(path, base::File::FLAG_OPEN | base::File::FLAG_READ) {}

  int64_t Read(int64_t bytes_to_read,
               const void** destination_buffer) override {
    buffer_.resize(bytes_to_read);
    *destination_buffer = buffer_.data();
    return file_.ReadAtCurrentPos(buffer_.data(), bytes_to_read);
  }

  int64_t Seek(int64_t offset, base::File::Whence whence) override {
    return file_.Seek(whence, offset);
  }

  std::unique_ptr<std::string> Passphrase() override { return nullptr; }

  int64_t offset() override { return Seek(0, base::File::FROM_CURRENT); }

  int64_t archive_size() override { return file_.GetLength(); }

 private:
  base::File file_;
  std::vector<char> buffer_;
};

class VolumeArchiveMinizipTest : public testing::Test {
 public:
  VolumeArchiveMinizipTest() = default;

  base::FilePath GetTestZipPath(const std::string& name) {
    base::FilePath root_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

    base::FilePath full_path =
        root_path
            .Append("chrome/browser/resources/chromeos/zip_archiver/test/data")
            .Append(name);
    CHECK(base::PathExists(full_path)) << full_path.value();
    return full_path;
  }

  void SetUp() override {}

  void TearDown() override {}

 private:
  std::unique_ptr<VolumeArchiveMinizip> archive_;
};

struct FileInfo {
  int64_t size;
  bool is_directory;
  time_t mod_time;
  const char* md5_sum;
};
const std::map<std::string, FileInfo> kSmallZipFiles = {
    {"file1", {15, false, 1407912954, "b4d9b82bb1cd97aa6191843149df18e6"}},
    {"file2", {33, false, 1407912974, "b864e9456deb246b018c49ef831f7ca7"}},
    {"dir/", {0, true, 1407913020, nullptr}},
    {"dir/file3", {56, false, 1407913020, "bffbca4992b32db8ed72bfc2c88e7f11"}},
};

TEST_F(VolumeArchiveMinizipTest, Basic) {
  std::unique_ptr<TestVolumeReader> reader =
      std::make_unique<TestVolumeReader>(GetTestZipPath("small_zip.zip"));
  VolumeArchiveMinizip archive(std::move(reader));
  ASSERT_TRUE(archive.Init(""));

  auto file_infos = kSmallZipFiles;
  while (true) {
    std::string file_path;
    bool is_utf8 = false;
    int64_t size = -1;
    bool is_directory = false;
    time_t mod_time = 0;
    auto result = archive.GetCurrentFileInfo(&file_path, &is_utf8, &size,
                                             &is_directory, &mod_time);
    EXPECT_EQ(result, VolumeArchive::RESULT_SUCCESS);
    FileInfo fi = file_infos[file_path];
    EXPECT_EQ(size, fi.size);
    EXPECT_EQ(is_directory, fi.is_directory);
    EXPECT_EQ(mod_time, fi.mod_time);
    file_infos.erase(file_path);

    result = archive.GoToNextFile();
    if (result == VolumeArchive::RESULT_EOF)
      break;
    ASSERT_EQ(result, VolumeArchive::RESULT_SUCCESS);
  }
  EXPECT_TRUE(file_infos.empty());
}

TEST_F(VolumeArchiveMinizipTest, SeekHeader) {
  std::unique_ptr<TestVolumeReader> reader =
      std::make_unique<TestVolumeReader>(GetTestZipPath("small_zip.zip"));
  VolumeArchiveMinizip archive(std::move(reader));
  ASSERT_TRUE(archive.Init(""));

  for (auto it : kSmallZipFiles) {
    EXPECT_TRUE(archive.SeekHeader(it.first));

    std::string file_path;
    bool is_utf8 = false;
    int64_t size = -1;
    bool is_directory = false;
    time_t mod_time = 0;
    auto result = archive.GetCurrentFileInfo(&file_path, &is_utf8, &size,
                                             &is_directory, &mod_time);
    EXPECT_EQ(result, VolumeArchive::RESULT_SUCCESS);
    EXPECT_EQ(file_path, it.first);
    EXPECT_EQ(size, it.second.size);
    EXPECT_EQ(is_directory, it.second.is_directory);
    EXPECT_EQ(mod_time, it.second.mod_time);
  }
}

TEST_F(VolumeArchiveMinizipTest, SeekHeader_NonExistant) {
  std::unique_ptr<TestVolumeReader> reader =
      std::make_unique<TestVolumeReader>(GetTestZipPath("small_zip.zip"));
  VolumeArchiveMinizip archive(std::move(reader));
  ASSERT_TRUE(archive.Init(""));

  EXPECT_FALSE(archive.SeekHeader("file4"));
  EXPECT_FALSE(archive.SeekHeader("dir/file4"));
  EXPECT_FALSE(archive.SeekHeader("dir2/"));
}

TEST_F(VolumeArchiveMinizipTest, Read) {
  std::unique_ptr<TestVolumeReader> reader =
      std::make_unique<TestVolumeReader>(GetTestZipPath("small_zip.zip"));
  VolumeArchiveMinizip archive(std::move(reader));
  ASSERT_TRUE(archive.Init(""));

  for (auto it : kSmallZipFiles) {
    EXPECT_TRUE(archive.SeekHeader(it.first));
    if (it.second.is_directory)
      continue;

    base::MD5Context ctx;
    base::MD5Init(&ctx);
    const char* buffer = nullptr;
    int64_t offset = 0;
    while (offset < it.second.size) {
      int64_t read = archive.ReadData(offset, it.second.size - offset, &buffer);
      ASSERT_GT(read, 0);
      EXPECT_LE(read, it.second.size - offset);
      base::MD5Update(&ctx, base::StringPiece(buffer, read));
      offset += read;
    }
    EXPECT_EQ(it.second.size, offset);
    EXPECT_EQ(0, archive.ReadData(offset, 1, &buffer));

    base::MD5Digest digest;
    base::MD5Final(&digest, &ctx);
    std::string md5_sum = base::MD5DigestToBase16(digest);
    EXPECT_EQ(md5_sum, it.second.md5_sum);
  }
}

}  // namespace
