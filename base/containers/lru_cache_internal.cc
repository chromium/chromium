// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/lru_cache_internal.h"

#include "base/memory_coordinator/traits.h"

namespace base::internal {

namespace {

constexpr MemoryConsumerTraits kDefaultTraits =
    MemoryConsumerTraits(MemoryConsumerTraits::ConsumerType::kPassive);

}  // namespace

MemoryConsumerImpl::MemoryConsumerImpl()
    : registration_(
          "LRUCache",
          kDefaultTraits,
          this,
          // Allow LRUCache instances stored in base::NoDestructor
          // singletons or long-lived test fixtures to outlive the
          // MemoryConsumerRegistry.
          AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled) {}

MemoryConsumerImpl::~MemoryConsumerImpl() = default;

void MemoryConsumerImpl::OnUpdateMemoryLimit() {
  current_memory_limit_.store(memory_limit(), std::memory_order_relaxed);
}

}  // namespace base::internal
