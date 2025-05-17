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
#include "base/strings/string_number_conversions.h"
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

// This case is to test against a UAF security issue,
// https://crbug.com/286210532. If it has to be disabled, for example for
// flakiness reasons, please be sure to leave a comment in
// https://crbug.com/286210532 to alert the test owners.
//
// More on the UAF, before CL https://crrev.com/c/4583920, AsyncLog::Append()
// would post a task that binds the WeakPtr to an AsyncLog object and its
// AsyncLog::AppendImpl() member function to call. In certain situations, as is
// documented in https://crbug.com/286210532, the task would have started
// running, while the AsyncLog object is destroyed mid-execution. This would
// lead to UAF as operations to access member variables of AsyncLog used to be
// performed inside AsyncLog::AppendImpl(), before CL
// https://crrev.com/c/4583920 was landed.
//
// CL https://crrev.com/c/4583920 fixed this issue by changing AppendImpl() and
// CreateFile() to free functions in an anonymous namespace, which prevented
// access to member variables from an async task.
//
// This test was once disabled by chromium gardeners due to flakiness caused by
// a minor flaw. It used to be that all 10 AsyncLog objects inside the for loop
// was trying to write to the same file path asynchronously all at once. A
// DCHECK() failure would quickly emerge inside AsyncLog::CreateFile(), as
// AsyncLog was designed in a way that a unique file path was supposed to be
// handled by a unique AsyncLog object. Multiple AsyncLog objects handling the
// same file path was not expected. A follow-up CL fixed this flakiness,
// https://crrev.com/c/6550156.
TEST_F(AsyncLogTest, NoUseAfterFreeCrash) {
  const std::string new_line = "Line\n";

  // Simulate race conditions between the destruction of AsyncLog and the
  // execution of AppendImpl.
  for (size_t i = 0; i < 10; ++i) {
    auto unique_file_path = temp_dir_.GetPath().AppendASCII(
        std::string(kLogFileName) + "_" + base::NumberToString(i));
    auto log = std::make_unique<AsyncLog>(unique_file_path);
    log->Append(new_line);
  }

  // This should finish without crash.
  task_environment_.RunUntilIdle();
}

}  // namespace diagnostics
}  // namespace ash
