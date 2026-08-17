// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/dummy_memory_consumer_registry.h"

namespace base {

DummyMemoryConsumerRegistry::DummyMemoryConsumerRegistry() = default;

DummyMemoryConsumerRegistry::~DummyMemoryConsumerRegistry() {
  NotifyDestruction();
}

void DummyMemoryConsumerRegistry::OnMemoryConsumerAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer) {}

void DummyMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    uint32_t consumer_id,
    MemoryConsumer* consumer) {}

}  // namespace base
