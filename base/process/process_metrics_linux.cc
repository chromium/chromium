// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <utility>

#include "base/cpu.h"
#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/internal_linux.h"
#include "base/process/process_metrics_iocounters.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

class ScopedAllowBlockingForProcessMetrics : public ScopedAllowBlocking {};

namespace {

void TrimKeyValuePairs(StringPairs* pairs) {
  for (auto& pair : *pairs) {
    TrimWhitespaceASCII(pair.first, TRIM_ALL, &pair.first);
    TrimWhitespaceASCII(pair.second, TRIM_ALL, &pair.second);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// Read a file with a single number string and return the number as a uint64_t.
uint64_t ReadFileToUint64(const FilePath& file) {
  std::string file_contents;
  if (!ReadFileToString(file, &file_contents))
    return 0;
  TrimWhitespaceASCII(file_contents, TRIM_ALL, &file_contents);
  uint64_t file_contents_uint64 = 0;
  if (!StringToUint64(file_contents, &file_contents_uint64))
    return 0;
  return file_contents_uint64;
}
#endif

// Read |filename| in /proc/<pid>/, split the entries into key/value pairs, and
// trim the key and value. On success, return true and write the trimmed
// key/value pairs into |key_value_pairs|.
bool ReadProcFileToTrimmedStringPairs(pid_t pid,
                                      StringPiece filename,
                                      StringPairs* key_value_pairs) {
  std::string status_data;
  FilePath status_file = internal::GetProcPidDir(pid).Append(filename);
  if (!internal::ReadProcFile(status_file, &status_data))
    return false;
  SplitStringIntoKeyValuePairs(status_data, ':', '\n', key_value_pairs);
  TrimKeyValuePairs(key_value_pairs);
  return true;
}

// Read /proc/<pid>/status and return the value for |field|, or 0 on failure.
// Only works for fields in the form of "Field: value kB".
size_t ReadProcStatusAndGetFieldAsSizeT(pid_t pid, StringPiece field) {
  StringPairs pairs;
  if (!ReadProcFileToTrimmedStringPairs(pid, "status", &pairs))
    return 0;

  for (const auto& pair : pairs) {
    const std::string& key = pair.first;
    const std::string& value_str = pair.second;
    if (key != field)
      continue;

    std::vector<StringPiece> split_value_str =
        SplitStringPiece(value_str, " ", TRIM_WHITESPACE, SPLIT_WANT_ALL);
    if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
      NOTREACHED();
      return 0;
    }
    size_t value;
    if (!StringToSizeT(split_value_str[0], &value)) {
      NOTREACHED();
      return 0;
    }
    return value;
  }
  // This can be reached if the process dies when proc is read -- in that case,
  // the kernel can return missing fields.
  return 0;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)
// Read /proc/<pid>/status and look for |field|. On success, return true and
// write the value for |field| into |result|.
// Only works for fields in the form of "field    :     uint_value"
bool ReadProcStatusAndGetFieldAsUint64(pid_t pid,
                                       StringPiece field,
                                       uint64_t* result) {
  StringPairs pairs;
  if (!ReadProcFileToTrimmedStringPairs(pid, "status", &pairs))
    return false;

  for (const auto& pair : pairs) {
    const std::string& key = pair.first;
    const std::string& value_str = pair.second;
    if (key != field)
      continue;

    uint64_t value;
    if (!StringToUint64(value_str, &value))
      return false;
    *result = value;
    return true;
  }
  return false;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)

// Get the total CPU from a proc stat buffer.  Return value is number of jiffies
// on success or 0 if parsing failed.
int64_t ParseTotalCPUTimeFromStats(const std::vector<std::string>& proc_stats) {
  return internal::GetProcStatsFieldAsInt64(proc_stats, internal::VM_UTIME) +
         internal::GetProcStatsFieldAsInt64(proc_stats, internal::VM_STIME);
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
int64_t GetProcessCPU(pid_t pid) {
  std::string buffer;
  std::vector<std::string> proc_stats;
  if (!internal::ReadProcStats(pid, &buffer) ||
      !internal::ParseProcStats(buffer, &proc_stats)) {
    return -1;
  }

  return ParseTotalCPUTimeFromStats(proc_stats);
}

}  // namespace

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

size_t ProcessMetrics::GetResidentSetSize() const {
  return internal::ReadProcStatsAndGetFieldAsSizeT(process_, internal::VM_RSS) *
         checked_cast<size_t>(getpagesize());
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  return internal::ClockTicksToTimeDelta(GetProcessCPU(process_));
}

bool ProcessMetrics::GetCumulativeCPUUsagePerThread(
    CPUUsagePerThread& cpu_per_thread) {
  cpu_per_thread.clear();

  internal::ForEachProcessTask(
      process_,
      [&cpu_per_thread](PlatformThreadId tid, const FilePath& task_path) {
        FilePath thread_stat_path = task_path.Append("stat");

        std::string buffer;
        std::vector<std::string> proc_stats;
        if (!internal::ReadProcFile(thread_stat_path, &buffer) ||
            !internal::ParseProcStats(buffer, &proc_stats)) {
          return;
        }

        TimeDelta thread_time = internal::ClockTicksToTimeDelta(
            ParseTotalCPUTimeFromStats(proc_stats));
        cpu_per_thread.emplace_back(tid, thread_time);
      });

  return !cpu_per_thread.empty();
}

// For the /proc/self/io file to exist, the Linux kernel must have
// CONFIG_TASK_IO_ACCOUNTING enabled.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  StringPairs pairs;
  if (!ReadProcFileToTrimmedStringPairs(process_, "io", &pairs))
    return false;

  io_counters->OtherOperationCount = 0;
  io_counters->OtherTransferCount = 0;

  for (const auto& pair : pairs) {
    const std::string& key = pair.first;
    const std::string& value_str = pair.second;
    uint64_t* target_counter = nullptr;
    if (key == "syscr")
      target_counter = &io_counters->ReadOperationCount;
    else if (key == "syscw")
      target_counter = &io_counters->WriteOperationCount;
    else if (key == "rchar")
      target_counter = &io_counters->ReadTransferCount;
    else if (key == "wchar")
      target_counter = &io_counters->WriteTransferCount;
    if (!target_counter)
      continue;
    bool converted = StringToUint64(value_str, target_counter);
    DCHECK(converted);
  }
  return true;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
uint64_t ProcessMetrics::GetVmSwapBytes() const {
  return ReadProcStatusAndGetFieldAsSizeT(process_, "VmSwap") * 1024;
}

bool ProcessMetrics::GetPageFaultCounts(PageFaultCounts* counts) const {
  // We are not using internal::ReadStatsFileAndGetFieldAsInt64(), since it
  // would read the file twice, and return inconsistent numbers.
  std::string stats_data;
  if (!internal::ReadProcStats(process_, &stats_data))
    return false;
  std::vector<std::string> proc_stats;
  if (!internal::ParseProcStats(stats_data, &proc_stats))
    return false;

  counts->minor =
      internal::GetProcStatsFieldAsInt64(proc_stats, internal::VM_MINFLT);
  counts->major =
      internal::GetProcStatsFieldAsInt64(proc_stats, internal::VM_MAJFLT);
  return true;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

int ProcessMetrics::GetOpenFdCount() const {
  // Use /proc/<pid>/fd to count the number of entries there.
  FilePath fd_path = internal::GetProcPidDir(process_).Append("fd");

  DirReaderPosix dir_reader(fd_path.value().c_str());
  if (!dir_reader.IsValid())
    return -1;

  int total_count = 0;
  for (; dir_reader.Next(); ) {
    const char* name = dir_reader.name();
    if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
      ++total_count;
  }

  return total_count;
}

int ProcessMetrics::GetOpenFdSoftLimit() const {
  // Use /proc/<pid>/limits to read the open fd limit.
  FilePath fd_path = internal::GetProcPidDir(process_).Append("limits");

  std::string limits_contents;
  if (!ReadFileToStringNonBlocking(fd_path, &limits_contents))
    return -1;

  for (const auto& line : SplitStringPiece(
           limits_contents, "\n", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    if (!StartsWith(line, "Max open files"))
      continue;

    auto tokens =
        SplitStringPiece(line, " ", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    if (tokens.size() > 3) {
      int limit = -1;
      if (!StringToInt(tokens[3], &limit))
        return -1;
      return limit;
    }
  }
  return -1;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)
ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process), last_absolute_idle_wakeups_(0) {}
#else
ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process) {}
#endif

size_t GetSystemCommitCharge() {
  SystemMemoryInfoKB meminfo;
  if (!GetSystemMemoryInfo(&meminfo))
    return 0;
  return checked_cast<size_t>(meminfo.total - meminfo.free - meminfo.buffers -
                              meminfo.cached);
}

int ParseProcStatCPU(StringPiece input) {
  // |input| may be empty if the process disappeared somehow.
  // e.g. http://crbug.com/145811.
  if (input.empty())
    return -1;

  size_t start = input.find_last_of(')');
  if (start == input.npos)
    return -1;

  // Number of spaces remaining until reaching utime's index starting after the
  // last ')'.
  int num_spaces_remaining = internal::VM_UTIME - 1;

  size_t i = start;
  while ((i = input.find(' ', i + 1)) != input.npos) {
    // Validate the assumption that there aren't any contiguous spaces
    // in |input| before utime.
    DCHECK_NE(input[i - 1], ' ');
    if (--num_spaces_remaining == 0) {
      int utime = 0;
      int stime = 0;
      if (sscanf(&input.data()[i], "%d %d", &utime, &stime) != 2)
        return -1;

      return utime + stime;
    }
  }

  return -1;
}

int64_t GetNumberOfThreads(ProcessHandle process) {
  return internal::ReadProcStatsAndGetFieldAsInt64(process,
                                                   internal::VM_NUMTHREADS);
}

const char kProcSelfExe[] = "/proc/self/exe";

namespace {

// The format of /proc/diskstats is:
//  Device major number
//  Device minor number
//  Device name
//  Field  1 -- # of reads completed
//      This is the total number of reads completed successfully.
//  Field  2 -- # of reads merged, field 6 -- # of writes merged
//      Reads and writes which are adjacent to each other may be merged for
//      efficiency.  Thus two 4K reads may become one 8K read before it is
//      ultimately handed to the disk, and so it will be counted (and queued)
//      as only one I/O.  This field lets you know how often this was done.
//  Field  3 -- # of sectors read
//      This is the total number of sectors read successfully.
//  Field  4 -- # of milliseconds spent reading
//      This is the total number of milliseconds spent by all reads (as
//      measured from __make_request() to end_that_request_last()).
//  Field  5 -- # of writes completed
//      This is the total number of writes completed successfully.
//  Field  6 -- # of writes merged
//      See the description of field 2.
//  Field  7 -- # of sectors written
//      This is the total number of sectors written successfully.
//  Field  8 -- # of milliseconds spent writing
//      This is the total number of milliseconds spent by all writes (as
//      measured from __make_request() to end_that_request_last()).
//  Field  9 -- # of I/Os currently in progress
//      The only field that should go to zero. Incremented as requests are
//      given to appropriate struct request_queue and decremented as they
//      finish.
//  Field 10 -- # of milliseconds spent doing I/Os
//      This field increases so long as field 9 is nonzero.
//  Field 11 -- weighted # of milliseconds spent doing I/Os
//      This field is incremented at each I/O start, I/O completion, I/O
//      merge, or read of these stats by the number of I/Os in progress
//      (field 9) times the number of milliseconds spent doing I/O since the
//      last update of this field.  This can provide an easy measure of both
//      I/O completion time and the backlog that may be accumulating.

const size_t kDiskDriveName = 2;
const size_t kDiskReads = 3;
const size_t kDiskReadsMerged = 4;
const size_t kDiskSectorsRead = 5;
const size_t kDiskReadTime = 6;
const size_t kDiskWrites = 7;
const size_t kDiskWritesMerged = 8;
const size_t kDiskSectorsWritten = 9;
const size_t kDiskWriteTime = 10;
const size_t kDiskIO = 11;
const size_t kDiskIOTime = 12;
const size_t kDiskWeightedIOTime = 13;

}  // namespace

Value::Dict SystemMemoryInfoKB::ToDict() const {
  Value::Dict res;
  res.Set("total", total);
  res.Set("free", free);
  res.Set("available", available);
  res.Set("buffers", buffers);
  res.Set("cached", cached);
  res.Set("active_anon", active_anon);
  res.Set("inactive_anon", inactive_anon);
  res.Set("active_file", active_file);
  res.Set("inactive_file", inactive_file);
  res.Set("swap_total", swap_total);
  res.Set("swap_free", swap_free);
  res.Set("swap_used", swap_total - swap_free);
  res.Set("dirty", dirty);
  res.Set("reclaimable", reclaimable);
#if BUILDFLAG(IS_CHROMEOS)
  res.Set("shmem", shmem);
  res.Set("slab", slab);
#endif

  return res;
}

bool ParseProcMeminfo(StringPiece meminfo_data, SystemMemoryInfoKB* meminfo) {
  // The format of /proc/meminfo is:
  //
  // MemTotal:      8235324 kB
  // MemFree:       1628304 kB
  // Buffers:        429596 kB
  // Cached:        4728232 kB
  // ...
  // There is no guarantee on the ordering or position
  // though it doesn't appear to change very often

  // As a basic sanity check at the end, make sure the MemTotal value will be at
  // least non-zero. So start off with a zero total.
  meminfo->total = 0;

  for (const StringPiece& line : SplitStringPiece(
           meminfo_data, "\n", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    std::vector<StringPiece> tokens = SplitStringPiece(
        line, kWhitespaceASCII, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
    // HugePages_* only has a number and no suffix so there may not be exactly 3
    // tokens.
    if (tokens.size() <= 1) {
      DLOG(WARNING) << "meminfo: tokens: " << tokens.size()
                    << " malformed line: " << line;
      continue;
    }

    int* target = nullptr;
    if (tokens[0] == "MemTotal:")
      target = &meminfo->total;
    else if (tokens[0] == "MemFree:")
      target = &meminfo->free;
    else if (tokens[0] == "MemAvailable:")
      target = &meminfo->available;
    else if (tokens[0] == "Buffers:")
      target = &meminfo->buffers;
    else if (tokens[0] == "Cached:")
      target = &meminfo->cached;
    else if (tokens[0] == "Active(anon):")
      target = &meminfo->active_anon;
    else if (tokens[0] == "Inactive(anon):")
      target = &meminfo->inactive_anon;
    else if (tokens[0] == "Active(file):")
      target = &meminfo->active_file;
    else if (tokens[0] == "Inactive(file):")
      target = &meminfo->inactive_file;
    else if (tokens[0] == "SwapTotal:")
      target = &meminfo->swap_total;
    else if (tokens[0] == "SwapFree:")
      target = &meminfo->swap_free;
    else if (tokens[0] == "Dirty:")
      target = &meminfo->dirty;
    else if (tokens[0] == "SReclaimable:")
      target = &meminfo->reclaimable;
#if BUILDFLAG(IS_CHROMEOS)
    // Chrome OS has a tweaked kernel that allows querying Shmem, which is
    // usually video memory otherwise invisible to the OS.
    else if (tokens[0] == "Shmem:")
      target = &meminfo->shmem;
    else if (tokens[0] == "Slab:")
      target = &meminfo->slab;
#endif
    if (target)
      StringToInt(tokens[1], target);
  }

  // Make sure the MemTotal is valid.
  return meminfo->total > 0;
}

bool ParseProcVmstat(StringPiece vmstat_data, VmStatInfo* vmstat) {
  // The format of /proc/vmstat is:
  //
  // nr_free_pages 299878
  // nr_inactive_anon 239863
  // nr_active_anon 1318966
  // nr_inactive_file 2015629
  // ...
  //
  // Iterate through the whole file because the position of the
  // fields are dependent on the kernel version and configuration.

  // Returns true if all of these 3 fields are present.
  bool has_pswpin = false;
  bool has_pswpout = false;
  bool has_pgmajfault = false;

  // The oom_kill field is optional. The vmstat oom_kill field is available on
  // upstream kernel 4.13. It's backported to Chrome OS kernel 3.10.
  bool has_oom_kill = false;
  vmstat->oom_kill = 0;

  for (const StringPiece& line : SplitStringPiece(
           vmstat_data, "\n", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
    std::vector<StringPiece> tokens = SplitStringPiece(
        line, " ", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
    if (tokens.size() != 2)
      continue;

    uint64_t val;
    if (!StringToUint64(tokens[1], &val))
      continue;

    if (tokens[0] == "pswpin") {
      vmstat->pswpin = val;
      DCHECK(!has_pswpin);
      has_pswpin = true;
    } else if (tokens[0] == "pswpout") {
      vmstat->pswpout = val;
      DCHECK(!has_pswpout);
      has_pswpout = true;
    } else if (tokens[0] == "pgmajfault") {
      vmstat->pgmajfault = val;
      DCHECK(!has_pgmajfault);
      has_pgmajfault = true;
    } else if (tokens[0] == "oom_kill") {
      vmstat->oom_kill = val;
      DCHECK(!has_oom_kill);
      has_oom_kill = true;
    }
  }

  return has_pswpin && has_pswpout && has_pgmajfault;
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  // Used memory is: total - free - buffers - caches
  // ReadFileToStringNonBlocking doesn't require ScopedAllowIO, and reading
  // /proc/meminfo is fast. See crbug.com/1160988 for details.
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!ReadFileToStringNonBlocking(meminfo_file, &meminfo_data)) {
    DLOG(WARNING) << "Failed to open " << meminfo_file.value();
    return false;
  }

  if (!ParseProcMeminfo(meminfo_data, meminfo)) {
    DLOG(WARNING) << "Failed to parse " << meminfo_file.value();
    return false;
  }

  return true;
}

Value::Dict VmStatInfo::ToDict() const {
  Value::Dict res;
  // TODO(crbug.com/1334256): Make base::Value able to hold uint64_t and remove
  // casts below.
  res.Set("pswpin", static_cast<int>(pswpin));
  res.Set("pswpout", static_cast<int>(pswpout));
  res.Set("pgmajfault", static_cast<int>(pgmajfault));
  return res;
}

bool GetVmStatInfo(VmStatInfo* vmstat) {
  // Synchronously reading files in /proc is safe.
  ScopedAllowBlockingForProcessMetrics allow_blocking;

  FilePath vmstat_file("/proc/vmstat");
  std::string vmstat_data;
  if (!ReadFileToStringNonBlocking(vmstat_file, &vmstat_data)) {
    DLOG(WARNING) << "Failed to open " << vmstat_file.value();
    return false;
  }
  if (!ParseProcVmstat(vmstat_data, vmstat)) {
    DLOG(WARNING) << "Failed to parse " << vmstat_file.value();
    return false;
  }
  return true;
}

SystemDiskInfo::SystemDiskInfo() {
  reads = 0;
  reads_merged = 0;
  sectors_read = 0;
  read_time = 0;
  writes = 0;
  writes_merged = 0;
  sectors_written = 0;
  write_time = 0;
  io = 0;
  io_time = 0;
  weighted_io_time = 0;
}

SystemDiskInfo::SystemDiskInfo(const SystemDiskInfo&) = default;

SystemDiskInfo& SystemDiskInfo::operator=(const SystemDiskInfo&) = default;

Value::Dict SystemDiskInfo::ToDict() const {
  Value::Dict res;

  // Write out uint64_t variables as doubles.
  // Note: this may discard some precision, but for JS there's no other option.
  res.Set("reads", static_cast<double>(reads));
  res.Set("reads_merged", static_cast<double>(reads_merged));
  res.Set("sectors_read", static_cast<double>(sectors_read));
  res.Set("read_time", static_cast<double>(read_time));
  res.Set("writes", static_cast<double>(writes));
  res.Set("writes_merged", static_cast<double>(writes_merged));
  res.Set("sectors_written", static_cast<double>(sectors_written));
  res.Set("write_time", static_cast<double>(write_time));
  res.Set("io", static_cast<double>(io));
  res.Set("io_time", static_cast<double>(io_time));
  res.Set("weighted_io_time", static_cast<double>(weighted_io_time));

  return res;
}

bool IsValidDiskName(StringPiece candidate) {
  if (candidate.length() < 3)
    return false;

  if (candidate[1] == 'd' &&
      (candidate[0] == 'h' || candidate[0] == 's' || candidate[0] == 'v')) {
    // [hsv]d[a-z]+ case
    for (size_t i = 2; i < candidate.length(); ++i) {
      if (!islower(candidate[i]))
        return false;
    }
    return true;
  }

  const char kMMCName[] = "mmcblk";
  if (!StartsWith(candidate, kMMCName))
    return false;

  // mmcblk[0-9]+ case
  for (size_t i = strlen(kMMCName); i < candidate.length(); ++i) {
    if (!isdigit(candidate[i]))
      return false;
  }
  return true;
}

bool GetSystemDiskInfo(SystemDiskInfo* diskinfo) {
  // Synchronously reading files in /proc does not hit the disk.
  ScopedAllowBlockingForProcessMetrics allow_blocking;

  FilePath diskinfo_file("/proc/diskstats");
  std::string diskinfo_data;
  if (!ReadFileToStringNonBlocking(diskinfo_file, &diskinfo_data)) {
    DLOG(WARNING) << "Failed to open " << diskinfo_file.value();
    return false;
  }

  std::vector<StringPiece> diskinfo_lines = SplitStringPiece(
      diskinfo_data, "\n", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
  if (diskinfo_lines.empty()) {
    DLOG(WARNING) << "No lines found";
    return false;
  }

  diskinfo->reads = 0;
  diskinfo->reads_merged = 0;
  diskinfo->sectors_read = 0;
  diskinfo->read_time = 0;
  diskinfo->writes = 0;
  diskinfo->writes_merged = 0;
  diskinfo->sectors_written = 0;
  diskinfo->write_time = 0;
  diskinfo->io = 0;
  diskinfo->io_time = 0;
  diskinfo->weighted_io_time = 0;

  uint64_t reads = 0;
  uint64_t reads_merged = 0;
  uint64_t sectors_read = 0;
  uint64_t read_time = 0;
  uint64_t writes = 0;
  uint64_t writes_merged = 0;
  uint64_t sectors_written = 0;
  uint64_t write_time = 0;
  uint64_t io = 0;
  uint64_t io_time = 0;
  uint64_t weighted_io_time = 0;

  for (const StringPiece& line : diskinfo_lines) {
    std::vector<StringPiece> disk_fields = SplitStringPiece(
        line, kWhitespaceASCII, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);

    // Fields may have overflowed and reset to zero.
    if (!IsValidDiskName(disk_fields[kDiskDriveName]))
      continue;

    StringToUint64(disk_fields[kDiskReads], &reads);
    StringToUint64(disk_fields[kDiskReadsMerged], &reads_merged);
    StringToUint64(disk_fields[kDiskSectorsRead], &sectors_read);
    StringToUint64(disk_fields[kDiskReadTime], &read_time);
    StringToUint64(disk_fields[kDiskWrites], &writes);
    StringToUint64(disk_fields[kDiskWritesMerged], &writes_merged);
    StringToUint64(disk_fields[kDiskSectorsWritten], &sectors_written);
    StringToUint64(disk_fields[kDiskWriteTime], &write_time);
    StringToUint64(disk_fields[kDiskIO], &io);
    StringToUint64(disk_fields[kDiskIOTime], &io_time);
    StringToUint64(disk_fields[kDiskWeightedIOTime], &weighted_io_time);

    diskinfo->reads += reads;
    diskinfo->reads_merged += reads_merged;
    diskinfo->sectors_read += sectors_read;
    diskinfo->read_time += read_time;
    diskinfo->writes += writes;
    diskinfo->writes_merged += writes_merged;
    diskinfo->sectors_written += sectors_written;
    diskinfo->write_time += write_time;
    diskinfo->io += io;
    diskinfo->io_time += io_time;
    diskinfo->weighted_io_time += weighted_io_time;
  }

  return true;
}

TimeDelta GetUserCpuTimeSinceBoot() {
  return internal::GetUserCpuTimeSinceBoot();
}

#if BUILDFLAG(IS_CHROMEOS)
Value::Dict SwapInfo::ToDict() const {
  Value::Dict res;

  // Write out uint64_t variables as doubles.
  // Note: this may discard some precision, but for JS there's no other option.
  res.Set("num_reads", static_cast<double>(num_reads));
  res.Set("num_writes", static_cast<double>(num_writes));
  res.Set("orig_data_size", static_cast<double>(orig_data_size));
  res.Set("compr_data_size", static_cast<double>(compr_data_size));
  res.Set("mem_used_total", static_cast<double>(mem_used_total));
  double ratio = compr_data_size ? static_cast<double>(orig_data_size) /
                                       static_cast<double>(compr_data_size)
                                 : 0;
  res.Set("compression_ratio", ratio);

  return res;
}

Value::Dict GraphicsMemoryInfoKB::ToDict() const {
  Value::Dict res;

  res.Set("gpu_objects", gpu_objects);
  res.Set("gpu_memory_size", static_cast<double>(gpu_memory_size));

  return res;
}

bool ParseZramMmStat(StringPiece mm_stat_data, SwapInfo* swap_info) {
  // There are 7 columns in /sys/block/zram0/mm_stat,
  // split by several spaces. The first three columns
  // are orig_data_size, compr_data_size and mem_used_total.
  // Example:
  // 17715200 5008166 566062  0 1225715712  127 183842
  //
  // For more details:
  // https://www.kernel.org/doc/Documentation/blockdev/zram.txt

  std::vector<StringPiece> tokens = SplitStringPiece(
      mm_stat_data, kWhitespaceASCII, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  if (tokens.size() < 7) {
    DLOG(WARNING) << "zram mm_stat: tokens: " << tokens.size()
                  << " malformed line: " << mm_stat_data;
    return false;
  }

  if (!StringToUint64(tokens[0], &swap_info->orig_data_size))
    return false;
  if (!StringToUint64(tokens[1], &swap_info->compr_data_size))
    return false;
  if (!StringToUint64(tokens[2], &swap_info->mem_used_total))
    return false;

  return true;
}

bool ParseZramStat(StringPiece stat_data, SwapInfo* swap_info) {
  // There are 11 columns in /sys/block/zram0/stat,
  // split by several spaces. The first column is read I/Os
  // and fifth column is write I/Os.
  // Example:
  // 299    0    2392    0    1    0    8    0    0    0    0
  //
  // For more details:
  // https://www.kernel.org/doc/Documentation/blockdev/zram.txt

  std::vector<StringPiece> tokens = SplitStringPiece(
      stat_data, kWhitespaceASCII, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  if (tokens.size() < 11) {
    DLOG(WARNING) << "zram stat: tokens: " << tokens.size()
                  << " malformed line: " << stat_data;
    return false;
  }

  if (!StringToUint64(tokens[0], &swap_info->num_reads))
    return false;
  if (!StringToUint64(tokens[4], &swap_info->num_writes))
    return false;

  return true;
}

namespace {

bool IgnoreZramFirstPage(uint64_t orig_data_size, SwapInfo* swap_info) {
  if (orig_data_size <= 4096) {
    // A single page is compressed at startup, and has a high compression
    // ratio. Ignore this as it doesn't indicate any real swapping.
    swap_info->orig_data_size = 0;
    swap_info->num_reads = 0;
    swap_info->num_writes = 0;
    swap_info->compr_data_size = 0;
    swap_info->mem_used_total = 0;
    return true;
  }
  return false;
}

void ParseZramPath(SwapInfo* swap_info) {
  FilePath zram_path("/sys/block/zram0");
  uint64_t orig_data_size =
      ReadFileToUint64(zram_path.Append("orig_data_size"));
  if (IgnoreZramFirstPage(orig_data_size, swap_info))
    return;

  swap_info->orig_data_size = orig_data_size;
  swap_info->num_reads = ReadFileToUint64(zram_path.Append("num_reads"));
  swap_info->num_writes = ReadFileToUint64(zram_path.Append("num_writes"));
  swap_info->compr_data_size =
      ReadFileToUint64(zram_path.Append("compr_data_size"));
  swap_info->mem_used_total =
      ReadFileToUint64(zram_path.Append("mem_used_total"));
}

bool GetSwapInfoImpl(SwapInfo* swap_info) {
  // Synchronously reading files in /sys/block/zram0 does not hit the disk.
  ScopedAllowBlockingForProcessMetrics allow_blocking;

  // Since ZRAM update, it shows the usage data in different places.
  // If file "/sys/block/zram0/mm_stat" exists, use the new way, otherwise,
  // use the old way.
  static absl::optional<bool> use_new_zram_interface;
  FilePath zram_mm_stat_file("/sys/block/zram0/mm_stat");
  if (!use_new_zram_interface.has_value()) {
    use_new_zram_interface = PathExists(zram_mm_stat_file);
  }

  if (!use_new_zram_interface.value()) {
    ParseZramPath(swap_info);
    return true;
  }

  std::string mm_stat_data;
  if (!ReadFileToStringNonBlocking(zram_mm_stat_file, &mm_stat_data)) {
    DLOG(WARNING) << "Failed to open " << zram_mm_stat_file.value();
    return false;
  }
  if (!ParseZramMmStat(mm_stat_data, swap_info)) {
    DLOG(WARNING) << "Failed to parse " << zram_mm_stat_file.value();
    return false;
  }
  if (IgnoreZramFirstPage(swap_info->orig_data_size, swap_info))
    return true;

  FilePath zram_stat_file("/sys/block/zram0/stat");
  std::string stat_data;
  if (!ReadFileToStringNonBlocking(zram_stat_file, &stat_data)) {
    DLOG(WARNING) << "Failed to open " << zram_stat_file.value();
    return false;
  }
  if (!ParseZramStat(stat_data, swap_info)) {
    DLOG(WARNING) << "Failed to parse " << zram_stat_file.value();
    return false;
  }

  return true;
}

}  // namespace

bool GetSwapInfo(SwapInfo* swap_info) {
  if (!GetSwapInfoImpl(swap_info)) {
    *swap_info = SwapInfo();
    return false;
  }
  return true;
}

bool GetGraphicsMemoryInfo(GraphicsMemoryInfoKB* gpu_meminfo) {
#if defined(ARCH_CPU_X86_FAMILY)
  // Reading i915_gem_objects on intel platform with kernel 5.4 is slow and is
  // prohibited.
  // TODO(b/170397975): Update if i915_gem_objects reading time is improved.
  static bool is_newer_kernel =
      base::StartsWith(base::SysInfo::KernelVersion(), "5.");
  static bool is_intel_cpu = base::CPU().vendor_name() == "GenuineIntel";
  if (is_newer_kernel && is_intel_cpu)
    return false;
#endif

#if defined(ARCH_CPU_ARM_FAMILY)
  const FilePath geminfo_path("/run/debugfs_gpu/exynos_gem_objects");
#else
  const FilePath geminfo_path("/run/debugfs_gpu/i915_gem_objects");
#endif
  std::string geminfo_data;
  gpu_meminfo->gpu_objects = -1;
  gpu_meminfo->gpu_memory_size = -1;
  if (ReadFileToStringNonBlocking(geminfo_path, &geminfo_data)) {
    int gpu_objects = -1;
    int64_t gpu_memory_size = -1;
    int num_res = sscanf(geminfo_data.c_str(), "%d objects, %" SCNd64 " bytes",
                         &gpu_objects, &gpu_memory_size);
    if (num_res == 2) {
      gpu_meminfo->gpu_objects = gpu_objects;
      gpu_meminfo->gpu_memory_size = gpu_memory_size;
    }
  }

#if defined(ARCH_CPU_ARM_FAMILY)
  // Incorporate Mali graphics memory if present.
  FilePath mali_memory_file("/sys/class/misc/mali0/device/memory");
  std::string mali_memory_data;
  if (ReadFileToStringNonBlocking(mali_memory_file, &mali_memory_data)) {
    int64_t mali_size = -1;
    int num_res =
        sscanf(mali_memory_data.c_str(), "%" SCNd64 " bytes", &mali_size);
    if (num_res == 1)
      gpu_meminfo->gpu_memory_size += mali_size;
  }
#endif  // defined(ARCH_CPU_ARM_FAMILY)

  return gpu_meminfo->gpu_memory_size != -1;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)
int ProcessMetrics::GetIdleWakeupsPerSecond() {
  uint64_t num_switches;
  static const char kSwitchStat[] = "voluntary_ctxt_switches";
  return ReadProcStatusAndGetFieldAsUint64(process_, kSwitchStat, &num_switches)
             ? CalculateIdleWakeupsPerSecond(num_switches)
             : 0;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)

}  // namespace base
