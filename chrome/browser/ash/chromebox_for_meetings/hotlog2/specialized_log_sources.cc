// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/specialized_log_sources.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"

namespace ash::cfm {

/*** audit.log ***/

constexpr LazyRE2 kAuditLogLineRegex = {
    // Note the blank () group for severity. Audit logs don't have a
    // severity, so match on a blank string and force to the default.
    "type=[A-Z0-9_]+ msg=audit\\(([0-9.]+):[0-9]+\\): ()((?s).*)"};

AuditLogSource::AuditLogSource(base::TimeDelta poll_rate, size_t batch_size)
    : LogSource(kCfmAuditLogFile, poll_rate, batch_size) {}

AuditLogSource::~AuditLogSource() = default;

RE2& AuditLogSource::GetLogLineRegex() {
  return *kAuditLogLineRegex;
}

uint64_t AuditLogSource::TimestampStringToUnixTime(
    const std::string& timestamp) {
  // Audit files have the timestamp directly in time-since-epoch format
  // (in seconds) so just convert to an int and return it. To convert to
  // microseconds, parse the TS as a double (to capture the decimal point),
  // then multiply by 1,000,000.
  double ts_double;
  if (!base::StringToDouble(timestamp, &ts_double)) {
    return 0;
  }
  return ts_double * 1000000;
}

/*** bios_info.txt ***/

BiosInfoLogSource::BiosInfoLogSource(base::TimeDelta poll_rate,
                                     size_t batch_size)
    : LogSource(kCfmBiosInfoLogFile, poll_rate, batch_size) {}

BiosInfoLogSource::~BiosInfoLogSource() = default;

bool BiosInfoLogSource::AreTimestampsExpected() const {
  return false;
}

}  // namespace ash::cfm
