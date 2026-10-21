// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/zip_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/zstd_extractor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::image_writer {

class ExtractorTest : public testing::Test {
 protected:
  ExtractorTest() = default;
  ~ExtractorTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ExtractorTest, IsZipFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  EXPECT_TRUE(ZipExtractor::IsZipFile(test_data_dir.AppendASCII("test.zip")));
  EXPECT_FALSE(ZipExtractor::IsZipFile(test_data_dir.AppendASCII("test.tar")));
  EXPECT_FALSE(
      ZipExtractor::IsZipFile(test_data_dir.AppendASCII("test.tar.xz")));

  // Tests files with incorrect extensions.
  const base::FilePath temp_dir = temp_dir_.GetPath();
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.zip"),
                             temp_dir.AppendASCII("test.tar")));
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar"),
                             temp_dir.AppendASCII("test.zip")));
  EXPECT_FALSE(ZipExtractor::IsZipFile(temp_dir.AppendASCII("test.zip")));
  EXPECT_TRUE(ZipExtractor::IsZipFile(temp_dir.AppendASCII("test.tar")));
}

TEST_F(ExtractorTest, IsTarFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  EXPECT_TRUE(TarExtractor::IsTarFile(test_data_dir.AppendASCII("test.tar")));
  EXPECT_FALSE(
      TarExtractor::IsTarFile(test_data_dir.AppendASCII("test.tar.xz")));
  EXPECT_FALSE(TarExtractor::IsTarFile(test_data_dir.AppendASCII("test.zip")));

  // Tests files with incorrect extensions.
  const base::FilePath temp_dir = temp_dir_.GetPath();
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar"),
                             temp_dir.AppendASCII("test.tar.xz")));
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar.xz"),
                             temp_dir.AppendASCII("test.tar")));
  EXPECT_FALSE(TarExtractor::IsTarFile(temp_dir.AppendASCII("test.tar")));
  EXPECT_TRUE(TarExtractor::IsTarFile(temp_dir.AppendASCII("test.tar.xz")));
}

TEST_F(ExtractorTest, IsXzFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  EXPECT_TRUE(XzExtractor::IsXzFile(test_data_dir.AppendASCII("test.tar.xz")));
  EXPECT_FALSE(XzExtractor::IsXzFile(test_data_dir.AppendASCII("test.tar")));
  EXPECT_FALSE(XzExtractor::IsXzFile(test_data_dir.AppendASCII("test.zip")));

  // Tests files with incorrect extensions.
  const base::FilePath temp_dir = temp_dir_.GetPath();
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar.xz"),
                             temp_dir.AppendASCII("test.tar")));
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar"),
                             temp_dir.AppendASCII("test.tar.xz")));
  EXPECT_FALSE(XzExtractor::IsXzFile(temp_dir.AppendASCII("test.tar.xz")));
  EXPECT_TRUE(XzExtractor::IsXzFile(temp_dir.AppendASCII("test.tar")));
}

TEST_F(ExtractorTest, IsZstdFile) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  const base::FilePath zstd_file = temp_dir_.GetPath().AppendASCII("test.zst");
  // RFC 8878 Zstandard frame magic number (0xFD2FB528 little-endian) + valid
  // single-segment raw block containing "foo\n".
  constexpr uint8_t kZstdTestFrame[] = {
      0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x04, 0x21, 0x00, 0x00, 'f', 'o', 'o', '\n',
  };
  ASSERT_TRUE(base::WriteFile(zstd_file, kZstdTestFrame));

  EXPECT_TRUE(ZstdExtractor::IsZstdFile(zstd_file));
  EXPECT_FALSE(
      ZstdExtractor::IsZstdFile(test_data_dir.AppendASCII("test.tar")));
  EXPECT_FALSE(
      ZstdExtractor::IsZstdFile(test_data_dir.AppendASCII("test.tar.xz")));
  EXPECT_FALSE(
      ZstdExtractor::IsZstdFile(test_data_dir.AppendASCII("test.zip")));

  // Tests files with incorrect extensions.
  const base::FilePath fake_tar = temp_dir_.GetPath().AppendASCII("fake.tar");
  const base::FilePath fake_zst = temp_dir_.GetPath().AppendASCII("fake.zst");
  EXPECT_TRUE(base::CopyFile(zstd_file, fake_tar));
  EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII("test.tar"), fake_zst));
  EXPECT_TRUE(ZstdExtractor::IsZstdFile(fake_tar));
  EXPECT_FALSE(ZstdExtractor::IsZstdFile(fake_zst));
}

TEST_F(ExtractorTest, ExtractZstdFile) {
  const base::FilePath zstd_file = temp_dir_.GetPath().AppendASCII("test.zst");
  constexpr uint8_t kZstdTestFrame[] = {
      0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x04, 0x21, 0x00, 0x00, 'f', 'o', 'o', '\n',
  };
  ASSERT_TRUE(base::WriteFile(zstd_file, kZstdTestFrame));

  ExtractionProperties properties;
  properties.image_path = zstd_file;
  properties.temp_dir_path = temp_dir_.GetPath();

  base::FilePath out_path;
  base::MockCallback<ExtractionProperties::OpenCallback> open_callback;
  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  base::MockCallback<ExtractionProperties::CompleteCallback> complete_callback;
  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;

  properties.open_callback = open_callback.Get();
  properties.progress_callback = progress_callback.Get();
  properties.complete_callback = complete_callback.Get();
  properties.failure_callback = failure_callback.Get();

  EXPECT_CALL(open_callback,
              Run(temp_dir_.GetPath().AppendASCII("extracted.bin")))
      .WillOnce(testing::SaveArg<0>(&out_path));
  EXPECT_CALL(progress_callback, Run(testing::Gt(0), 0)).Times(1);
  EXPECT_CALL(progress_callback, Run(testing::Gt(0), testing::Gt(0)))
      .Times(testing::AtLeast(1));
  EXPECT_CALL(complete_callback, Run()).Times(1);
  EXPECT_CALL(failure_callback, Run(testing::_)).Times(0);

  ZstdExtractor::Extract(std::move(properties));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(ExtractorTest, ExtractCorruptZstdFileFails) {
  const base::FilePath corrupt_zstd_file =
      temp_dir_.GetPath().AppendASCII("corrupt.zst");
  // Valid magic header followed by truncated/corrupt frame data.
  constexpr uint8_t kCorruptZstdFrame[] = {0x28, 0xb5, 0x2f, 0xfd, 0xff};
  ASSERT_TRUE(base::WriteFile(corrupt_zstd_file, kCorruptZstdFrame));

  ExtractionProperties properties;
  properties.image_path = corrupt_zstd_file;
  properties.temp_dir_path = temp_dir_.GetPath();

  base::MockCallback<ExtractionProperties::OpenCallback> open_callback;
  base::MockCallback<ExtractionProperties::ProgressCallback> progress_callback;
  base::MockCallback<ExtractionProperties::CompleteCallback> complete_callback;
  base::MockCallback<ExtractionProperties::FailureCallback> failure_callback;

  properties.open_callback = open_callback.Get();
  properties.progress_callback = progress_callback.Get();
  properties.complete_callback = complete_callback.Get();
  properties.failure_callback = failure_callback.Get();

  EXPECT_CALL(open_callback, Run(testing::_)).Times(1);
  EXPECT_CALL(progress_callback, Run(testing::Gt(0), 0)).Times(1);
  EXPECT_CALL(complete_callback, Run()).Times(0);
  EXPECT_CALL(failure_callback, Run(error::kUnzipGenericError)).Times(1);

  ZstdExtractor::Extract(std::move(properties));
}

// TODO(tetsui): Add a test of passing a non-tar file to TarExtractor.

}  // namespace extensions::image_writer
