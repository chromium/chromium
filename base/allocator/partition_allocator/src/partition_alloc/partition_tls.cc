// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_tls.h"

namespace partition_alloc::internal {

namespace {
constinit PartitionTlsRegistry g_instance;
}

PartitionTlsRegistry& PartitionTlsRegistry::Instance() {
  return g_instance;
}

void PartitionTlsRegistry::Register(PartitionTls* tls) {
  internal::ScopedGuard scoped_locker(GetLock());
  if (tls->is_registered) {
    return;
  }
  tls->is_registered = true;
  tls->prev_ = nullptr;
  tls->next_ = list_head_;
  if (list_head_) {
    list_head_->prev_ = tls;
  }
  list_head_ = tls;
}

void PartitionTlsRegistry::Unregister(PartitionTls* tls) {
  internal::ScopedGuard scoped_locker(GetLock());
  if (!tls->is_registered) {
    return;
  }
  tls->is_registered = false;
  if (tls->prev_) {
    tls->prev_->next_ = tls->next_;
  }
  if (tls->next_) {
    tls->next_->prev_ = tls->prev_;
  }
  if (tls == list_head_) {
    list_head_ = tls->next_;
  }
  tls->next_ = nullptr;
  tls->prev_ = nullptr;
}

bool PartitionTlsRegistry::IsRegisteredForTesting(PartitionTls* tls) {
  internal::ScopedGuard scoped_locker(GetLock());
  for (auto* cur = list_head_; cur; cur = cur->next_) {
    if (cur == tls) {
      return true;
    }
  }
  return false;
}

void PartitionTlsRegistry::ResetForTesting() {
  internal::ScopedGuard scoped_locker(GetLock());
  list_head_ = nullptr;
}

}  // namespace partition_alloc::internal
