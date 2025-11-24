// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/pmf_utils.h"

#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

#include <optional>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace base::android {
namespace {
std::optional<ByteCount> CalculateProcessMemoryFootprint(
    base::File& statm_file,
    base::File& status_file) {
  // Get total resident and shared sizes from statm file.
  static size_t page_size = static_cast<size_t>(getpagesize());
  uint64_t resident_pages = 0;
  uint64_t shared_pages = 0;
  uint64_t vm_size_pages = 0;
  uint64_t swap_footprint_kb = 0;
  constexpr uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];
  base::span<uint8_t> line_span = base::as_writable_byte_span(line);

  std::optional<size_t> n =
      statm_file.ReadAtCurrentPos(line_span.first<kMaxLineSize - 1>());
  if (n.value_or(0) == 0) {
    return std::optional<ByteCount>();
  }
  line_span[*n] = 0;

  int num_scanned =
      UNSAFE_TODO(sscanf(line, "%" SCNu64 " %" SCNu64 " %" SCNu64,
                         &vm_size_pages, &resident_pages, &shared_pages));
  if (num_scanned != 3) {
    return std::optional<ByteCount>();
  }

  // Get swap size from status file. The format is: VmSwap :  10 kB.
  n = status_file.ReadAtCurrentPos(line_span.first<kMaxLineSize - 1>());
  if (n.value_or(0) == 0) {
    return std::optional<ByteCount>();
  }
  line_span[*n] = 0;

  char* swap_line = UNSAFE_TODO(strstr(line, "VmSwap"));
  if (!swap_line) {
    return std::optional<ByteCount>();
  }
  num_scanned = UNSAFE_TODO(
      sscanf(swap_line, "VmSwap: %" SCNu64 " kB", &swap_footprint_kb));
  if (num_scanned != 1) {
    return std::optional<ByteCount>();
  }

  return ByteCount::FromUnsigned((resident_pages - shared_pages) * page_size) +
         KiB(swap_footprint_kb);
}
}  // namespace

// static
std::optional<ByteCount> PmfUtils::CalculatePrivateMemoryFootprintForTesting(
    base::File& statm_file,
    base::File& status_file) {
  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

// static
std::optional<ByteCount>
PmfUtils::GetPrivateMemoryFootprintForCurrentProcess() {
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
    return std::optional<ByteCount>();
  }

  return CalculateProcessMemoryFootprint(statm_file, status_file);
}

}  // namespace base::android
