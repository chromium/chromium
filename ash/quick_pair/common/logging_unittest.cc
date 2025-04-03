// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/logging.h"

#include <stddef.h>

#include "ash/quick_pair/common/log_buffer.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

namespace {

const char kLog1[] = "Mahogony destined to make a sturdy table";
const char kLog2[] = "Construction grade cedar";
const char kLog3[] = "Pine infested by hungry beetles";
const char kLog4[] = "Unremarkable maple";

// Called for every log message added to the standard logging system. The new
// log is saved in |g_standard_logs| and consumed so it does not flood stdout.
base::LazyInstance<std::vector<std::string>>::DestructorAtExit g_standard_logs =
    LAZY_INSTANCE_INITIALIZER;
bool HandleStandardLogMessage(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str) {
  g_standard_logs.Get().push_back(str);
  return true;
}

}  // namespace

class QuickPairLoggingTest : public testing::Test {
 public:
  void SetUp() override {
    LogBuffer::GetInstance()->Clear();
    g_standard_logs.Get().clear();

    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&HandleStandardLogMessage);
  }

  void TearDown() override { logging::SetLogMessageHandler(previous_handler_); }

 private:
  logging::LogMessageHandlerFunction previous_handler_{nullptr};
};

TEST_F(QuickPairLoggingTest, LogsSavedToBuffer) {
  int base_line_number = __LINE__;
  QP_LOG(INFO) << kLog1;
  QP_LOG(WARNING) << kLog2;
  QP_LOG(ERROR) << kLog3;
  QP_LOG(VERBOSE) << kLog3;

  auto* logs = LogBuffer::GetInstance()->logs();
  ASSERT_EQ(4u, logs->size());

  auto iterator = logs->begin();
  const LogBuffer::LogMessage& log_message1 = *iterator;
  EXPECT_EQ(kLog1, log_message1.text);
  EXPECT_EQ(__FILE__, log_message1.file);
  EXPECT_EQ(base_line_number + 1, log_message1.line);
  EXPECT_EQ(logging::LOGGING_INFO, log_message1.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message2 = *iterator;
  EXPECT_EQ(kLog2, log_message2.text);
  EXPECT_EQ(__FILE__, log_message2.file);
  EXPECT_EQ(base_line_number + 2, log_message2.line);
  EXPECT_EQ(logging::LOGGING_WARNING, log_message2.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message3 = *iterator;
  EXPECT_EQ(kLog3, log_message3.text);
  EXPECT_EQ(__FILE__, log_message3.file);
  EXPECT_EQ(base_line_number + 3, log_message3.line);
  EXPECT_EQ(logging::LOGGING_ERROR, log_message3.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message4 = *iterator;
  EXPECT_EQ(kLog3, log_message4.text);
  EXPECT_EQ(__FILE__, log_message4.file);
  EXPECT_EQ(base_line_number + 4, log_message4.line);
  EXPECT_EQ(logging::LOGGING_VERBOSE, log_message4.severity);
}

TEST_F(QuickPairLoggingTest, LogWhenBufferIsFull) {
  LogBuffer* log_buffer = LogBuffer::GetInstance();
  EXPECT_EQ(0u, log_buffer->logs()->size());

  for (size_t i = 0; i < log_buffer->MaxBufferSize(); ++i) {
    QP_LOG(INFO) << "log " << i;
  }

  EXPECT_EQ(log_buffer->MaxBufferSize(), log_buffer->logs()->size());
  QP_LOG(INFO) << kLog1;
  EXPECT_EQ(log_buffer->MaxBufferSize(), log_buffer->logs()->size());

  auto iterator = log_buffer->logs()->begin();
  for (size_t i = 0; i < log_buffer->MaxBufferSize() - 1; ++iterator, ++i) {
    std::string expected_text = "log " + base::NumberToString(i + 1);
    EXPECT_EQ(expected_text, (*iterator).text);
  }
  EXPECT_EQ(kLog1, (*iterator).text);
}

TEST_F(QuickPairLoggingTest, StandardLogsCreated) {
  QP_LOG(INFO) << kLog1;
  QP_LOG(WARNING) << kLog2;
  QP_LOG(ERROR) << kLog3;
  QP_LOG(VERBOSE) << kLog4;

  ASSERT_EQ(3u, g_standard_logs.Get().size());
  EXPECT_NE(std::string::npos, g_standard_logs.Get()[0].find(kLog1));
  EXPECT_NE(std::string::npos, g_standard_logs.Get()[1].find(kLog2));
  EXPECT_NE(std::string::npos, g_standard_logs.Get()[2].find(kLog3));
}

}  // namespace quick_pair
}  // namespace ash
