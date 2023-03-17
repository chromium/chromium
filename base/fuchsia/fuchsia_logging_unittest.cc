// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_fx_logger.h"

#include <fidl/base.testfidl/cpp/fidl.h>
#include <fidl/fuchsia.logger/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/test_log_listener_safe.h"
#include "base/logging.h"
#include "base/test/scoped_logging_settings.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

}  // namespace

// Verifies that calling the log macro goes to the Fuchsia system logs, by
// default.
TEST(FuchsiaLoggingTest, SystemLogging) {
  constexpr char kLogMessage[] = "This is FuchsiaLoggingTest.SystemLogging!";

  test::SingleThreadTaskEnvironment task_environment_{
      test::SingleThreadTaskEnvironment::MainThreadType::IO};
  SimpleTestLogListener listener;
  ListenFilteredByCurrentProcessId(listener);

  // Ensure that logging is directed to the system debug log.
  logging::ScopedLoggingSettings scoped_logging_settings;
  CHECK(logging::InitLogging({.logging_dest = logging::LOG_DEFAULT}));

  // Emit the test log message, and spin the loop until it is reported to the
  // test listener.
  LOG(ERROR) << kLogMessage;

  absl::optional<fuchsia_logger::LogMessage> logged_message =
      listener.RunUntilMessageReceived(kLogMessage);

  ASSERT_TRUE(logged_message.has_value());
  EXPECT_EQ(logged_message->severity(),
            static_cast<int32_t>(fuchsia_logger::LogLevelFilter::kError));
  ASSERT_EQ(logged_message->tags().size(), 1u);

  EXPECT_EQ(logged_message->tags()[0], "base_unittests__exec");
}

// Verifies that configuring a system logger with multiple tags works.
TEST(FuchsiaLoggingTest, SystemLoggingMultipleTags) {
  constexpr char kLogMessage[] =
      "This is FuchsiaLoggingTest.SystemLoggingMultipleTags!";
  const std::vector<StringPiece> kTags = {"tag1", "tag2"};

  test::SingleThreadTaskEnvironment task_environment_{
      test::SingleThreadTaskEnvironment::MainThreadType::IO};
  SimpleTestLogListener listener;
  ListenFilteredByCurrentProcessId(listener);

  // Connect the test LogListenerSafe to the Log.
  auto log_sink_client_end =
      fuchsia_component::Connect<fuchsia_logger::LogSink>();
  EXPECT_TRUE(log_sink_client_end.is_ok())
      << FidlConnectionErrorMessage(log_sink_client_end);

  // Create a logger with multiple tags and emit a message to it.
  ScopedFxLogger logger = ScopedFxLogger::CreateFromLogSink(
      std::move(log_sink_client_end.value()), kTags);
  logger.LogMessage("", 0, kLogMessage, FUCHSIA_LOG_ERROR);

  absl::optional<fuchsia_logger::LogMessage> logged_message =
      listener.RunUntilMessageReceived(kLogMessage);

  ASSERT_TRUE(logged_message.has_value());
  auto tags = std::vector<StringPiece>(logged_message->tags().begin(),
                                       logged_message->tags().end());
  EXPECT_EQ(tags, kTags);
}

// Verifies the Fuchsia-specific ZX_*() logging macros.
TEST(FuchsiaLoggingTest, FuchsiaLogging) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log())
      .Times(DCHECK_IS_ON() ? 2 : 1)
      .WillRepeatedly(testing::Return("log message"));

  logging::ScopedLoggingSettings scoped_logging_settings;
  logging::SetMinLogLevel(logging::LOGGING_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_EQ(DCHECK_IS_ON(), DLOG_IS_ON(INFO));

  ZX_LOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();
  ZX_DLOG(INFO, ZX_ERR_INTERNAL) << mock_log_source.Log();

  ZX_CHECK(true, ZX_ERR_INTERNAL);
  ZX_DCHECK(true, ZX_ERR_INTERNAL);
}

TEST(FuchsiaLoggingTest, ConnectionErrorMessage) {
  zx::result<fidl::ClientEnd<base_testfidl::TestInterface>> result =
      zx::error_result{ZX_ERR_PEER_CLOSED};

  EXPECT_EQ(
      "Failed to connect to base.testfidl.TestInterface: "
      "ZX_ERR_PEER_CLOSED",
      base::FidlConnectionErrorMessage(result));
}

}  // namespace base
