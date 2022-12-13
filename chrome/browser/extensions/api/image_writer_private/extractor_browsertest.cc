// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace extensions {
namespace image_writer {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::StrictMock;

class ExtractorBrowserTest : public InProcessBrowserTest {
 protected:
  ExtractorBrowserTest() = default;
  ~ExtractorBrowserTest() override = default;

  void SetUpOnMainThread() override {
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
};

IN_PROC_BROWSER_TEST_F(ExtractorBrowserTest, ExtractTar) {
  base::ScopedAllowBlockingForTesting allow_blocking;

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

IN_PROC_BROWSER_TEST_F(ExtractorBrowserTest, ExtractTarXz) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("test.tar.xz");

  base::FilePath out_path;
  base::RunLoop run_loop;
  EXPECT_CALL(open_callback_, Run(_)).WillOnce(SaveArg<0>(&out_path));
  EXPECT_CALL(complete_callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  XzExtractor::Extract(std::move(properties_));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ("foo\n", contents);
}

IN_PROC_BROWSER_TEST_F(ExtractorBrowserTest, ExtractNonExistentTarXz) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("non_existent.tar.xz");

  base::RunLoop run_loop;
  EXPECT_CALL(failure_callback_, Run(error::kUnzipGenericError))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  XzExtractor::Extract(std::move(properties_));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ExtractorBrowserTest, ZeroByteTarXzFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Use a tar.xz containing an empty file.
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("empty_file.tar.xz");

  base::FilePath out_path;
  base::RunLoop run_loop;
  EXPECT_CALL(open_callback_, Run(_)).WillOnce(SaveArg<0>(&out_path));

  EXPECT_CALL(complete_callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Calling progress callback with total bytes == 0 causes 0 division, so it
  // should not be called.
  EXPECT_CALL(progress_callback_, Run(0, _)).Times(0);
  XzExtractor::Extract(std::move(properties_));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_TRUE(contents.empty());
}

IN_PROC_BROWSER_TEST_F(ExtractorBrowserTest, ExtractBigTarXzFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));
  properties_.image_path = test_data_dir.AppendASCII("2MBzeros.tar.xz");

  base::FilePath out_path;
  base::RunLoop run_loop;
  EXPECT_CALL(open_callback_, Run(_)).WillOnce(SaveArg<0>(&out_path));
  EXPECT_CALL(complete_callback_, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  XzExtractor::Extract(std::move(properties_));
  run_loop.Run();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path, &contents));
  EXPECT_EQ(contents, std::string(2097152, '\0'));
}

}  // namespace image_writer
}  // namespace extensions
