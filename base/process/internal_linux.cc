// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/internal_linux.h"

#include <limits.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"

// Not defined on AIX by default.
#if BUILDFLAG(IS_AIX)
#define NAME_MAX 255
#endif

namespace base::internal {

namespace {

void TrimKeyValuePairs(StringPairs* pairs) {
  for (auto& pair : *pairs) {
    TrimWhitespaceASCII(pair.first, TRIM_ALL, &pair.first);
    TrimWhitespaceASCII(pair.second, TRIM_ALL, &pair.second);
  }
}

}  // namespace

const char kProcDir[] = "/proc";

const char kStatFile[] = "stat";

FilePath GetProcPidDir(pid_t pid) {
  return FilePath(kProcDir).Append(NumberToString(pid));
}

pid_t ProcDirSlotToPid(std::string_view d_name) {
  if (d_name.size() >= NAME_MAX ||
      !std::ranges::all_of(d_name, &IsAsciiDigit<char>)) {
    return 0;
  }

  // Read the process's command line.
  pid_t pid;
  std::string pid_string(d_name);
  if (!StringToInt(pid_string, &pid)) {
    NOTREACHED();
  }
  return pid;
}

bool ReadProcFile(const FilePath& file, std::string* buffer) {
  DCHECK(FilePath(kProcDir).IsParent(file));
  buffer->clear();
  // Synchronously reading files in /proc is safe.
  ScopedAllowBlocking scoped_allow_blocking;

  if (!ReadFileToString(file, buffer)) {
    return false;
  }
  return !buffer->empty();
}

bool ReadProcFileToTrimmedStringPairs(pid_t pid,
                                      std::string_view filename,
                                      StringPairs* key_value_pairs) {
  std::string status_data;
  FilePath status_file = GetProcPidDir(pid).Append(filename);
  if (!ReadProcFile(status_file, &status_data)) {
    return false;
  }
  SplitStringIntoKeyValuePairs(status_data, ':', '\n', key_value_pairs);
  TrimKeyValuePairs(key_value_pairs);
  return true;
}

size_t ReadProcStatusAndGetKbFieldAsSizeT(pid_t pid, std::string_view field) {
  StringPairs pairs;
  if (!ReadProcFileToTrimmedStringPairs(pid, "status", &pairs)) {
    return 0;
  }

  for (const auto& pair : pairs) {
    const std::string& key = pair.first;
    const std::string& value_str = pair.second;
    if (key != field) {
      continue;
    }

    std::vector<std::string_view> split_value_str =
        SplitStringPiece(value_str, " ", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
      NOTREACHED();
    }
    size_t value;
    if (!StringToSizeT(split_value_str[0], &value)) {
      NOTREACHED();
    }
    return value;
  }
  // This can be reached if the process dies when proc is read -- in that case,
  // the kernel can return missing fields.
  return 0;
}

bool ReadProcStatusAndGetFieldAsUint64(pid_t pid,
                                       std::string_view field,
                                       uint64_t* result) {
  StringPairs pairs;
  if (!ReadProcFileToTrimmedStringPairs(pid, "status", &pairs)) {
    return false;
  }

  for (const auto& pair : pairs) {
    const std::string& key = pair.first;
    const std::string& value_str = pair.second;
    if (key != field) {
      continue;
    }

    uint64_t value;
    if (!StringToUint64(value_str, &value)) {
      return false;
    }
    *result = value;
    return true;
  }
  return false;
}

bool ReadProcStats(pid_t pid, std::string* buffer) {
  FilePath stat_file = internal::GetProcPidDir(pid).Append(kStatFile);
  return ReadProcFile(stat_file, buffer);
}

bool ParseProcStats(const std::string& stats_data,
                    std::vector<std::string>* proc_stats) {
  // |stats_data| may be empty if the process disappeared somehow.
  // e.g. http://crbug.com/145811
  if (stats_data.empty())
    return false;

  // The stat file is formatted as:
  // pid (process name) data1 data2 .... dataN
  // Look for the closing paren by scanning backwards, to avoid being fooled by
  // processes with ')' in the name.
  size_t open_parens_idx = stats_data.find(" (");
  size_t close_parens_idx = stats_data.rfind(") ");
  if (open_parens_idx == std::string::npos ||
      close_parens_idx == std::string::npos ||
      open_parens_idx > close_parens_idx) {
    DLOG(WARNING) << "Failed to find matched parens in '" << stats_data << "'";
    NOTREACHED();
  }
  open_parens_idx++;

  proc_stats->clear();
  // PID.
  proc_stats->push_back(stats_data.substr(0, open_parens_idx));
  // Process name without parentheses.
  proc_stats->push_back(
      stats_data.substr(open_parens_idx + 1,
                        close_parens_idx - (open_parens_idx + 1)));

  // Split the rest.
  std::vector<std::string> other_stats = SplitString(
      stats_data.substr(close_parens_idx + 2), " ",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& i : other_stats)
    proc_stats->push_back(i);
  return true;
}

typedef std::map<std::string, std::string> ProcStatMap;
void ParseProcStat(const std::string& contents, ProcStatMap* output) {
  StringPairs key_value_pairs;
  SplitStringIntoKeyValuePairs(contents, ' ', '\n', &key_value_pairs);
  for (auto& i : key_value_pairs) {
    output->insert(std::move(i));
  }
}

int64_t GetProcStatsFieldAsInt64(const std::vector<std::string>& proc_stats,
                                 ProcStatsFields field_num) {
  DCHECK_GE(field_num, VM_PPID);
  return GetProcStatsFieldAsOptionalInt64(proc_stats, field_num).value_or(0);
}

std::optional<int64_t> GetProcStatsFieldAsOptionalInt64(
    base::span<const std::string> proc_stats,
    ProcStatsFields field_num) {
  int64_t value;
  if (StringToInt64(proc_stats[size_t{field_num}], &value)) {
    return value;
  }
  return std::nullopt;
}

size_t GetProcStatsFieldAsSizeT(const std::vector<std::string>& proc_stats,
                                ProcStatsFields field_num) {
  DCHECK_GE(field_num, VM_PPID);
  CHECK_LT(static_cast<size_t>(field_num), proc_stats.size());

  size_t value;
  return StringToSizeT(proc_stats[field_num], &value) ? value : 0;
}

int64_t ReadStatFileAndGetFieldAsInt64(const FilePath& stat_file,
                                       ProcStatsFields field_num) {
  std::string stats_data;
  if (!ReadProcFile(stat_file, &stats_data))
    return 0;
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(stats_data, &proc_stats))
    return 0;
  return GetProcStatsFieldAsInt64(proc_stats, field_num);
}

int64_t ReadProcStatsAndGetFieldAsInt64(pid_t pid, ProcStatsFields field_num) {
  FilePath stat_file = internal::GetProcPidDir(pid).Append(kStatFile);
  return ReadStatFileAndGetFieldAsInt64(stat_file, field_num);
}

int64_t ReadProcSelfStatsAndGetFieldAsInt64(ProcStatsFields field_num) {
  FilePath stat_file = FilePath(kProcDir).Append("self").Append(kStatFile);
  return ReadStatFileAndGetFieldAsInt64(stat_file, field_num);
}

size_t ReadProcStatsAndGetFieldAsSizeT(pid_t pid, ProcStatsFields field_num) {
  std::string stats_data;
  if (!ReadProcStats(pid, &stats_data))
    return 0;
  std::vector<std::string> proc_stats;
  if (!ParseProcStats(stats_data, &proc_stats))
    return 0;
  return GetProcStatsFieldAsSizeT(proc_stats, field_num);
}

Time GetBootTime() {
  FilePath path("/proc/stat");
  std::string contents;
  if (!ReadProcFile(path, &contents))
    return Time();
  ProcStatMap proc_stat;
  ParseProcStat(contents, &proc_stat);
  ProcStatMap::const_iterator btime_it = proc_stat.find("btime");
  if (btime_it == proc_stat.end())
    return Time();
  int btime;
  if (!StringToInt(btime_it->second, &btime))
    return Time();
  return Time::FromTimeT(btime);
}

TimeDelta GetUserCpuTimeSinceBoot() {
  FilePath path("/proc/stat");
  std::string contents;
  if (!ReadProcFile(path, &contents))
    return TimeDelta();

  ProcStatMap proc_stat;
  ParseProcStat(contents, &proc_stat);
  ProcStatMap::const_iterator cpu_it = proc_stat.find("cpu");
  if (cpu_it == proc_stat.end())
    return TimeDelta();

  std::vector<std::string> cpu = SplitString(
      cpu_it->second, kWhitespaceASCII, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);

  if (cpu.size() < 2 || cpu[0] != "cpu")
    return TimeDelta();

  uint64_t user;
  uint64_t nice;
  if (!StringToUint64(cpu[0], &user) || !StringToUint64(cpu[1], &nice))
    return TimeDelta();

  return ClockTicksToTimeDelta(checked_cast<int64_t>(user + nice));
}

TimeDelta ClockTicksToTimeDelta(int64_t clock_ticks) {
  // This queries the /proc-specific scaling factor which is
  // conceptually the system hertz.  To dump this value on another
  // system, try
  //   od -t dL /proc/self/auxv
  // and look for the number after 17 in the output; mine is
  //   0000040          17         100           3   134512692
  // which means the answer is 100.
  // It may be the case that this value is always 100.
  static const long kHertz = sysconf(_SC_CLK_TCK);

  return Microseconds(Time::kMicrosecondsPerSecond * clock_ticks / kHertz);
}

}  // namespace base::internal
