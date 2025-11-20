// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::logging {

namespace {

using ::testing::_;
using ::testing::Return;

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

TEST(PALoggingTest, BasicLogging) {
  MockLogSource mock_log_source;
  constexpr int kTimes =
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      16;
#else
      8;
#endif
  EXPECT_CALL(mock_log_source, Log())
      .Times(kTimes)
      .WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOGGING_INFO);

  EXPECT_TRUE(PA_LOG_IS_ON(INFO));
  EXPECT_EQ(PA_BUILDFLAG(DCHECKS_ARE_ON), PA_DLOG_IS_ON(INFO));
  EXPECT_TRUE(PA_VLOG_IS_ON(0));

  PA_LOG(INFO) << mock_log_source.Log();
  PA_LOG_IF(INFO, true) << mock_log_source.Log();
  PA_PLOG(INFO) << mock_log_source.Log();
  PA_PLOG_IF(INFO, true) << mock_log_source.Log();
  PA_VLOG(0) << mock_log_source.Log();
  PA_VLOG_IF(0, true) << mock_log_source.Log();
  PA_VPLOG(0) << mock_log_source.Log();
  PA_VPLOG_IF(0, true) << mock_log_source.Log();

  PA_DLOG(INFO) << mock_log_source.Log();
  PA_DLOG_IF(INFO, true) << mock_log_source.Log();
  PA_DPLOG(INFO) << mock_log_source.Log();
  PA_DPLOG_IF(INFO, true) << mock_log_source.Log();
  PA_DVLOG(0) << mock_log_source.Log();
  PA_DVLOG_IF(0, true) << mock_log_source.Log();
  PA_DVPLOG(0) << mock_log_source.Log();
  PA_DVPLOG_IF(0, true) << mock_log_source.Log();
}

TEST(PALoggingTest, LogIsOn) {
  SetMinLogLevel(LOGGING_INFO);
  EXPECT_TRUE(PA_LOG_IS_ON(INFO));
  EXPECT_TRUE(PA_LOG_IS_ON(WARNING));
  EXPECT_TRUE(PA_LOG_IS_ON(ERROR));
  EXPECT_TRUE(PA_LOG_IS_ON(FATAL));
  EXPECT_TRUE(PA_LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_WARNING);
  EXPECT_FALSE(PA_LOG_IS_ON(INFO));
  EXPECT_TRUE(PA_LOG_IS_ON(WARNING));
  EXPECT_TRUE(PA_LOG_IS_ON(ERROR));
  EXPECT_TRUE(PA_LOG_IS_ON(FATAL));
  EXPECT_TRUE(PA_LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_ERROR);
  EXPECT_FALSE(PA_LOG_IS_ON(INFO));
  EXPECT_FALSE(PA_LOG_IS_ON(WARNING));
  EXPECT_TRUE(PA_LOG_IS_ON(ERROR));
  EXPECT_TRUE(PA_LOG_IS_ON(FATAL));
  EXPECT_TRUE(PA_LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOGGING_FATAL + 1);
  EXPECT_FALSE(PA_LOG_IS_ON(INFO));
  EXPECT_FALSE(PA_LOG_IS_ON(WARNING));
  EXPECT_FALSE(PA_LOG_IS_ON(ERROR));
  // PA_LOG_IS_ON(FATAL) should always be true.
  EXPECT_TRUE(PA_LOG_IS_ON(FATAL));
  // If PA_BUILDFLAG(DCHECKS_ARE_ON) then DFATAL is FATAL.
  EXPECT_EQ(PA_BUILDFLAG(DCHECKS_ARE_ON), PA_LOG_IS_ON(DFATAL));
}

TEST(PALoggingTest, LoggingIsLazyBySeverity) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOGGING_WARNING);

  EXPECT_FALSE(PA_LOG_IS_ON(INFO));
  EXPECT_FALSE(PA_DLOG_IS_ON(INFO));
  EXPECT_FALSE(PA_VLOG_IS_ON(1));

  PA_LOG(INFO) << mock_log_source.Log();
  PA_LOG_IF(INFO, false) << mock_log_source.Log();
  PA_PLOG(INFO) << mock_log_source.Log();
  PA_PLOG_IF(INFO, false) << mock_log_source.Log();
  PA_VLOG(1) << mock_log_source.Log();
  PA_VLOG_IF(1, true) << mock_log_source.Log();
  PA_VPLOG(1) << mock_log_source.Log();
  PA_VPLOG_IF(1, true) << mock_log_source.Log();

  PA_DLOG(INFO) << mock_log_source.Log();
  PA_DLOG_IF(INFO, true) << mock_log_source.Log();
  PA_DPLOG(INFO) << mock_log_source.Log();
  PA_DPLOG_IF(INFO, true) << mock_log_source.Log();
  PA_DVLOG(1) << mock_log_source.Log();
  PA_DVLOG_IF(1, true) << mock_log_source.Log();
  PA_DVPLOG(1) << mock_log_source.Log();
  PA_DVPLOG_IF(1, true) << mock_log_source.Log();
}

// Always log-to-stderr(RawLog) if message handler is not assigned.
TEST(PALoggingTest, LogIsAlwaysToStdErr) {
  MockLogSource mock_log_source_stderr;
  SetMinLogLevel(LOGGING_INFO);
  EXPECT_TRUE(PA_LOG_IS_ON(INFO));
  EXPECT_CALL(mock_log_source_stderr, Log()).Times(1).WillOnce(Return("foo"));
  PA_LOG(INFO) << mock_log_source_stderr.Log();
}

TEST(PALoggingTest, DebugLoggingReleaseBehavior) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  int debug_only_variable = 1;
#endif
  // These should avoid emitting references to |debug_only_variable|
  // in release mode.
  PA_DLOG_IF(INFO, debug_only_variable) << "test";
  PA_DLOG_ASSERT(debug_only_variable) << "test";
  PA_DPLOG_IF(INFO, debug_only_variable) << "test";
  PA_DVLOG_IF(1, debug_only_variable) << "test";
}

}  // namespace

}  // namespace partition_alloc::internal::logging
