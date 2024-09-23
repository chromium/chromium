// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/async_log.h"

#include <memory>

#include "ash/system/diagnostics/log_test_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

const char kLogFileName[] = "test_async_log";

}  // namespace

class AsyncLogTest : public testing::Test {
 public:
  AsyncLogTest() : task_runner_(new base::TestSimpleTaskRunner) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~AsyncLogTest() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(AsyncLogTest, NoWriteEmpty) {
  AsyncLog log(log_path_);
  log.SetTaskRunnerForTesting(task_runner_);

  // The file won't until it is written to.
  EXPECT_FALSE(base::PathExists(log_path_));

  // The log is empty.
  EXPECT_TRUE(log.GetContents().empty());
}

TEST_F(AsyncLogTest, WriteEmpty) {
  AsyncLog log(log_path_);
  log.SetTaskRunnerForTesting(task_runner_);

  // Append empty string to the log.
  log.Append("");

  EXPECT_TRUE(task_runner_->HasPendingTask());
  // Ensure pending tasks complete.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // The file exists.
  EXPECT_TRUE(base::PathExists(log_path_));

  // But log is still empty.
  EXPECT_TRUE(log.GetContents().empty());
}

TEST_F(AsyncLogTest, WriteOneLine) {
  AsyncLog log(log_path_);
  log.SetTaskRunnerForTesting(task_runner_);

  const std::string line = "Hello";

  // Append `line` to the log.
  log.Append(line);

  // Ensure pending tasks complete.
  task_runner_->RunUntilIdle();

  // Log contains `line`.
  EXPECT_EQ(line, log.GetContents());
}

TEST_F(AsyncLogTest, WriteMultipleLines) {
  AsyncLog log(log_path_);
  log.SetTaskRunnerForTesting(task_runner_);

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
  task_runner_->RunUntilIdle();

  // Read back the log and split the lines.
  EXPECT_EQ(lines, GetLogLines(log.GetContents()));
}

// TODO(crbug.com/40918364): reenable this.
TEST_F(AsyncLogTest, DISABLED_NoUseAfterFreeCrash) {
  const std::string new_line = "Line\n";

  // Simulate race conditions between the destruction of AsyncLog and the
  // execution of AppendImpl.
  for (size_t i = 0; i < 10; ++i) {
    auto log = std::make_unique<AsyncLog>(log_path_);
    log->Append(new_line);
  }

  // This should finish without crash.
  task_environment_.RunUntilIdle();
}

}  // namespace diagnostics
}  // namespace ash
