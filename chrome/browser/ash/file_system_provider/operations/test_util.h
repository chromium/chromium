// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/files/file.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher.h"

namespace extensions {
struct Event;
}  // namespace extensions

namespace ash::file_system_provider::operations::util {

// Fake event dispatcher implementation with extra logging capability. Acts as
// a providing extension end-point.
class LoggingDispatchEventImpl : public RequestDispatcher {
 public:
  explicit LoggingDispatchEventImpl(bool dispatch_reply);

  LoggingDispatchEventImpl(const LoggingDispatchEventImpl&) = delete;
  LoggingDispatchEventImpl& operator=(const LoggingDispatchEventImpl&) = delete;

  ~LoggingDispatchEventImpl() override;

  // Handles sending a request event to a providing extension.
  bool DispatchRequest(int request_id,
                       std::optional<std::string> file_system_id,
                       std::unique_ptr<extensions::Event> event) override;
  void CancelRequest(int request_id,
                     std::optional<std::string> file_system_id) override;

  // Returns events sent to providing extensions.
  std::vector<std::unique_ptr<extensions::Event>>& events() { return events_; }

 private:
  std::vector<std::unique_ptr<extensions::Event>> events_;
  bool dispatch_reply_;
};

// Container for remembering operations' callback invocations.
typedef std::vector<base::File::Error> StatusCallbackLog;

// Pushes a result of the StatusCallback invocation to a log vector.
void LogStatusCallback(StatusCallbackLog* log, base::File::Error result);

}  // namespace ash::file_system_provider::operations::util

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPERATIONS_TEST_UTIL_H_
