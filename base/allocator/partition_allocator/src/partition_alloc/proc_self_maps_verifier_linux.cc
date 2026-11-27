// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/proc_self_maps_verifier_linux.h"

#if PA_CONFIG(HAS_LINUX_KERNEL)

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>

#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/optional.h"
#include "partition_alloc/partition_alloc_base/types/to_address.h"

namespace partition_alloc::internal {

// static
ProcSelfMapsVerifier::CheckResult ProcSelfMapsVerifier::VerifyRange(
    uintptr_t no_write_start,
    uintptr_t required_mapped_start,
    uintptr_t check_end) {
  if (check_end == 0) {
    return CheckResult::kVerified;
  }
  ProcSelfMapsVerifier verifier(no_write_start, required_mapped_start,
                                check_end);
  return verifier.Verify();
}

ProcSelfMapsVerifier::ProcSelfMapsVerifier(uintptr_t no_write_start,
                                           uintptr_t required_mapped_start,
                                           uintptr_t check_end)
    : no_write_start_(no_write_start),
      check_end_(check_end),
      covered_until_(required_mapped_start) {}

// static
int ProcSelfMapsVerifier::OpenReadOnly(const char* path) {
  int fd = -1;
  do {
    fd = open(path, O_RDONLY | O_CLOEXEC);
  } while (fd < 0 && errno == EINTR);
  return fd;
}

// static
ssize_t ProcSelfMapsVerifier::ReadRetry(int fd, base::span<char> buf) {
  ssize_t n = -1;
  do {
    n = read(fd, buf.data(), buf.size());
  } while (n < 0 && errno == EINTR);
  return n;
}

// static
std::optional<uintptr_t> ProcSelfMapsVerifier::ParseHex(std::string_view& sv) {
  uintptr_t value = 0;
  const char* const begin = sv.data();
  const char* const end = base::to_address(sv.end());
  const auto [ptr, ec] = std::from_chars(begin, end, value, 16);
  if (ec != std::errc{}) {
    return std::nullopt;
  }
  sv.remove_prefix(static_cast<size_t>(ptr - begin));
  return value;
}

// Parses the address range and 4-char permissions from a `/proc/self/maps`
// line (e.g. "7fff7000-8fff7000 r--p ..."). Only inspects the prefix so
// arbitrarily long trailing pathnames are ignored.
// static
bool ProcSelfMapsVerifier::ParseLinePrefix(std::string_view line,
                                           uintptr_t& start_out,
                                           uintptr_t& end_out,
                                           std::array<char, 4>& perms_out) {
  const std::optional<uintptr_t> start = ParseHex(line);
  if (!start) {
    return false;
  }
  if (line.empty() || line[0] != '-') {
    return false;
  }
  line.remove_prefix(1);
  const std::optional<uintptr_t> end = ParseHex(line);
  if (!end) {
    return false;
  }
  if (line.size() < 5 || line[0] != ' ') {
    return false;
  }
  line.remove_prefix(1);
  if (line.size() > 4 && line[4] != ' ') {
    return false;
  }
  start_out = *start;
  end_out = *end;
  for (size_t i = 0; i < 4; ++i) {
    perms_out[i] = line[i];
  }
  return true;
}

void ProcSelfMapsVerifier::ProcessLine(std::string_view line) {
  uintptr_t map_start = 0;
  uintptr_t map_end = 0;
  std::array<char, 4> perms{};
  if (!ParseLinePrefix(line, map_start, map_end, perms) ||
      map_end < map_start) {
    result_ = CheckResult::kUnreadable;
    done_ = true;
    return;
  }
  if (map_end == map_start) {
    return;
  }
  if (map_start >= check_end_) {
    done_ = true;
    return;
  }
  if (map_end > no_write_start_ && perms[1] != '-') {
    result_ = CheckResult::kFailed;
    done_ = true;
    return;
  }
  if (covered_until_ < check_end_ && map_end > covered_until_) {
    if (map_start > covered_until_) {
      result_ = CheckResult::kFailed;
      done_ = true;
      return;
    }
    if ((perms[0] != '-' && perms[0] != 'r') || perms[1] != '-' ||
        perms[2] != '-') {
      result_ = CheckResult::kFailed;
      done_ = true;
      return;
    }
    covered_until_ = std::min(map_end, check_end_);
  }
}

ProcSelfMapsVerifier::CheckResult ProcSelfMapsVerifier::Verify() {
  const int fd = OpenReadOnly("/proc/self/maps");
  if (fd < 0) {
    return CheckResult::kUnreadable;
  }

  std::array<char, 4096> buf;
  size_t buffered = 0;

  while (!done_) {
    base::span<char> free_space = base::span<char>(buf).subspan(buffered);
    const ssize_t n = ReadRetry(fd, free_space);
    if (n < 0) {
      result_ = CheckResult::kUnreadable;
      break;
    }
    if (n == 0) {
      if (buffered > 0 && !skipping_long_line_) {
        ProcessLine(
            base::as_string_view(base::span<const char>(buf).first(buffered)));
      }
      break;
    }

    buffered += static_cast<size_t>(n);
    const std::string_view data =
        base::as_string_view(base::span<const char>(buf).first(buffered));
    size_t consumed = 0;

    while (!done_ && consumed < data.size()) {
      const size_t newline = data.find('\n', consumed);
      if (newline == std::string_view::npos) {
        break;
      }
      if (skipping_long_line_) {
        skipping_long_line_ = false;
      } else {
        ProcessLine(data.substr(consumed, newline - consumed));
      }
      consumed = newline + 1;
    }

    if (done_) {
      break;
    }

    const size_t remaining = buffered - consumed;
    if (consumed == 0 && remaining == buf.size()) {
      // Single line exceeded the stack buffer. Parse its prefix now (which
      // always fits well within the buffer) and skip the rest until '\n'.
      if (!skipping_long_line_) {
        ProcessLine(data);
        skipping_long_line_ = true;
      }
      buffered = 0;
    } else if (remaining > 0 && consumed > 0) {
      base::span<char>(buf).first(remaining).copy_from(
          base::span<const char>(buf).subspan(consumed, remaining));
      buffered = remaining;
    } else if (remaining == 0) {
      buffered = 0;
    }
  }

  close(fd);

  if (result_ == CheckResult::kVerified && covered_until_ < check_end_) {
    return CheckResult::kFailed;
  }
  return result_;
}

}  // namespace partition_alloc::internal

#endif  // PA_CONFIG(HAS_LINUX_KERNEL)
