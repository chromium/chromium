// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/operations/test_util.h"

#include <utility>

#include "extensions/browser/event_router.h"

namespace ash::file_system_provider::operations::util {

LoggingDispatchEventImpl::LoggingDispatchEventImpl(bool dispatch_reply)
    : dispatch_reply_(dispatch_reply) {
}

LoggingDispatchEventImpl::~LoggingDispatchEventImpl() = default;

bool LoggingDispatchEventImpl::DispatchRequest(
    int request_id,
    std::optional<std::string> file_system_id,
    std::unique_ptr<extensions::Event> event) {
  events_.push_back(std::move(event));
  return dispatch_reply_;
}

void LoggingDispatchEventImpl::CancelRequest(
    int request_id,
    std::optional<std::string> file_system_id) {}

void LogStatusCallback(StatusCallbackLog* log, base::File::Error result) {
  log->push_back(result);
}

}  // namespace ash::file_system_provider::operations::util
