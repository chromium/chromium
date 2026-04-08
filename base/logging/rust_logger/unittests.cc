// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/logging/log_severity.h"
#include "base/logging/rust_logger/test_support.rs.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_log.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace base::test {
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
  EXPECT_CALL(log_, Log(logging::LOGGING_INFO, _, _, _,
                        testing::HasSubstr("test trace log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_, Log(logging::LOGGING_INFO, _, _, _,
                        testing::HasSubstr("test debug log")))
      .WillOnce(testing::Return(true));
#endif

  EXPECT_CALL(log_, Log(logging::LOGGING_INFO, _, _, _,
                        testing::HasSubstr("test info log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_, Log(logging::LOGGING_WARNING, _, _, _,
                        testing::HasSubstr("test warning log")))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(log_, Log(logging::LOGGING_ERROR, _, _, _,
                        testing::HasSubstr("test error log")))
      .WillOnce(testing::Return(true));

  log_trace_from_rust();
  log_debug_from_rust();
  log_info_from_rust();
  log_warning_from_rust();
  log_error_from_rust();
}

// TODO(crbug.com/374023535): Logging does not work in component builds.
#if defined(COMPONENT_BUILD)
#define MAYBE_Placeholders DISABLED_Placeholders
#else
#define MAYBE_Placeholders Placeholders
#endif
TEST_F(RustLogIntegrationTest, MAYBE_Placeholders) {
  EXPECT_CALL(log_, Log(logging::LOGGING_ERROR, _, _, _,
                        testing::HasSubstr("test log with placeholder 2")))
      .WillOnce(testing::Return(true));

  log_error_with_placeholder_from_rust(2);
}

// TODO(crbug.com/374023535): Logging does not work in component builds.
// TODO(crbug.com/497896152): Avoid failures that seem CFI-related and re-enable
TEST(RustLogIntegrationTestWithoutMocking, DISABLED_Panic) {
  std::string expected_msg;

  // Verify presence of `LOG(FATAL)`-specific prefix in the message.
  expected_msg += "\\bFATAL\\b.*base.logging.rust_logger.test_support.rs";
  expected_msg += "[\\s\\S]*";  // Skip over a newline

  // Verify presence of Rust-provided, generic panicking message
  expected_msg += "panicked at.*base.logging.rust_logger.test_support.rs";
  expected_msg += "[\\s\\S]*";  // Skip over a newline

  // Verify presence of the custom message passed to `panic!` (including the
  // placeholder).
  expected_msg += "panic with placeholder 123";

  BASE_EXPECT_DEATH(
      { base::test::panic_with_placeholder_from_rust(123); }, expected_msg);
}

}  // namespace
}  // namespace base::test
