// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace image_writer {

class ExtractorTest : public testing::Test {
 public:
  ExtractorTest() = default;
  ~ExtractorTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    properties_.temp_dir_path = temp_dir_.GetPath();
    properties_.open_callback =
        base::BindOnce(&ExtractorTest::OnOpenSuccess, base::Unretained(this));
    properties_.complete_callback =
        base::BindOnce(&ExtractorTest::OnComplete, base::Unretained(this));
    properties_.failure_callback =
        base::BindOnce(&ExtractorTest::OnError, base::Unretained(this));
    properties_.progress_callback =
        base::BindRepeating(&ExtractorTest::OnProgress, base::Unretained(this));
  }

  void OnOpenSuccess(const base::FilePath& out_path) { out_path_ = out_path; }

  void OnComplete() {
    is_complete_ = true;
    run_loop_.Quit();
  }

  void OnError(const std::string& error) {
    error_ = error;
    run_loop_.Quit();
  }

  void OnProgress(int64_t total_bytes, int64_t curr_bytes) {}

 protected:
  base::FilePath out_path_;
  bool is_complete_ = false;
  std::string error_;

  ExtractionProperties properties_;

  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;

  base::RunLoop run_loop_;
};

TEST_F(ExtractorTest, ExtractTar) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  properties_.image_path = test_data_dir.AppendASCII("test.tar");
  TarExtractor::Extract(std::move(properties_));
  run_loop_.Run();

  EXPECT_TRUE(is_complete_);
  EXPECT_TRUE(error_.empty());

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path_, &contents));
  EXPECT_EQ("foo\n", contents);
}

TEST_F(ExtractorTest, ExtractTarLargerThanChunk) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  properties_.image_path = test_data_dir.AppendASCII("test_large.tar");
  TarExtractor::Extract(std::move(properties_));
  run_loop_.Run();

  EXPECT_TRUE(is_complete_);
  EXPECT_TRUE(error_.empty());

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(out_path_, &contents));
  // Larger than the buffer of SingleFileTarReader.
  EXPECT_EQ(10u * 1024u, contents.size());
}

TEST_F(ExtractorTest, ExtractNonExistentTar) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(GetTestDataDirectory(&test_data_dir));

  properties_.image_path = test_data_dir.AppendASCII("non_existent.tar");
  TarExtractor::Extract(std::move(properties_));
  run_loop_.Run();

  EXPECT_FALSE(is_complete_);
  EXPECT_EQ(error::kUnzipGenericError, error_);
}

// TODO(tetsui): Add a test of passing a non-tar file to TarExtractor.

}  // namespace image_writer
}  // namespace extensions
