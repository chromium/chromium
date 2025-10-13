// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/specialized_log_sources.h"

#include "base/files/file_enumerator.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_source.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::cfm {

/*** audit.log ***/

constexpr LazyRE2 kAuditLogLineRegex = {
    // Note the blank () group for severity. Audit logs don't have a
    // severity, so match on a blank string and force to the default.
    "type=[A-Z0-9_]+ msg=audit\\(([0-9.]+):[0-9]+\\): ()((?s).*)"};

AuditLogSource::AuditLogSource(size_t data_buffer_size_limit,
                               base::TimeDelta poll_rate,
                               size_t num_lines_per_batch)
    : LogSource(kCfmAuditLogFile,
                data_buffer_size_limit,
                poll_rate,
                num_lines_per_batch) {}

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

BiosInfoLogSource::BiosInfoLogSource(size_t data_buffer_size_limit,
                                     base::TimeDelta poll_rate,
                                     size_t num_lines_per_batch)
    : LogSource(kCfmBiosInfoLogFile,
                data_buffer_size_limit,
                poll_rate,
                num_lines_per_batch) {}

BiosInfoLogSource::~BiosInfoLogSource() = default;

bool BiosInfoLogSource::AreTimestampsExpected() const {
  return false;
}

/*** eventlog.txt ***/

constexpr LazyRE2 kEventlogLogLineRegex = {
    // Note the blank () group for severity. The eventlog doesn't have a
    // severity, so match on a blank string and force to the default.
    "[0-9]+ \\| ((?s).+?) \\| ()((?s).*)"};

EventlogLogSource::EventlogLogSource(size_t data_buffer_size_limit,
                                     base::TimeDelta poll_rate,
                                     size_t num_lines_per_batch)
    : LogSource(kCfmEventlogLogFile,
                data_buffer_size_limit,
                poll_rate,
                num_lines_per_batch) {}

EventlogLogSource::~EventlogLogSource() = default;

RE2& EventlogLogSource::GetLogLineRegex() {
  return *kEventlogLogLineRegex;
}

/*** .varations-list.txt ***/

VariationsListLogSource::VariationsListLogSource(size_t data_buffer_size_limit,
                                                 base::TimeDelta poll_rate,
                                                 size_t num_lines_per_batch)
    : LogSource(GetVariationsLogPath(kCfmVariationsListLogFile),
                data_buffer_size_limit,
                poll_rate,
                num_lines_per_batch) {}

VariationsListLogSource::~VariationsListLogSource() = default;

bool VariationsListLogSource::AreTimestampsExpected() const {
  return false;
}

std::string VariationsListLogSource::GetVariationsLogPath(
    const std::string& basename) {
  // CfM devices will only contain a single robot user, so use
  // a regex to find the only matching home directory.
  std::string regex = "/home/chronos/u-[0-9a-f]+";
  auto basepath = base::FilePath("/home/chronos");
  base::FileEnumerator file_iter(basepath,
                                 /*recursive=*/false,
                                 base::FileEnumerator::DIRECTORIES);

  for (base::FilePath file = file_iter.Next(); !file.empty();
       file = file_iter.Next()) {
    if (RE2::FullMatch(file.value(), regex)) {
      return file.Append(basename).value();
    }
  }

  // If the robot dir doesn't exist, fall back to variations list on rootfs.
  return "/home/chronos/" + basename;
}

}  // namespace ash::cfm
