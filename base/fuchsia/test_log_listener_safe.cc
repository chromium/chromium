// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_log_listener_safe.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestLogListenerSafe::TestLogListenerSafe() = default;

TestLogListenerSafe::~TestLogListenerSafe() = default;

void TestLogListenerSafe::set_on_dump_logs_done(
    base::OnceClosure on_dump_logs_done) {
  on_dump_logs_done_ = std::move(on_dump_logs_done);
}

bool TestLogListenerSafe::DidReceiveString(
    base::StringPiece message,
    fuchsia::logger::LogMessage* logged_message) {
  for (const auto& log_message : log_messages_) {
    if (log_message.msg.find(message.as_string()) != std::string::npos) {
      *logged_message = log_message;
      return true;
    }
  }
  return false;
}

void TestLogListenerSafe::LogMany(
    std::vector<fuchsia::logger::LogMessage> messages,
    LogManyCallback callback) {
  log_messages_.insert(log_messages_.end(),
                       std::make_move_iterator(messages.begin()),
                       std::make_move_iterator(messages.end()));
  callback();
}

void TestLogListenerSafe::Done() {
  std::move(on_dump_logs_done_).Run();
}

void TestLogListenerSafe::NotImplemented_(const std::string& name) {
  ADD_FAILURE() << "NotImplemented_: " << name;
}

}  // namespace base
