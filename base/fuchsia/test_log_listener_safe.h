// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_
#define BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_

#include <fuchsia/logger/cpp/fidl_test_base.h>

#include <lib/fidl/cpp/binding.h>
#include <lib/zx/time.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// LogListenerSafe implementation that invokes a caller-supplied callback for
// each received message.
// Note that messages will be delivered in order of receipt from the system
// logger, starting with any recent messages that the logging service had
// cached, i.e. including messages that may pre-date this log-listener being
// created.
class TestLogListenerSafe final
    : public fuchsia::logger::testing::LogListenerSafe_TestBase {
 public:
  using OnLogMessageCallback =
      base::RepeatingCallback<void(const fuchsia::logger::LogMessage&)>;

  TestLogListenerSafe();
  ~TestLogListenerSafe() override;

  TestLogListenerSafe(const TestLogListenerSafe&) = delete;
  TestLogListenerSafe& operator=(const TestLogListenerSafe&) = delete;

  // Sets a callback to be invoked with every message received via Log().
  void set_on_log_message(OnLogMessageCallback callback);

 private:
  // LogListenerSafe implementation.
  void Log(fuchsia::logger::LogMessage message, LogCallback callback) override;
  void LogMany(std::vector<fuchsia::logger::LogMessage> messages,
               LogManyCallback callback) override;
  void Done() override;
  void NotImplemented_(const std::string& name) override;

  OnLogMessageCallback on_log_message_;
};

// Helper that manages a TestLogListenerSafe to simplify running the message
// loop until specific messages are received.
// Messages received prior to ListenToLog() being called will be silently
// ignored.
class SimpleTestLogListener {
 public:
  SimpleTestLogListener();
  ~SimpleTestLogListener();

  SimpleTestLogListener(const SimpleTestLogListener&) = delete;
  SimpleTestLogListener& operator=(const SimpleTestLogListener&) = delete;

  // Attaches this instance to receive data matching |options|, from |log|.
  void ListenToLog(fuchsia::logger::Log* log,
                   std::unique_ptr<fuchsia::logger::LogFilterOptions> options);

  // Runs the message loop until a log message containing |expected_string| is
  // received, and returns it. Returns |absl::nullopt| if |binding_| disconnects
  // without the |expected_string| having been logged.
  absl::optional<fuchsia::logger::LogMessage> RunUntilMessageReceived(
      base::StringPiece expected_string);

 private:
  // Pushes |message| to the |logged_messages_| queue, or to |on_log_message_|.
  void PushLoggedMessage(const fuchsia::logger::LogMessage& message);

  // Used to ignore messages with timestamps prior to this listener's creation.
  zx::time ignore_before_;

  TestLogListenerSafe listener_;
  fidl::Binding<fuchsia::logger::LogListenerSafe> binding_;

  base::circular_deque<fuchsia::logger::LogMessage> logged_messages_;
  TestLogListenerSafe::OnLogMessageCallback on_log_message_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_
