// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/zero_segment.h"

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS) && \
    PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)

#include <cstddef>
#include <cstdint>

#include "partition_alloc/internal/page_allocator_internal.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#include "partition_alloc/partition_alloc_check.h"

#if PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)
#include <fcntl.h>
#include <sanitizer/asan_interface.h>
#include <unistd.h>

#include <array>
#include <bit>
#include <cerrno>
#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/containers/span.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/optional.h"
#include "partition_alloc/partition_alloc_base/types/to_address.h"
#include "partition_alloc/proc_self_maps_verifier_linux.h"
#endif

namespace partition_alloc::internal {

namespace {

// Reserves `[address, address + size)` as inaccessible via `SystemAllocPages`
// (hint-only `mmap`, avoiding `MAP_FIXED_NOREPLACE` which requires glibc 2.28+
// and is disallowed by Chromium's Linux seccomp-BPF `RestrictMmapFlags`).
// Releases any relocated mapping and returns false on failure.
bool TryReserveInaccessibleRegion(uintptr_t address, size_t size) {
  PA_DCHECK(address != 0);
  const uintptr_t allocated_base =
      SystemAllocPages(address, size,
                       PageAccessibilityConfiguration(
                           PageAccessibilityConfiguration::kInaccessible),
                       PageTag::kPartitionAlloc);
  if (allocated_base == address) {
    return true;
  }
  if (allocated_base) {
    FreePages(allocated_base, size);
  }
  return false;
}

#if PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)

// Parses an unsigned decimal integer from `text` (allowing optional trailing
// whitespace). Returns `std::nullopt` on empty input, non-digit prefix,
// trailing non-whitespace characters, or `uint64_t` overflow.
std::optional<uint64_t> ParseUnsignedDecimal(std::string_view text) {
  uint64_t value = 0;
  const char* const begin = text.data();
  const char* const end = base::to_address(text.end());
  const auto [ptr, ec] = std::from_chars(begin, end, value);
  if (ec != std::errc{}) {
    return std::nullopt;
  }
  const std::string_view trailing =
      text.substr(static_cast<size_t>(ptr - begin));
  if (trailing.find_first_not_of(" \t\r\n") != std::string_view::npos) {
    return std::nullopt;
  }
  return value;
}

// Reads `/proc/sys/vm/mmap_min_addr` using only a stack buffer (no heap
// allocations), clamps to `[page_size, max_addr]`, and rounds up to a
// `page_size` multiple. Falls back to `AlignUp(64 KiB, page_size)` (clamped to
// `[page_size, max_addr]`) on read or parse failure.
uintptr_t ReadMmapMinAddr(size_t page_size, uintptr_t max_addr) {
  PA_DCHECK(std::has_single_bit(page_size));
  PA_DCHECK(max_addr >= page_size);
  PA_DCHECK(max_addr % page_size == 0);

  constexpr size_t kDefaultMmapMinAddr = 64 * 1024;
  const uintptr_t fallback = std::clamp<uintptr_t>(
      base::bits::AlignUp<uintptr_t>(kDefaultMmapMinAddr, page_size), page_size,
      max_addr);

  int fd = -1;
  do {
    fd = open("/proc/sys/vm/mmap_min_addr", O_RDONLY | O_CLOEXEC);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    return fallback;
  }

  std::array<char, 64> buf{};
  base::span<char> remaining(buf);
  size_t total = 0;
  while (!remaining.empty()) {
    ssize_t n = -1;
    do {
      n = read(fd, remaining.data(), remaining.size());
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
      close(fd);
      return fallback;
    }
    if (n == 0) {
      break;
    }
    const size_t bytes_read = static_cast<size_t>(n);
    total += bytes_read;
    remaining = remaining.subspan(bytes_read);
  }
  close(fd);

  // Reject empty or unexpectedly long content (a 64-bit decimal integer fits
  // well within 64 bytes).
  if (total == 0 || remaining.empty()) {
    return fallback;
  }

  const std::optional<uint64_t> raw_value = ParseUnsignedDecimal(
      base::as_string_view(base::span<const char>(buf).first(total)));
  if (!raw_value) {
    return fallback;
  }

  if (*raw_value >= static_cast<uint64_t>(max_addr)) {
    return max_addr;
  }
  if (*raw_value < static_cast<uint64_t>(page_size)) {
    return page_size;
  }
  return base::bits::AlignUp<uintptr_t>(static_cast<uintptr_t>(*raw_value),
                                        page_size);
}

// Probes whether `address` (page-aligned) is already covered by a mapping
// without modifying existing mappings. Uses hint-only `SystemAllocPages`
// (seccomp-BPF safe) and immediately releases the probe mapping. Used as a
// fallback only when `/proc/self/maps` is unreadable (e.g. in a sandboxed
// process).
bool IsAddressAlreadyMapped(uintptr_t address, size_t page_size) {
  const uintptr_t mapped =
      SystemAllocPages(address, page_size,
                       PageAccessibilityConfiguration(
                           PageAccessibilityConfiguration::kInaccessible),
                       PageTag::kPartitionAlloc);
  if (!mapped) {
    return false;
  }
  FreePages(mapped, page_size);
  return mapped != address;
}

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// When PartitionAlloc is not the system allocator (ASan builds),
// `PartitionAddressSpace::Init()` is not called before `main()` (and may never
// be called in standalone binaries like `d8`). Run right after ASan's
// `.preinit_array` (`__asan_init`) and before C++ static initializers.
__attribute__((constructor(101))) void InitZeroSegmentAtStartup() {
  ZeroSegment::Init();
}
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#endif  // PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)

}  // namespace

size_t ZeroSegment::zero_segment_size_ = 0;

// static
void ZeroSegment::Init() {
  const size_t min_zero_segment_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;

#if PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)
  // Run at most once even if initialization bails out gracefully and leaves
  // `zero_segment_size_ == 0` (e.g. on non-PIE binaries or non-Native-Zygote
  // Android ART processes).
  static bool initialized = false;
  if (initialized) {
    return;
  }
  initialized = true;

  zero_segment_size_ = InitForAsan(min_zero_segment_size);
#else
  if (zero_segment_size_) {
    return;
  }

  const WellKnownReadOnlyRegions well_known =
      GetWellKnownReadOnlyRegions(min_zero_segment_size);
  PA_CHECK(well_known.count <= WellKnownReadOnlyRegions::kMaxRegions);
  // `well_known` covers 0-XGB as well if supported.
  if (well_known.count == 0 || well_known.regions[0].address != 0) {
    return;
  }

  uintptr_t current_addr = 0;

  // Attempts to fill any gap in `[current_addr, target_addr)`. Returns true if
  // the entire gap was filled (or no gap existed), or false if reservation
  // failed (in which case `zero_segment_size_` is set to the contiguous prefix
  // acquired so far).
  const auto fill_gap_up_to = [&](uintptr_t target_addr) -> bool {
    if (target_addr <= current_addr) {
      return true;
    }
    const size_t granularity = PageAllocationGranularity();
    PA_DCHECK(current_addr % granularity == 0);
    PA_DCHECK(target_addr % granularity == 0);
    if (current_addr % granularity != 0 || target_addr % granularity != 0 ||
        !TryReserveInaccessibleRegion(current_addr,
                                      target_addr - current_addr)) {
      zero_segment_size_ = current_addr;
      return false;
    }
    return true;
  };

  for (size_t i = 0; i < well_known.count; ++i) {
    const auto& region = well_known.regions[i];
    if (region.address >= min_zero_segment_size) {
      break;
    }
    if (!fill_gap_up_to(region.address)) {
      return;
    }
    current_addr = region.address + region.size;
  }

  // Reserve the trailing gap if applicable.
  if (!fill_gap_up_to(min_zero_segment_size)) {
    return;
  }

  zero_segment_size_ = std::max(current_addr, min_zero_segment_size);
#endif  // PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)
}

#if PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)

// static
size_t ZeroSegment::ReinitForAsanForTesting() {
  const size_t min_zero_segment_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;
  return InitForAsan(min_zero_segment_size);
}

// Establishes a non-writable zero segment `[0, target_end)` under ASan on
// Linux/ChromeOS/Android without heap allocations. Returns `target_end` on
// success or `0` on graceful bailout (leaving ASan's `LowShadow` untouched).
// static
size_t ZeroSegment::InitForAsan(size_t min_zero_segment_size) {
  if (min_zero_segment_size == 0) {
    return 0;
  }

  const size_t page_size = SystemPageSize();
  PA_CHECK(std::has_single_bit(page_size));
  PA_CHECK(min_zero_segment_size <=
           std::numeric_limits<size_t>::max() - (page_size - 1));
  const uintptr_t target_end =
      base::bits::AlignUp<uintptr_t>(min_zero_segment_size, page_size);

  size_t shadow_scale = 0;
  size_t shadow_offset = 0;
  __asan_get_shadow_mapping(&shadow_scale, &shadow_offset);

  // Zero-based shadow (`shadow_offset == 0`): ASan's `ShadowGap` already maps
  // `[mmap_min_addr, target_end)` (stepping up to `kZeroBaseMaxShadowStart` =
  // 256 KiB if LSM enforces a higher minimum address) as `PROT_NONE` unless
  // `ASAN_OPTIONS=protect_shadow_gap=0` was set.
  if (shadow_offset == 0) {
    constexpr size_t kZeroBaseMaxShadowStart = 256 * 1024;
    const uintptr_t low_mem_start = ReadMmapMinAddr(page_size, target_end);
    const uintptr_t required_mapped_start = std::min(
        target_end,
        std::max(low_mem_start, base::bits::AlignUp<uintptr_t>(
                                    kZeroBaseMaxShadowStart, page_size)));
    const ProcSelfMapsVerifier::CheckResult check =
        ProcSelfMapsVerifier::VerifyRange(0, required_mapped_start, target_end);
    if (check == ProcSelfMapsVerifier::CheckResult::kFailed) {
      return 0;
    }
    if (check == ProcSelfMapsVerifier::CheckResult::kUnreadable &&
        !IsAddressAlreadyMapped(target_end - page_size, page_size)) {
      return 0;
    }
    return target_end;
  }

  // Compute `LowShadow` bounds matching compiler-rt's
  // `InitializeShadowMemory()`.
  PA_CHECK(shadow_scale < sizeof(uintptr_t) * 8);
  PA_CHECK(shadow_offset >= page_size);
  PA_CHECK(shadow_offset % page_size == 0);
  const uintptr_t low_shadow_start = shadow_offset - page_size;
  const uintptr_t low_shadow_scaled = (shadow_offset - 1) >> shadow_scale;
  PA_CHECK(shadow_offset <=
           std::numeric_limits<uintptr_t>::max() - low_shadow_scaled - 1);
  const uintptr_t low_shadow_end = (low_shadow_scaled + shadow_offset) + 1;
  PA_CHECK(low_shadow_end % page_size == 0);

  // If `ShadowGap` overlaps `[0, target_end)` (e.g. x64 ASan where
  // `low_shadow_end` is ~2.25 GiB < 4097 MiB), verify that ASan protected
  // `ShadowGap` before modifying `LowMem` or `LowShadow`. This handles
  // `ASAN_OPTIONS=protect_shadow_gap=0` gracefully in both release and DCHECK
  // builds without claiming a zero segment or altering `LowShadow`.
  if (low_shadow_end < target_end) {
    const ProcSelfMapsVerifier::CheckResult gap_check =
        ProcSelfMapsVerifier::VerifyRange(low_shadow_end, low_shadow_end,
                                          target_end);
    if (gap_check == ProcSelfMapsVerifier::CheckResult::kFailed) {
      return 0;
    }
    if (gap_check == ProcSelfMapsVerifier::CheckResult::kUnreadable &&
        (!IsAddressAlreadyMapped(low_shadow_end, page_size) ||
         !IsAddressAlreadyMapped(target_end - page_size, page_size))) {
      return 0;
    }
  }

  // Reserve `LowMem` `[low_mem_start, low_mem_end)` as `PROT_NONE` BEFORE
  // write-protecting `LowShadow`, because ASan's `mmap` handling may touch
  // `LowShadow` while mapping into `LowMem`. If reservation fails (e.g.
  // non-PIE binaries or Android ART processes with pre-existing low mappings),
  // bail out gracefully without modifying `LowShadow`.
  uintptr_t low_mem_start = ReadMmapMinAddr(page_size, target_end);
  const uintptr_t low_mem_end = std::min(target_end, low_shadow_start);
  if (low_mem_start < low_mem_end) {
    constexpr size_t kMaxSteppedMmapMinAddr = 256 * 1024;
    const uintptr_t max_low_mem_start = std::min(
        low_mem_end - page_size,
        base::bits::AlignUp<uintptr_t>(kMaxSteppedMmapMinAddr, page_size));

    bool reserved = TryReserveInaccessibleRegion(low_mem_start,
                                                 low_mem_end - low_mem_start);
    if (!reserved) {
      // If multiple DSOs statically link PartitionAlloc, another copy's
      // startup constructor may have already established the zero segment.
      // Adopt `target_end` without modifying any mappings when
      // `/proc/self/maps` confirms `[0, target_end)` already satisfies the
      // zero-segment invariants (no writable mappings and contiguous
      // `PROT_NONE`/`PROT_READ` coverage from `required_mapped_start`,
      // accounting for LSM stepping up to `max_low_mem_start`).
      const uintptr_t required_mapped_start =
          std::max(low_mem_start, max_low_mem_start);
      if (ProcSelfMapsVerifier::VerifyRange(0, required_mapped_start,
                                            target_end) ==
          ProcSelfMapsVerifier::CheckResult::kVerified) {
        return target_end;
      }

      // If an LSM enforces a higher minimum address than
      // `/proc/sys/vm/mmap_min_addr`, hint-only `mmap` silently relocates
      // below the effective minimum. Probe 1 page at a time up to
      // `max_low_mem_start` to find the lowest mappable address and retry once
      // if `low_mem_start` advanced.
      const uintptr_t initial_low_mem_start = low_mem_start;
      while (low_mem_start < max_low_mem_start) {
        if (TryReserveInaccessibleRegion(low_mem_start, page_size)) {
          FreePages(low_mem_start, page_size);
          break;
        }
        low_mem_start += page_size;
      }
      // Verify `[0, low_mem_start)` has no writable mappings before retrying
      // the stepped reservation. Any non-writable mapping in that range is
      // safe (and unmapping it later under ASan would fault on read-only
      // `LowShadow`).
      if (low_mem_start != initial_low_mem_start &&
          ProcSelfMapsVerifier::VerifyRange(0, low_mem_start, low_mem_start) ==
              ProcSelfMapsVerifier::CheckResult::kVerified) {
        reserved = TryReserveInaccessibleRegion(low_mem_start,
                                                low_mem_end - low_mem_start);
      }
      if (!reserved) {
        return 0;
      }
    }
  }

  // Write-protect `LowShadow` (keeping `PROT_READ` so ASan's shadow reads for
  // null/near-null dereference reporting continue to work).
  if (low_shadow_start < target_end) {
    const uintptr_t protect_end = std::min(target_end, low_shadow_end);
    PA_CHECK(TrySetSystemPagesAccess(
        low_shadow_start, protect_end - low_shadow_start,
        PageAccessibilityConfiguration(PageAccessibilityConfiguration::kRead)));
  }

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  // Verify post-condition via `/proc/self/maps` when readable: `[0,
  // target_end)` has no writable mappings and `[low_mem_start, target_end)` is
  // contiguously covered by `PROT_NONE`/`PROT_READ` mappings.
  const ProcSelfMapsVerifier::CheckResult post_check =
      ProcSelfMapsVerifier::VerifyRange(0, low_mem_start, target_end);
  PA_DCHECK(post_check != ProcSelfMapsVerifier::CheckResult::kFailed);
#endif

  return target_end;
}

#endif  // PA_BUILDFLAG(IS_ASAN) && PA_CONFIG(HAS_LINUX_KERNEL)

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS) &&
        // PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
