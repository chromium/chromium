// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_
#define BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_

#include <fuchsia/logger/cpp/fidl_test_base.h>

#include "base/callback.h"
#include "base/fuchsia/process_context.h"
#include "base/strings/string_piece.h"

namespace base {

// A LogListenerSafe implementation for use in Fuchsia logging tests.
// For use with fuchsia.logger.Log.DumpLogsSafe().
// Stores messages received via LogMany() for inspection by tests.
class TestLogListenerSafe
    : public ::fuchsia::logger::testing::LogListenerSafe_TestBase {
 public:
  TestLogListenerSafe();
  TestLogListenerSafe(const TestLogListenerSafe&) = delete;
  TestLogListenerSafe& operator=(const TestLogListenerSafe&) = delete;
  ~TestLogListenerSafe() override;

  void set_on_dump_logs_done(base::OnceClosure on_dump_logs_done);

  bool DidReceiveString(base::StringPiece message,
                        ::fuchsia::logger::LogMessage* logged_message);

  // LogListenerSafe implementation.
  void LogMany(std::vector<::fuchsia::logger::LogMessage> messages,
               LogManyCallback callback) override;
  void Done() override;
  void NotImplemented_(const std::string& name) override;

 private:
  std::vector<::fuchsia::logger::LogMessage> log_messages_;
  base::OnceClosure on_dump_logs_done_;
};

}  // namespace base

#endif  // BASE_FUCHSIA_TEST_LOG_LISTENER_SAFE_H_
