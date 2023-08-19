// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"

#include <inttypes.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/event_logger.h"

namespace extensions {

namespace {

constexpr base::TimeDelta kDefaultSlowOperationThreshold =
    base::Milliseconds(500);
constexpr base::TimeDelta kDefaultVerySlowOperationThreshold = base::Seconds(5);

}  // namespace

LoggedExtensionFunction::LoggedExtensionFunction()
    : log_on_completion_(false),
      slow_threshold_(kDefaultSlowOperationThreshold),
      very_slow_threshold_(kDefaultVerySlowOperationThreshold) {
  start_time_ = base::TimeTicks::Now();
}

LoggedExtensionFunction::~LoggedExtensionFunction() = default;

void LoggedExtensionFunction::OnResponded() {
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;

  drive::EventLogger* logger = file_manager::util::GetLogger(
      Profile::FromBrowserContext(browser_context()));
  std::string request_id_str = request_uuid().AsLowercaseString();
  if (logger && log_on_completion_) {
    DCHECK(response_type());
    bool success = *response_type() == SUCCEEDED;
    logger->Log(logging::LOGGING_INFO,
                "%s[%s] %s. (elapsed time: %" PRId64 "ms)", name(),
                request_id_str.c_str(), success ? "succeeded" : "failed",
                elapsed.InMilliseconds());
  }

  // Log performance issues separately from completion.
  if (elapsed >= very_slow_threshold_) {
    auto log_message = base::StringPrintf(
        "%s[%s] was VERY slow. (elapsed time: %" PRId64 "ms)", name(),
        request_id_str.c_str(), elapsed.InMilliseconds());
    LOG(WARNING) << log_message;
    if (logger) {
      logger->LogRawString(logging::LOGGING_ERROR,
                           "PERFORMANCE WARNING: " + log_message);
    }
  } else if (logger && elapsed >= slow_threshold_) {
    logger->Log(logging::LOGGING_WARNING,
                "PERFORMANCE WARNING: %s[%s] was slow. (elapsed time: %" PRId64
                "ms)",
                name(), request_id_str.c_str(), elapsed.InMilliseconds());
  }
  ExtensionFunction::OnResponded();
}

void LoggedExtensionFunction::SetWarningThresholds(
    base::TimeDelta slow_threshold,
    base::TimeDelta very_slow_threshold) {
  slow_threshold_ = slow_threshold;
  very_slow_threshold_ = very_slow_threshold;
}

}  // namespace extensions
