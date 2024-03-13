// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_log_listener_safe.h"

#include <lib/async/default.h>
#include <lib/fidl/cpp/box.h>
#include <lib/zx/clock.h>

#include <optional>
#include <string_view>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/callback_helpers.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestLogListenerSafe::TestLogListenerSafe() = default;

TestLogListenerSafe::~TestLogListenerSafe() = default;

void TestLogListenerSafe::set_on_log_message(
    base::RepeatingCallback<void(const fuchsia_logger::LogMessage&)> callback) {
  on_log_message_ = std::move(callback);
}

void TestLogListenerSafe::Log(
    TestLogListenerSafe::LogRequest& request,
    TestLogListenerSafe::LogCompleter::Sync& completer) {
  if (on_log_message_)
    on_log_message_.Run(request.log());
  completer.Reply();
}

void TestLogListenerSafe::LogMany(
    TestLogListenerSafe::LogManyRequest& request,
    TestLogListenerSafe::LogManyCompleter::Sync& completer) {
  for (const auto& message : request.log()) {
    on_log_message_.Run(message);
  }
  completer.Reply();
}

void TestLogListenerSafe::Done(
    TestLogListenerSafe::DoneCompleter::Sync& completer) {}

SimpleTestLogListener::SimpleTestLogListener() = default;
SimpleTestLogListener::~SimpleTestLogListener() = default;

void SimpleTestLogListener::ListenToLog(
    const fidl::Client<fuchsia_logger::Log>& log,
    std::unique_ptr<fuchsia_logger::LogFilterOptions> options) {
  auto listener_endpoints =
      fidl::CreateEndpoints<fuchsia_logger::LogListenerSafe>();
  ZX_CHECK(listener_endpoints.is_ok(), listener_endpoints.status_value())
      << "Failed to create listener endpoints";
  binding_.emplace(
      async_get_default_dispatcher(), std::move(listener_endpoints->server),
      &listener_, [](fidl::UnbindInfo info) {
        ZX_LOG(ERROR, info.status()) << "LogListenerSafe disconnected";
      });

  ignore_before_ = zx::clock::get_monotonic();
  listener_.set_on_log_message(base::BindRepeating(
      &SimpleTestLogListener::PushLoggedMessage, base::Unretained(this)));
  auto listen_safe_result =
      log->ListenSafe({{.log_listener = std::move(listener_endpoints->client),
                        .options = std::move(options)}});
  if (listen_safe_result.is_error()) {
    ZX_DLOG(ERROR, listen_safe_result.error_value().status())
        << "ListenSafe() failed";
  }
}

std::optional<fuchsia_logger::LogMessage>
SimpleTestLogListener::RunUntilMessageReceived(
    std::string_view expected_string) {
  while (!logged_messages_.empty()) {
    fuchsia_logger::LogMessage message = logged_messages_.front();
    logged_messages_.pop_front();
    if (std::string_view(message.msg()).find(expected_string) !=
        std::string::npos) {
      return message;
    }
  }

  std::optional<fuchsia_logger::LogMessage> logged_message;
  base::RunLoop loop;
  on_log_message_ = base::BindLambdaForTesting(
      [ignore_before = ignore_before_, &logged_message,
       expected_string = std::string(expected_string),
       quit_loop =
           loop.QuitClosure()](const fuchsia_logger::LogMessage& message) {
        if (zx::time(message.time()) < ignore_before) {
          return;
        }
        if (message.msg().find(expected_string) == std::string::npos) {
          return;
        }
        logged_message.emplace(message);
        quit_loop.Run();
      });

  loop.Run();

  on_log_message_ = NullCallback();

  return logged_message;
}

void SimpleTestLogListener::PushLoggedMessage(
    const fuchsia_logger::LogMessage& message) {
  DVLOG(1) << "TestLogListener received: " << message.msg();
  if (zx::time(message.time()) < ignore_before_) {
    return;
  }
  if (on_log_message_) {
    DCHECK(logged_messages_.empty());
    on_log_message_.Run(message);
  } else {
    logged_messages_.push_back(std::move(message));
  }
}

void ListenFilteredByCurrentProcessId(SimpleTestLogListener& listener) {
  // Connect the test LogListenerSafe to the Log.
  auto log_client_end = fuchsia_component::Connect<fuchsia_logger::Log>();
  ASSERT_TRUE(log_client_end.is_ok())
      << FidlConnectionErrorMessage(log_client_end);
  fidl::Client log_client(std::move(log_client_end.value()),
                          async_get_default_dispatcher());
  listener.ListenToLog(
      log_client,
      std::make_unique<fuchsia_logger::LogFilterOptions>(
          fuchsia_logger::LogFilterOptions{
              {.filter_by_pid = true,
               .pid = Process::Current().Pid(),
               .min_severity = fuchsia_logger::LogLevelFilter::kInfo}}));
}

}  // namespace base
