// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/zip_extractor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace image_writer {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::StrictMock;

class ExtractorTest : public testing::Test {
 protected:
  ExtractorTest() = default;
  ~ExtractorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    properties_.temp_dir_path = temp_dir_.GetPath();
    properties_.open_callback = open_callback_.Get();
    properties_.complete_callback = complete_callback_.Get();
    properties_.failure_callback = failure_callback_.Get();
    properties_.progress_callback = progress_callback_.Get();
  }

 protected:
  StrictMock<base::MockCallback<ExtractionProperties::OpenCallback>>
      open_callback_;
  StrictMock<base::MockCallback<ExtractionProperties::CompleteCallback>>
      complete_callback_;
  StrictMock<base::MockCallback<ExtractionProperties::FailureCallback>>
      failure_callback_;
  NiceMock<base::MockCallback<ExtractionProperties::ProgressCallback>>
      progress_callback_;

  ExtractionProperties properties_;
  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(ExtractorTest, ExtractTar) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("test.tar");

  base::FilePath out_path;
  base::RunLoop run_loop;
  EXPECT_CALL(open_callback_, Run(_)).WillOnce(SaveArg<0>(&out_path));
  EXPECT_CALL(complete_callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  TarExtractor::Extract(std::move(properties_));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(ExtractorTest, ExtractTarLargerThanChunk) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("test_large.tar");

  base::FilePath out_path;
  base::RunLoop run_loop;
  EXPECT_CALL(open_callback_, Run(_)).WillOnce(SaveArg<0>(&out_path));
  EXPECT_CALL(complete_callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  TarExtractor::Extract(std::move(properties_));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  // Larger than the buffer of SingleFileTarReader.
  EXPECT_EQ(10u * 1024u, contents.size());
}

TEST_F(ExtractorTest, ExtractNonExistentTar) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("non_existent.tar");

  base::RunLoop run_loop;
  EXPECT_CALL(failure_callback_, Run(error::kUnzipGenericError))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  TarExtractor::Extract(std::move(properties_));
  run_loop.Run();
}

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

// TODO(tetsui): Add a test of passing a non-tar file to TarExtractor.

}  // namespace image_writer
}  // namespace extensions
