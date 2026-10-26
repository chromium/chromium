// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/reentrancy_guard.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && \
    (PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_WIN))

namespace base::internal {

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
std::atomic<int> ReentrancyState::Guard::guard_count_{0};
std::array<std::atomic<ReentrancyState::PlatformThreadRef>,
           ReentrancyState::Guard::kCapacity>
    ReentrancyState::Guard::threads_;
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

ReentrancyState::TlsKey ReentrancyState::key_{kInvalidTlsKey};
partition_alloc::internal::Lock ReentrancyState::creation_lock_;
bool ReentrancyState::key_created_{false};

// static
void ReentrancyState::InitializeKey() {
  partition_alloc::internal::ScopedGuard scope(creation_lock_);
  if (key_created_) {
    return;
  }
#if PA_BUILDFLAG(IS_POSIX)
  int error = pthread_key_create(&key_, nullptr);
  PA_CHECK(error == 0);
#elif PA_BUILDFLAG(IS_WIN)
  key_ = TlsAlloc();
  PA_CHECK(key_ != TLS_OUT_OF_INDEXES);
#endif  // PA_BUILDFLAG(IS_WIN)
  key_created_ = true;
}

// static
void ReentrancyState::ResetKeyForTesting() {
  partition_alloc::internal::ScopedGuard scope(creation_lock_);
  if (!key_created_) {
    return;
  }
#if PA_BUILDFLAG(IS_POSIX)
  PA_CHECK(0 == pthread_key_delete(key_));
#endif  // PA_BUILDFLAG(IS_POSIX)
#if PA_BUILDFLAG(IS_WIN)
  PA_CHECK(TlsFree(key_));
#endif
  key_ = kInvalidTlsKey;
  key_created_ = false;
}

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && (PA_BUILDFLAG(IS_POSIX)
        // ||
        // PA_BUILDFLAG(IS_WIN))
