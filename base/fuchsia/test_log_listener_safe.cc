// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_log_listener_safe.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

TestLogListenerSafe::TestLogListenerSafe() = default;

TestLogListenerSafe::~TestLogListenerSafe() = default;

void TestLogListenerSafe::set_on_log_message(
    base::RepeatingCallback<void(const fuchsia::logger::LogMessage&)>
        callback) {
  on_log_message_ = std::move(callback);
}

void TestLogListenerSafe::Log(fuchsia::logger::LogMessage message,
                              LogCallback callback) {
  if (on_log_message_)
    on_log_message_.Run(message);
  callback();
}

void TestLogListenerSafe::LogMany(
    std::vector<fuchsia::logger::LogMessage> messages,
    LogManyCallback callback) {
  for (const auto& message : messages)
    on_log_message_.Run(message);
  callback();
}

void TestLogListenerSafe::Done() {}

void TestLogListenerSafe::NotImplemented_(const std::string& name) {
  ADD_FAILURE() << "NotImplemented_: " << name;
}

SimpleTestLogListener::SimpleTestLogListener() : binding_(&listener_) {}

SimpleTestLogListener::~SimpleTestLogListener() = default;

void SimpleTestLogListener::ListenToLog(
    fuchsia::logger::Log* log,
    std::unique_ptr<fuchsia::logger::LogFilterOptions> options) {
  listener_.set_on_log_message(base::BindRepeating(
      &SimpleTestLogListener::PushLoggedMessage, base::Unretained(this)));
  log->ListenSafe(binding_.NewBinding(), std::move(options));
}

absl::optional<fuchsia::logger::LogMessage>
SimpleTestLogListener::RunUntilMessageReceived(
    base::StringPiece expected_string) {
  while (!logged_messages_.empty()) {
    fuchsia::logger::LogMessage message = logged_messages_.front();
    logged_messages_.pop_front();
    if (base::StringPiece(message.msg).find(expected_string) !=
        std::string::npos) {
      return message;
    }
  }

  absl::optional<fuchsia::logger::LogMessage> logged_message;
  base::RunLoop loop;
  binding_.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "LogListenerSafe disconnected";
        quit_loop.Run();
      });
  on_log_message_ = base::BindLambdaForTesting(
      [&logged_message, expected_string = std::string(expected_string),
       quit_loop =
           loop.QuitClosure()](const fuchsia::logger::LogMessage& message) {
        if (message.msg.find(expected_string) == std::string::npos)
          return;
        logged_message.emplace(message);
        quit_loop.Run();
      });

  loop.Run();

  binding_.set_error_handler({});
  on_log_message_ = {};

  return logged_message;
}

void SimpleTestLogListener::PushLoggedMessage(
    const fuchsia::logger::LogMessage& message) {
  DVLOG(1) << "TestLogListener received: " << message.msg;
  if (on_log_message_) {
    DCHECK(logged_messages_.empty());
    on_log_message_.Run(message);
  } else {
    logged_messages_.push_back(std::move(message));
  }
}

}  // namespace base
