// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define _GNU_SOURCE to ensure that <errno.h> defines
// program_invocation_short_name which is used in GetProgramName(). Keep this at
// the top of the file since some system headers might include <errno.h> and the
// header could be skipped on subsequent includes.
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "base/logging.h"

#include <errno.h>

#include <iomanip>
#include <string_view>

#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"

namespace logging {

namespace {
const char* GetProgramName() {
  return program_invocation_short_name ? program_invocation_short_name : "";
}
}  // namespace

// InitWithSyslogPrefix generates log header for Chrome OS in syslog compatible
// format. The timezone will always be UTC regardless of devices' timezone.
// `<rfc3339_timestamp> <tickcount> <log_level>`
// `<program_name>[<pid>:<thread_id>]: `
// `[<filename>(<line_number>)] <message>`
//
// e.g.
// 2020-06-27T23:55:25.094701Z 1234 VERBOSE1 chrome[3816:3877]:
// [drm_device_handle.cc(90)] Succeeded authenticating /dev/dri/card0 in 0 ms
// with 1 attempt(s)
void LogMessage::InitWithSyslogPrefix(std::string_view filename,
                                      int line,
                                      uint64_t tick_count,
                                      const char* log_severity_name_c_str,
                                      const char* log_prefix,
                                      bool enable_process_id,
                                      bool enable_thread_id,
                                      bool enable_timestamp,
                                      bool enable_tickcount) {
  if (log_prefix)
    stream_ << log_prefix << ':';
  if (enable_timestamp) {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm utc_time {};
    gmtime_r(&t, &utc_time);
    stream_ << std::setfill('0')                               // Set fill to 0
            << std::setw(4) << 1900 + utc_time.tm_year << "-"  // year
            << std::setw(2) << 1 + utc_time.tm_mon << "-"      // month
            << std::setw(2) << utc_time.tm_mday                // date
            << 'T' << std::setw(2) << utc_time.tm_hour << ":"  // hour
            << std::setw(2) << utc_time.tm_min << ":"          // minute
            << std::setw(2) << utc_time.tm_sec << "."          // second
            << std::setw(6) << tv.tv_usec                      // millisecond
            << "Z ";                                           // timezone UTC
  }
  if (enable_tickcount)
    stream_ << tick_count << ' ';
  if (severity_ >= 0) {
    stream_ << log_severity_name_c_str;
  } else {
    stream_ << "VERBOSE" << -severity_;
  }
  stream_ << ' ' << GetProgramName();
  if (enable_process_id || enable_thread_id) {
    stream_ << "[";
    if (enable_process_id) {
      stream_ << base::GetUniqueIdForProcess();
    }
    if (enable_thread_id) {
      stream_ << ':' << base::PlatformThread::CurrentId();
    }
    stream_ << "]";
  }
  stream_ << ": ";
  stream_ << "[" << filename << "(" << line << ")] ";
}

}  // namespace logging
