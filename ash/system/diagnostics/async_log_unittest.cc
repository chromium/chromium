// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/async_log.h"

#include "ash/system/diagnostics/log_test_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

const char kLogFileName[] = "test_async_log";

}  // namespace

class AsyncLogTest : public testing::Test {
 public:
  AsyncLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~AsyncLogTest() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(AsyncLogTest, NoWriteEmpty) {
  AsyncLog log(log_path_);

  // The file won't until it is written to.
  EXPECT_FALSE(base::PathExists(log_path_));

  // The log is empty.
  EXPECT_TRUE(log.GetContents().empty());
}

TEST_F(AsyncLogTest, WriteEmpty) {
  AsyncLog log(log_path_);

  // Append empty string to the log.
  log.Append("");

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  // The file exists.
  EXPECT_TRUE(base::PathExists(log_path_));

  // But log is still empty.
  EXPECT_TRUE(log.GetContents().empty());
}

TEST_F(AsyncLogTest, WriteOneLine) {
  AsyncLog log(log_path_);

  const std::string line = "Hello";

  // Append `line` to the log.
  log.Append(line);

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  // Log contains `line`.
  EXPECT_EQ(line, log.GetContents());
}

TEST_F(AsyncLogTest, WriteMultipleLines) {
  AsyncLog log(log_path_);

  const std::vector<std::string> lines = {
      "Line 1",
      "Line 2",
      "Line 3",
  };

  // Append all the `lines` with a new line to the log.
  for (auto line : lines) {
    log.Append(line + "\n");
  }

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  // Read back the log and split the lines.
  EXPECT_EQ(lines, GetLogLines(log.GetContents()));
}

}  // namespace diagnostics
}  // namespace ash
