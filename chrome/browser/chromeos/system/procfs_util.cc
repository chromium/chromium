// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/procfs_util.h"

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace chromeos {
namespace system {

base::Optional<SingleProcStat> GetSingleProcStat(
    const base::FilePath& stat_file) {
  SingleProcStat stat;
  std::string stat_contents;
  if (!base::ReadFileToString(stat_file, &stat_contents))
    return base::nullopt;

  // This file looks like:
  // <num1> (<str>) <char> <num2> <num3> ...
  // The entries at 0-based index 0 is the PID.
  // The entry at index 1 represents a filename, which can have an arbitrary
  // number of spaces, so skip it by finding the last parenthesis.
  // The entry at index 3, represents the PPID of the process.
  // The entries at indices 13 and 14 represent the amount of time the
  // process was in user mode and kernel mode in jiffies.
  // The entry at index 23 represents process resident memory in pages.
  const auto first_space = stat_contents.find(' ');
  if (first_space == std::string::npos)
    return base::nullopt;
  if (!base::StringToInt(stat_contents.substr(0, first_space), &stat.pid))
    return base::nullopt;

  const auto left_parenthesis = stat_contents.find('(');
  if (left_parenthesis == std::string::npos)
    return base::nullopt;
  const auto right_parenthesis = stat_contents.find(')');
  if (right_parenthesis == std::string::npos)
    return base::nullopt;
  if ((right_parenthesis - left_parenthesis - 1) <= 0)
    return base::nullopt;
  stat.name = stat_contents.substr(left_parenthesis + 1,
                                   right_parenthesis - left_parenthesis - 1);

  // Skip the comm field.
  const auto last_parenthesis = stat_contents.find_last_of(')');
  if (last_parenthesis == std::string::npos ||
      last_parenthesis + 1 > stat_contents.length())
    return base::nullopt;

  // Skip the parenthesis itself.
  const std::string truncated_proc_stat_contents =
      stat_contents.substr(last_parenthesis + 1);

  std::vector<base::StringPiece> proc_stat_split =
      base::SplitStringPiece(truncated_proc_stat_contents, " \t\n",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // The first 2 entries of the file were removed earlier, so all the indices
  // for the entries will be shifted by 2.
  if (proc_stat_split.size() < 21)
    return base::nullopt;
  if (!base::StringToInt(proc_stat_split[1], &stat.ppid))
    return base::nullopt;

  // These two entries contain the total time this process spent in user mode
  // and kernel mode. This is roughly the total CPU time that the process has
  // used.
  if (!base::StringToInt64(proc_stat_split[11], &stat.utime))
    return base::nullopt;

  if (!base::StringToInt64(proc_stat_split[12], &stat.stime))
    return base::nullopt;

  if (!base::StringToInt64(proc_stat_split[21], &stat.rss))
    return base::nullopt;
  return stat;
}

base::Optional<int64_t> GetCpuTimeJiffies(const base::FilePath& stat_file) {
  std::string stat_contents;
  if (!base::ReadFileToString(stat_file, &stat_contents))
    return base::nullopt;

  // This file looks like:
  // cpu <num1> <num2> ...
  // cpu0 <num1> <num2> ...
  // cpu1 <num1> <num2> ...
  // ...
  // Where each number represents the amount of time in jiffies a certain CPU is
  // in some state. The first line presents the total amount of time in jiffies
  // the system is in some state across all CPUs. The first line beginning with
  // "cpu " needs to be singled out. The first 8 of the 10 numbers on that line
  // need to be summed to obtain the total amount of time in jiffies the system
  // has been running across all states. The last 2 numbers are guest and
  // guest_nice, which are already accounted for in the first 2 numbers of user
  // and nice respectively.
  std::vector<base::StringPiece> stat_lines = base::SplitStringPiece(
      stat_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : stat_lines) {
    // Find the line that starts with "cpu " and sum the first 8 numbers to
    // get the total amount of jiffies used.
    if (base::StartsWith(line, "cpu ", base::CompareCase::SENSITIVE)) {
      std::vector<base::StringPiece> cpu_info_parts = base::SplitStringPiece(
          line, " \t", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (cpu_info_parts.size() != 11)
        return base::nullopt;

      int64_t total_time = 0;
      // Sum the first 8 numbers. Element 0 is "cpu".
      for (int i = 1; i <= 8; i++) {
        int64_t curr;
        if (!base::StringToInt64(cpu_info_parts.at(i), &curr))
          return base::nullopt;
        total_time += curr;
      }
      return total_time;
    }
  }
  return base::nullopt;
}

base::Optional<int64_t> GetUsedMemTotalKB(const base::FilePath& meminfo_file) {
  int64_t mem_total = 0;
  int64_t mem_free = 0;
  std::string meminfo_contents;
  if (!base::ReadFileToString(meminfo_file, &meminfo_contents))
    return base::nullopt;

  std::vector<base::StringPiece> meminfo_lines = base::SplitStringPiece(
      meminfo_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (auto& line : meminfo_lines) {
    if (base::StartsWith(line, "MemTotal:", base::CompareCase::SENSITIVE)) {
      std::vector<base::StringPiece> line_items = base::SplitStringPiece(
          line, " \t", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (line_items.size() != 3)
        return base::nullopt;
      if (!base::StringToInt64(line_items.at(1), &mem_total))
        return base::nullopt;
    }
    if (base::StartsWith(line, "MemFree:", base::CompareCase::SENSITIVE)) {
      std::vector<base::StringPiece> line_items = base::SplitStringPiece(
          line, " \t", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (line_items.size() != 3)
        return base::nullopt;
      if (!base::StringToInt64(line_items.at(1), &mem_free))
        return base::nullopt;
      break;
    }
  }
  return mem_total - mem_free;
}

}  // namespace system
}  // namespace chromeos
