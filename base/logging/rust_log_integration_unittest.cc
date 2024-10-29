// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/test/logging/test_rust_logger_consumer.rs.h"
#include "base/test/mock_log.h"

using testing::_;

namespace logging {
namespace {

class RustLogIntegrationTest : public testing::Test {
 public:
  void SetUp() override { log_.StartCapturingLogs(); }

  void TearDown() override { log_.StopCapturingLogs(); }

  base::test::MockLog log_;
};

// TODO(crbug.com/374023535): Logging does not work in component builds.
#if defined(COMPONENT_BUILD)
#define MAYBE_CheckAllSeverity DISABLED_CheckAllSeverity
#else
#define MAYBE_CheckAllSeverity CheckAllSeverity
#endif
TEST_F(RustLogIntegrationTest, MAYBE_CheckAllSeverity) {
#if DCHECK_IS_ON()
  // Debug and Trace logs from Rust are discarded when DCHECK_IS_ON() is false;
  // otherwise, they are logged as info.
  EXPECT_CALL(log_,
              Log(LOGGING_INFO, _, _, _, testing::HasSubstr("test trace log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_,
              Log(LOGGING_INFO, _, _, _, testing::HasSubstr("test debug log")))
      .WillOnce(testing::Return(true));
#endif

  EXPECT_CALL(log_,
              Log(LOGGING_INFO, _, _, _, testing::HasSubstr("test info log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_, Log(LOGGING_WARNING, _, _, _,
                        testing::HasSubstr("test warning log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_,
              Log(LOGGING_ERROR, _, _, _, testing::HasSubstr("test error log")))
      .WillOnce(testing::Return(true));

  base::test::print_test_trace_log();
  base::test::print_test_debug_log();
  base::test::print_test_info_log();
  base::test::print_test_warning_log();
  base::test::print_test_error_log();
}

// TODO(crbug.com/374023535): Logging does not work in component builds.
#if defined(COMPONENT_BUILD)
#define MAYBE_Placeholders DISABLED_Placeholders
#else
#define MAYBE_Placeholders Placeholders
#endif
TEST_F(RustLogIntegrationTest, MAYBE_Placeholders) {
  EXPECT_CALL(log_, Log(LOGGING_ERROR, _, _, _,
                        testing::HasSubstr("test log with placeholder 2")))
      .WillOnce(testing::Return(true));

  base::test::print_test_error_log_with_placeholder(2);
}

}  // namespace
}  // namespace logging
