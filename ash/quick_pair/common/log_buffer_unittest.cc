// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/log_buffer.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class QuickPairLogBufferTest : public testing::Test,
                               public LogBuffer::Observer {
 public:
  void SetUp() override {
    log_buffer_ = LogBuffer::GetInstance();
    log_buffer_->AddObserver(this);
  }

  void TearDown() override { log_buffer_->RemoveObserver(this); }

  void OnLogMessageAdded(const LogBuffer::LogMessage& log_message) override {
    log_messages_.push_back(log_message);
  }

  void OnLogBufferCleared() override { log_messages_.clear(); }

 protected:
  std::vector<LogBuffer::LogMessage> log_messages_;
  raw_ptr<LogBuffer> log_buffer_ = nullptr;
};

TEST_F(QuickPairLogBufferTest, ObserversNotifiedWhenLogsAdded) {
  LogBuffer::GetInstance()->AddLogMessage(LogBuffer::LogMessage(
      /*text=*/"text", /*time=*/base::Time::Now(), /*file=*/"file", /*line=*/0,
      /*severity=*/logging::LOGGING_WARNING));
  EXPECT_EQ(log_messages_.size(), 1u);
}

TEST_F(QuickPairLogBufferTest, ObserversNotifiedWhenLogBufferCleared) {
  LogBuffer::GetInstance()->Clear();
  EXPECT_EQ(log_messages_.size(), 0u);
}

}  // namespace quick_pair
}  // namespace ash
