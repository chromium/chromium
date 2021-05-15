// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive_minizip.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_stream.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive_minizip.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestFileName[] = "test.txt";
const char kLargeTestFileName[] = "large.file";
const char kTestDirName[] = "foo";
const char kTestFileContent[] = "Hello, World!";
const std::string kLargeTestFileContent(1234567, 'a');

class TestCompressorStream : public CompressorStream {
 public:
  TestCompressorStream() = default;

  int64_t Flush() override { return 0; }

  int64_t Write(int64_t zip_offset,
                int64_t zip_length,
                const char* zip_buffer) override {
    CHECK_EQ(zip_offset, write_offset_);
    CHECK_GT(zip_length, 0);
    CHECK_NE(zip_buffer, nullptr);

    if (write_error_)
      return -1;

    write_buffer_.append(zip_buffer, zip_length);
    write_offset_ += zip_length;
    return zip_length;
  }

  int64_t WriteChunkDone(int64_t write_bytes) override {
    NOTREACHED();
    return -1;
  }

  int64_t Read(int64_t bytes_to_read, char* destination_buffer) override {
    if (read_buffer_.empty())
      return 0;

    int64_t read_length =
        std::min(bytes_to_read, static_cast<int64_t>(read_buffer_.size()));
    memcpy(destination_buffer, read_buffer_.data(), read_length);
    read_buffer_ = read_buffer_.substr(read_length);
    return read_length;
  }

  int64_t ReadFileChunkDone(int64_t read_bytes,
                            pp::VarArrayBuffer* buffer) override {
    NOTREACHED();
    return -1;
  }

  void SetReadBuffer(const std::string& buffer) { read_buffer_ = buffer; }
  void SetWriteError() { write_error_ = true; }

  const std::string& write_buffer() { return write_buffer_; }

 private:
  std::string write_buffer_;
  int64_t write_offset_ = 0;

  bool write_error_ = false;

  std::string read_buffer_;
};

class InMemoryVolumeReader : public VolumeReader {
 public:
  explicit InMemoryVolumeReader(const std::string& file) : file_(file) {}

  int64_t Read(int64_t bytes_to_read,
               const void** destination_buffer) override {
    if (file_offset_ >= static_cast<int64_t>(file_.size()))
      return 0;

    int64_t read_length = std::min(
        bytes_to_read, static_cast<int64_t>(file_.size()) - file_offset_);
    *destination_buffer = static_cast<const void*>(file_.data() + file_offset_);
    file_offset_ += read_length;
    return read_length;
  }

  int64_t Seek(int64_t offset, base::File::Whence whence) override {
    switch (whence) {
      case base::File::FROM_BEGIN:
        file_offset_ = offset;
        break;
      case base::File::FROM_CURRENT:
        file_offset_ += offset;
        break;
      case base::File::FROM_END:
        file_offset_ = file_.size() + offset;
        break;
    }
    return file_offset_;
  }

  absl::optional<std::string> Passphrase() override { return {}; }

  int64_t offset() override { return file_offset_; }

  int64_t archive_size() override { return file_.size(); }

 private:
  const std::string file_;
  int64_t file_offset_ = 0;
};

class CompressorArchiveMinizipTest : public testing::Test {
 public:
  CompressorArchiveMinizipTest() = default;

  void CheckZipContents(const std::string& volume,
                        const std::string& path,
                        const std::string& contents) {
    std::unique_ptr<InMemoryVolumeReader> reader =
        std::make_unique<InMemoryVolumeReader>(volume);
    VolumeArchiveMinizip archive(std::move(reader));
    ASSERT_TRUE(archive.Init(""));

    EXPECT_TRUE(archive.SeekHeader(path));
    const char* buffer = nullptr;
    int64_t offset = 0;
    while (offset < static_cast<int64_t>(contents.size())) {
      int64_t read =
          archive.ReadData(offset, contents.size() - offset, &buffer);
      ASSERT_GT(read, 0);
      EXPECT_EQ(contents.substr(offset, read), base::StringPiece(buffer, read));
      offset += read;
    }
    EXPECT_EQ(offset, static_cast<int64_t>(contents.size()));
  }

  void CheckZipMetadata(const std::string& volume,
                        const std::string& path,
                        int64_t size,
                        base::Time mod_time,
                        bool is_directory) {
    std::unique_ptr<InMemoryVolumeReader> reader =
        std::make_unique<InMemoryVolumeReader>(volume);
    VolumeArchiveMinizip archive(std::move(reader));
    ASSERT_TRUE(archive.Init(""));

    EXPECT_TRUE(archive.SeekHeader(path));
    std::string volume_file_path;
    bool volume_is_utf8 = false;
    int64_t volume_size = -1;
    bool volume_is_directory = false;
    time_t volume_mod_time = 0;
    auto result = archive.GetCurrentFileInfo(&volume_file_path, &volume_is_utf8,
                                             &volume_size, &volume_is_directory,
                                             &volume_mod_time);
    EXPECT_EQ(result, VolumeArchive::RESULT_SUCCESS);
    EXPECT_EQ(size, volume_size);
    EXPECT_EQ(mod_time.ToTimeT(), volume_mod_time);
    EXPECT_EQ(is_directory, volume_is_directory);
  }

 private:
  std::unique_ptr<CompressorArchiveMinizip> archive_;
};

TEST_F(CompressorArchiveMinizipTest, Create) {
  TestCompressorStream stream;
  CompressorArchiveMinizip archive(&stream);

  const base::Time add_time = base::Time::FromTimeT(1234567890);
  EXPECT_TRUE(archive.CreateArchive());
  stream.SetReadBuffer(kTestFileContent);
  EXPECT_TRUE(archive.AddToArchive(kTestFileName, sizeof(kTestFileContent) - 1,
                                   add_time, false));
  stream.SetReadBuffer(kLargeTestFileContent);
  EXPECT_TRUE(archive.AddToArchive(
      kLargeTestFileName, kLargeTestFileContent.size(), add_time, false));
  EXPECT_TRUE(archive.AddToArchive(kTestDirName, 0, add_time, true));

  EXPECT_TRUE(archive.CloseArchive(false));
  EXPECT_FALSE(stream.write_buffer().empty());

  CheckZipMetadata(stream.write_buffer(), kTestFileName,
                   sizeof(kTestFileContent) - 1, add_time, false);
  CheckZipMetadata(stream.write_buffer(), kLargeTestFileName,
                   kLargeTestFileContent.size(), add_time, false);
  CheckZipMetadata(stream.write_buffer(), std::string(kTestDirName) + "/", 0,
                   add_time, true);

  CheckZipContents(stream.write_buffer(), kTestFileName, kTestFileContent);
  CheckZipContents(stream.write_buffer(), kLargeTestFileName,
                   kLargeTestFileContent);
}

TEST_F(CompressorArchiveMinizipTest, Create_WriteError) {
  TestCompressorStream stream;
  CompressorArchiveMinizip archive(&stream);

  const base::Time add_time = base::Time::FromTimeT(1234567890);
  EXPECT_TRUE(archive.CreateArchive());
  stream.SetReadBuffer(kTestFileContent);
  EXPECT_TRUE(archive.AddToArchive(kTestFileName, sizeof(kTestFileContent) - 1,
                                   add_time, false));
  stream.SetReadBuffer(kLargeTestFileContent);
  stream.SetWriteError();
  EXPECT_FALSE(archive.AddToArchive(
      kLargeTestFileName, kLargeTestFileContent.size(), add_time, false));
  EXPECT_FALSE(archive.error_message().empty());
}

TEST_F(CompressorArchiveMinizipTest, CreateAndCancel) {
  TestCompressorStream stream;
  CompressorArchiveMinizip archive(&stream);

  const base::Time add_time = base::Time::FromTimeT(1234567890);
  EXPECT_TRUE(archive.CreateArchive());
  stream.SetReadBuffer(kTestFileContent);
  EXPECT_TRUE(archive.AddToArchive(kTestFileName, sizeof(kTestFileContent) - 1,
                                   add_time, false));
  stream.SetReadBuffer(kLargeTestFileContent);
  archive.CancelArchive();
  EXPECT_FALSE(archive.AddToArchive(
      kLargeTestFileName, kLargeTestFileContent.size(), add_time, false));
  EXPECT_TRUE(archive.error_message().empty());
}

}  // namespace
