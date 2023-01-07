// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/routine_log.h"

#include <string>
#include <vector>

#include "ash/system/diagnostics/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {
namespace {

const char kLogFileName[] = "diagnostic_routine_log";

}  // namespace

class RoutineLogTest : public testing::Test {
 public:
  RoutineLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~RoutineLogTest() override { base::RunLoop().RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(RoutineLogTest, Empty) {
  RoutineLog log(log_path_);

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(log_path_));
  EXPECT_TRUE(
      log.GetContentsForCategory(RoutineLog::RoutineCategory::kSystem).empty());
}

TEST_F(RoutineLogTest, Basic) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kCpuStress);

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents =
      log.GetContentsForCategory(RoutineLog::RoutineCategory::kSystem);
  const std::string first_line = GetLogLines(contents)[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("CpuStress", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);
}

TEST_F(RoutineLogTest, TwoLine) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kMemory);
  log.LogRoutineCompleted(mojom::RoutineType::kMemory,
                          mojom::StandardRoutineResult::kTestPassed);

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents =
      log.GetContentsForCategory(RoutineLog::RoutineCategory::kSystem);
  const std::vector<std::string> log_lines = GetLogLines(contents);
  const std::string first_line = log_lines[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("Memory", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);

  const std::string second_line = log_lines[1];
  const std::vector<std::string> second_line_contents =
      GetLogLineContents(second_line);

  ASSERT_EQ(3u, second_line_contents.size());
  EXPECT_EQ("Memory", second_line_contents[1]);
  EXPECT_EQ("Passed", second_line_contents[2]);
}

TEST_F(RoutineLogTest, Cancelled) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kMemory);
  log.LogRoutineCancelled(mojom::RoutineType::kMemory);

  // Ensure pending tasks complete.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::PathExists(log_path_));

  const std::string contents =
      log.GetContentsForCategory(RoutineLog::RoutineCategory::kSystem);
  LOG(ERROR) << contents;
  const std::vector<std::string> log_lines = GetLogLines(contents);

  ASSERT_EQ(2u, log_lines.size());
  const std::string second_line = log_lines[1];
  const std::vector<std::string> second_line_contents =
      GetLogLineContents(second_line);

  ASSERT_EQ(2u, second_line_contents.size());
  EXPECT_EQ("Inflight Routine Cancelled", second_line_contents[1]);
}

}  // namespace diagnostics
}  // namespace ash
