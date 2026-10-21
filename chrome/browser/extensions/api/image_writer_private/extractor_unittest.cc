// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
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

// TODO(tetsui): Add a test of passing a non-tar file to TarExtractor.

}  // namespace extensions::image_writer
