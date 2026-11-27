// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PROC_SELF_MAPS_VERIFIER_LINUX_H_
#define PARTITION_ALLOC_PROC_SELF_MAPS_VERIFIER_LINUX_H_

#include "partition_alloc/partition_alloc_config.h"

#if PA_CONFIG(HAS_LINUX_KERNEL)

#include <sys/types.h>

#include <array>
#include <cstdint>
#include <string_view>

#include "partition_alloc/partition_alloc_base/containers/span.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/optional.h"

namespace partition_alloc::internal {

// Reads `/proc/self/maps` without heap allocations to verify memory mapping
// invariants for a target address range during early initialization.
class ProcSelfMapsVerifier {
 public:
  enum class CheckResult {
    // Confirmed no writable mappings in `[no_write_start, check_end)` and
    // contiguous `PROT_NONE`/`PROT_READ` coverage of
    // `[required_mapped_start, check_end)`.
    kVerified,
    // Found a writable mapping in `[no_write_start, check_end)` or an unmapped
    // gap / non-(PROT_NONE|PROT_READ) mapping in
    // `[required_mapped_start, check_end)`.
    kFailed,
    // Could not open, read, or parse `/proc/self/maps`.
    kUnreadable,
  };

  ProcSelfMapsVerifier(const ProcSelfMapsVerifier&) = delete;
  ProcSelfMapsVerifier& operator=(const ProcSelfMapsVerifier&) = delete;

  static CheckResult VerifyRange(uintptr_t no_write_start,
                                 uintptr_t required_mapped_start,
                                 uintptr_t check_end);

 private:
  ProcSelfMapsVerifier(uintptr_t no_write_start,
                       uintptr_t required_mapped_start,
                       uintptr_t check_end);

  CheckResult Verify();
  void ProcessLine(std::string_view line);

  static std::optional<uintptr_t> ParseHex(std::string_view& sv);
  static bool ParseLinePrefix(std::string_view line,
                              uintptr_t& start_out,
                              uintptr_t& end_out,
                              std::array<char, 4>& perms_out);
  static int OpenReadOnly(const char* path);
  static ssize_t ReadRetry(int fd, base::span<char> buf);

  const uintptr_t no_write_start_;
  const uintptr_t check_end_;
  uintptr_t covered_until_;
  CheckResult result_ = CheckResult::kVerified;
  bool done_ = false;
  bool skipping_long_line_ = false;
};

}  // namespace partition_alloc::internal

#endif  // PA_CONFIG(HAS_LINUX_KERNEL)

#endif  // PARTITION_ALLOC_PROC_SELF_MAPS_VERIFIER_LINUX_H_
