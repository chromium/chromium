// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/diagnostics/log_test_helpers.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

class LogHelpersTest : public testing::Test {
 public:
  LogHelpersTest() = default;

  ~LogHelpersTest() override = default;
};

TEST_F(LogHelpersTest, GetLogLineContents) {
  const std::string log_line = "02:26:18.766 - RoutineType::kMemory - Started";
  const std::vector<std::string> actual_log_lines =
      GetLogLineContents(log_line);
  ASSERT_EQ(3u, actual_log_lines.size());
  ASSERT_EQ("02:26:18.766", actual_log_lines[0]);
  ASSERT_EQ("RoutineType::kMemory", actual_log_lines[1]);
  ASSERT_EQ("Started", actual_log_lines[2]);

  const std::string log_line_no_separator =
      "02:26:18.766  RoutineType::kMemory  Started";
  const std::vector<std::string> actual_log_lines_no_separator =
      GetLogLineContents(log_line_no_separator);
  ASSERT_EQ(1u, actual_log_lines_no_separator.size());
  ASSERT_EQ("02:26:18.766  RoutineType::kMemory  Started",
            actual_log_lines_no_separator[0]);
}

TEST_F(LogHelpersTest, GetLogLines) {
  const std::string log_lines =
      "02:26:18.766 - RoutineType::kMemory - Started \n 02:26:18.766 - "
      "RoutineType::kMemory - Completed \n";
  const std::vector<std::string> actual_log_lines = GetLogLines(log_lines);
  ASSERT_EQ(2u, actual_log_lines.size());
  ASSERT_EQ("02:26:18.766 - RoutineType::kMemory - Started",
            actual_log_lines[0]);
  ASSERT_EQ("02:26:18.766 - RoutineType::kMemory - Completed",
            actual_log_lines[1]);

  const std::string log_lines_no_newline =
      "02:26:18.766 - RoutineType::kMemory - Started 02:26:18.766 - "
      "RoutineType::kMemory - Completed";
  const std::vector<std::string> actual_log_lines_no_newline =
      GetLogLines(log_lines_no_newline);
  ASSERT_EQ(1u, actual_log_lines_no_newline.size());
  ASSERT_EQ(
      "02:26:18.766 - RoutineType::kMemory - Started 02:26:18.766 - "
      "RoutineType::kMemory - Completed",
      actual_log_lines_no_newline[0]);
}

}  // namespace diagnostics
}  // namespace ash
