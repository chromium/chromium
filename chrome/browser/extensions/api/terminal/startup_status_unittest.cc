// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/startup_status.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StartupStatusTest : public testing::Test {
 protected:
  void Print(const std::string& output) {
    output_.emplace_back(std::move(output));
  }

  std::unique_ptr<StartupStatusPrinter> NewStatusPrinter(bool verbose) {
    return std::make_unique<StartupStatusPrinter>(
        base::BindRepeating(&StartupStatusTest::Print, base::Unretained(this)),
        verbose);
  }

  std::vector<std::string> output_;
};

TEST_F(StartupStatusTest, TestNotVerbose) {
  auto status_printer = NewStatusPrinter(false);
  status_printer->set_max_stage(10);
  status_printer->PrintStage(0, "Starting Stage");
  status_printer->PrintStage(2, "Second Stage");
  status_printer->PrintStage(10, "Last Stage");
  status_printer->PrintSucceeded();

  ASSERT_EQ(output_.size(), 5u);

  // Hide cursor, init progress.
  EXPECT_EQ(output_[0], "\x1b[?25l\x1b[35m[          ] ");

  // CR, purple, progress, erase-right, yellow, empty-stage.
  EXPECT_EQ(output_[1], "\r\x1b[35m[          ] \x1b[K\x1b[33m ");

  // CR, purple, progress, erase-right, yellow, empty-stage.
  EXPECT_EQ(output_[2], "\r\x1b[35m[==        ] \x1b[K\x1b[33m ");

  // CR, purple, progress, erase-right, yellow, empty-stage.
  EXPECT_EQ(output_[3], "\r\x1b[35m[==========] \x1b[K\x1b[33m ");

  // CR, erase-right, default color, show cursor.
  EXPECT_EQ(output_[4], "\r\x1b[K\x1b[0m\x1b[?25h");
}

TEST_F(StartupStatusTest, TestVerbose) {
  auto status_printer = NewStatusPrinter(true);
  status_printer->set_max_stage(10);
  status_printer->PrintStage(0, "Starting Stage");
  status_printer->PrintStage(2, "Second Stage");
  status_printer->PrintStage(10, "Last Stage");
  status_printer->PrintSucceeded();

  ASSERT_EQ(output_.size(), 6u);

  // Hide cursor, init progress.
  EXPECT_EQ(output_[0], "\x1b[?25l\x1b[35m[          ] ");

  // CR, purple, progress, erase-right, yellow, stage.
  EXPECT_EQ(output_[1], "\r\x1b[35m[          ] \x1b[K\x1b[33mStarting Stage ");

  // CR, purple, progress, erase-right, yellow, stage.
  EXPECT_EQ(output_[2], "\r\x1b[35m[==        ] \x1b[K\x1b[33mSecond Stage ");

  // CR, purple, progress, erase-right, yellow, stage.
  EXPECT_EQ(output_[3], "\r\x1b[35m[==========] \x1b[K\x1b[33mLast Stage ");

  // CR, purple, progress, erase-right, yellow, stage.
  EXPECT_EQ(output_[4], "\r\x1b[35m[==========] \x1b[K\x1b[1;32mReady\r\n ");

  // CR, erase-right, default color, show cursor.
  EXPECT_EQ(output_[5], "\r\x1b[K\x1b[0m\x1b[?25h");
}

TEST_F(StartupStatusTest, TestError) {
  auto status_printer = NewStatusPrinter(false);
  status_printer->set_max_stage(10);
  status_printer->PrintStage(1, "First Stage");
  status_printer->PrintError("Error message");  // Prints two things.

  ASSERT_EQ(output_.size(), 4u);

  // Hide cursor, init progress.
  EXPECT_EQ(output_[0], "\x1b[?25l\x1b[35m[          ] ");
  // CR, purple, progress, erase-right, yellow, stage.
  EXPECT_EQ(output_[1], "\r\x1b[35m[=         ] \x1b[K\x1b[33m ");

  // CR, Move forward 14 characters, red, error message.
  EXPECT_EQ(output_[2], "\r\x1b[14C\x1b[1;31mError message");

  // CR, erase-right, default color, show cursor.
  EXPECT_EQ(output_[3], "\r\x1b[K\x1b[0m\x1b[?25h");
}

}  // namespace extensions
