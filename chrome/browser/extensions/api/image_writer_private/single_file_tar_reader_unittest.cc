// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h"

#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace image_writer {

class SingleFileTarReaderTest : public testing::Test,
                                public SingleFileTarReader::Delegate {
 public:
  SingleFileTarReaderTest() : reader_(this) {}

  SingleFileTarReaderTest(const SingleFileTarReaderTest&) = delete;
  SingleFileTarReaderTest& operator=(const SingleFileTarReaderTest&) = delete;
  ~SingleFileTarReaderTest() override = default;

  bool OpenTarFile(const base::FilePath& file_path) {
    infile_ = std::make_unique<base::File>(
        file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    return infile_->IsValid();
  }

  // SingleFileTarReader::Delegate:
  int ReadTarFile(char* data, int size, std::string* error_id) override {
    return infile_->ReadAtCurrentPos(data, size);
  }

  bool WriteContents(const char* data,
                     int size,
                     std::string* error_id) override {
    contents_.insert(contents_.end(), data, data + size);
    return true;
  }

  SingleFileTarReader& reader() { return reader_; }
  const std::vector<char>& contents() const { return contents_; }

 private:
  std::unique_ptr<base::File> infile_;
  std::vector<char> contents_;

  SingleFileTarReader reader_;
};

TEST_F(SingleFileTarReaderTest, ExtractTarFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  ASSERT_TRUE(OpenTarFile(test_data_dir.AppendASCII("test.tar")));

  while (!reader().IsComplete()) {
    EXPECT_TRUE(reader().ExtractChunk());
  }

  EXPECT_EQ(4u, reader().total_bytes());
  EXPECT_EQ(4u, reader().curr_bytes());
  EXPECT_EQ((std::vector<char>{'f', 'o', 'o', '\n'}), contents());
}

TEST_F(SingleFileTarReaderTest, ReadOctalNumber) {
  const char kNumber[12] = "00000000123";
  EXPECT_EQ(83u, SingleFileTarReader::ReadOctalNumber(kNumber, 12));

  const unsigned char kBigNumber[12] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x02, 0x0b, 0xc1, 0x3a, 0x00};
  EXPECT_EQ(8787147264u, SingleFileTarReader::ReadOctalNumber(
                             reinterpret_cast<const char*>(kBigNumber), 12));
}

}  // namespace image_writer
}  // namespace extensions
