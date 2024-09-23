// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_file.h"

#include <dlfcn.h>

#include <algorithm>
#include <array>
#include <atomic>

#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/immediate_crash.h"
#include "base/logging.h"

namespace {

// We want to avoid any kind of allocations in our close() implementation, so we
// use a fixed-size table. Given our common FD limits and the preference for new
// FD allocations to use the lowest available descriptor, this should be
// sufficient to guard most FD lifetimes. The worst case scenario if someone
// attempts to own a higher FD is that we don't track it.
const int kMaxTrackedFds = 4096;

std::atomic_bool g_is_ownership_enforced{false};
std::array<std::atomic_bool, kMaxTrackedFds> g_is_fd_owned;

NOINLINE void CrashOnFdOwnershipViolation() {
  RAW_LOG(ERROR, "Crashing due to FD ownership violation:\n");
  base::debug::StackTrace().Print();
  base::ImmediateCrash();
}

bool CanTrack(int fd) {
  return fd >= 0 && fd < kMaxTrackedFds;
}

void UpdateAndCheckFdOwnership(int fd, bool owned) {
  if (CanTrack(fd) &&
      g_is_fd_owned[static_cast<size_t>(fd)].exchange(owned) == owned &&
      g_is_ownership_enforced) {
    CrashOnFdOwnershipViolation();
  }
}

}  // namespace

namespace base {
namespace internal {

// static
void ScopedFDCloseTraits::Acquire(const ScopedFD& owner, int fd) {
  UpdateAndCheckFdOwnership(fd, /*owned=*/true);
}

// static
void ScopedFDCloseTraits::Release(const ScopedFD& owner, int fd) {
  UpdateAndCheckFdOwnership(fd, /*owned=*/false);
}

}  // namespace internal

namespace subtle {

#if !defined(COMPONENT_BUILD)
void EnableFDOwnershipEnforcement(bool enabled) {
  g_is_ownership_enforced = enabled;
}
#endif  // !defined(COMPONENT_BUILD)

void ResetFDOwnership() {
  std::fill(g_is_fd_owned.begin(), g_is_fd_owned.end(), false);
}

}  // namespace subtle

bool IsFDOwned(int fd) {
  return CanTrack(fd) && g_is_fd_owned[static_cast<size_t>(fd)];
}

}  // namespace base

#if !defined(COMPONENT_BUILD)
using LibcCloseFuncPtr = int (*)(int);

// Load the libc close symbol to forward to from the close wrapper.
LibcCloseFuncPtr LoadCloseSymbol() {
#if defined(THREAD_SANITIZER)
  // If TSAN is enabled use __interceptor___close first to make sure the TSAN
  // wrapper gets called.
  return reinterpret_cast<LibcCloseFuncPtr>(
      dlsym(RTLD_DEFAULT, "__interceptor___close"));
#else
  return reinterpret_cast<LibcCloseFuncPtr>(dlsym(RTLD_NEXT, "close"));
#endif
}

extern "C" {

NO_SANITIZE("cfi-icall")
__attribute__((visibility("default"), noinline)) int close(int fd) {
  static LibcCloseFuncPtr libc_close = LoadCloseSymbol();
  if (base::IsFDOwned(fd) && g_is_ownership_enforced)
    CrashOnFdOwnershipViolation();
  if (libc_close == nullptr) {
    RAW_LOG(ERROR, "close symbol missing\n");
    base::ImmediateCrash();
  }
  return libc_close(fd);
}

}  // extern "C"
#endif  // !defined(COMPONENT_BUILD)
