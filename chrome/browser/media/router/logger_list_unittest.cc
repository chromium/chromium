// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/logger_list.h"

#include "base/json/json_reader.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class LoggerListTest : public testing::Test {
 protected:
  std::string GetAttributeOfFirstEntry(const std::string& logs_json,
                                       const std::string& attribute) {
    base::Value logs = base::JSONReader::Read(logs_json).value();
    return *logs.GetList()[0].GetDict().FindString(attribute);
  }

  // Must be on Chrome_UIThread, as adding/removing loggers to/from LoggerList
  // can only be done on UI thread.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LoggerListTest, AddingAndRemovingLoggers) {
  LoggerImpl logger1, logger2, logger3;
  LoggerList* logger_list = LoggerList::GetInstance();
  EXPECT_EQ(logger_list->GetLoggerCount(), 0);
  logger_list->AddLogger(&logger1);
  EXPECT_EQ(logger_list->GetLoggerCount(), 1);
  logger_list->AddLogger(&logger2);
  EXPECT_EQ(logger_list->GetLoggerCount(), 2);
  logger_list->AddLogger(&logger1);
  EXPECT_EQ(logger_list->GetLoggerCount(), 2);
  logger_list->RemoveLogger(&logger1);
  EXPECT_EQ(logger_list->GetLoggerCount(), 1);
  logger_list->RemoveLogger(&logger1);
  EXPECT_EQ(logger_list->GetLoggerCount(), 1);
  logger_list->AddLogger(&logger3);
  EXPECT_EQ(logger_list->GetLoggerCount(), 2);
  logger_list->RemoveLogger(&logger3);
  EXPECT_EQ(logger_list->GetLoggerCount(), 1);
  logger_list->RemoveLogger(&logger2);
  EXPECT_EQ(logger_list->GetLoggerCount(), 0);
}

TEST_F(LoggerListTest, Log) {
  LoggerImpl logger1, logger2;
  LoggerList* logger_list = LoggerList::GetInstance();
  logger_list->AddLogger(&logger1);
  logger_list->AddLogger(&logger2);

  logger_list->Log(LoggerImpl::Severity::kInfo, mojom::LogCategory::kDiscovery,
                   "MyComponent", "My message", "cast:12345", "cast:ABCDEFGH",
                   "cast:abcd67890");

  const std::string logs1 = logger1.GetLogsAsJson();
  const std::string logs2 = logger2.GetLogsAsJson();

  std::string time_field = GetAttributeOfFirstEntry(logs1, "time");
  const std::string expected_logs =
      R"([
      {
        "severity": "Info",
        "category": "Discovery",
        "component": "MyComponent",
        "time": ")" +
      time_field + R"(",
        "message": "My message",
        "sinkId": "cast:1234",
        "mediaSource": "cast:ABCDEFGH",
        "sessionId": "cast:abcd"
      }
    ])";

  EXPECT_EQ(base::JSONReader::Read(logs1),
            base::JSONReader::Read(expected_logs));
  EXPECT_EQ(base::JSONReader::Read(logs2),
            base::JSONReader::Read(expected_logs));

  logger_list->RemoveLogger(&logger1);
  logger_list->RemoveLogger(&logger2);
}

}  // namespace media_router
