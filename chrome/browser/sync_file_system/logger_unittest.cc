// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/logger.h"
#include "testing/gtest/include/gtest/gtest.h"

using drive::EventLogger;

namespace sync_file_system {

namespace {

// Logs one event at each supported LogSeverity level.
void LogSampleEvents() {
  util::Log(logging::LOGGING_INFO, FROM_HERE, "Info test message");
  util::Log(logging::LOGGING_WARNING, FROM_HERE, "Warning test message");
  util::Log(logging::LOGGING_ERROR, FROM_HERE, "Error test message");
}

bool ContainsString(const std::string& contains_string,
                    EventLogger::Event event) {
  return event.what.find(contains_string) != std::string::npos;
}

}  // namespace

class LoggerTest : public testing::Test {
 public:
  LoggerTest() {}

  LoggerTest(const LoggerTest&) = delete;
  LoggerTest& operator=(const LoggerTest&) = delete;

  void SetUp() override {
    util::ClearLog();
  }
};

TEST_F(LoggerTest, GetLogHistory) {
  LogSampleEvents();

  const std::vector<EventLogger::Event> log = util::GetLogHistory();
  ASSERT_EQ(3u, log.size());
  EXPECT_TRUE(ContainsString("Info test message", log[0]));
  EXPECT_TRUE(ContainsString("Warning test message", log[1]));
  EXPECT_TRUE(ContainsString("Error test message", log[2]));
}

TEST_F(LoggerTest, ClearLog) {
  LogSampleEvents();
  EXPECT_EQ(3u, util::GetLogHistory().size());

  util::ClearLog();
  EXPECT_EQ(0u, util::GetLogHistory().size());
}


}  // namespace sync_file_system
