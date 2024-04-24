// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/android/pmf_utils.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace base::android {
namespace {
std::optional<uint64_t> CalculateProcessMemoryFootprint(
    base::File& statm_file,
    base::File& status_file) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = static_cast<size_t>(getpagesize());
  uint64_t resident_pages = 0;
  uint64_t shared_pages = 0;
  uint64_t vm_size_pages = 0;
  uint64_t swap_footprint = 0;
  constexpr uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];

  int n = statm_file.ReadAtCurrentPos(line, sizeof(line) - 1);
  if (n <= 0) {
    return std::optional<size_t>();
  }
  line[n] = '\0';

  int num_scanned = sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                           &vm_size_pages, &resident_pages, &shared_pages);
  if (num_scanned != 3) {
    return std::optional<size_t>();
  }

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  n = status_file.ReadAtCurrentPos(line, sizeof(line) - 1);
  if (n <= 0) {
    return std::optional<size_t>();
  }
  line[n] = '\0';

  char* swap_line = strstr(line, "VmSwap");
  if (!swap_line) {
    return std::optional<size_t>();
  }
  num_scanned = sscanf(swap_line, "VmSwap: %" SCNu64 " kB", &swap_footprint);
  if (num_scanned != 1) {
    return std::optional<size_t>();
  }

  swap_footprint *= 1024;
  return (resident_pages - shared_pages) * page_size + swap_footprint;
}
}  // namespace

// static
std::optional<uint64_t> PmfUtils::CalculatePrivateMemoryFootprintForTesting(
    base::File& statm_file,
    base::File& status_file) {
  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

// static
std::optional<uint64_t> PmfUtils::GetPrivateMemoryFootprintForCurrentProcess() {
  // ScopedAllowBlocking is required to use base::File, but /proc/{pid}/status
  // and /proc/{pid}/statm are not regular files. For example, regarding linux,
  // proc_pid_statm() defined in fs/proc/array.c is invoked when reading
  // /proc/{pid}/statm. proc_pid_statm() gets task information and directly
  // writes the information into the given seq_file. This is different from
  // regular file operations.
  base::ScopedAllowBlocking allow_blocking;

  base::FilePath proc_self_dir = base::FilePath("/proc/self");
  base::File status_file(
      proc_self_dir.Append("status"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::File statm_file(
      proc_self_dir.Append("statm"),
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!status_file.IsValid() || !statm_file.IsValid()) {
    return std::optional<size_t>();
  }

  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

}  // namespace base::android
