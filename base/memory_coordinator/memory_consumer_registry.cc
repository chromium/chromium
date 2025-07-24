// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_registry.h"

#include "base/check.h"
#include "base/check_op.h"

namespace base {

namespace {

MemoryConsumerRegistry* g_memory_consumer_registry = nullptr;

}  // namespace

void RegisteredMemoryConsumer::UpdateMemoryLimit(int percentage) {
  memory_consumer_->UpdateMemoryLimit(percentage);
}

void RegisteredMemoryConsumer::ReleaseMemory() {
  memory_consumer_->ReleaseMemory();
}

RegisteredMemoryConsumer::RegisteredMemoryConsumer(
    MemoryConsumer* memory_consumer)
    : memory_consumer_(memory_consumer) {}

// static
MemoryConsumerRegistry& MemoryConsumerRegistry::Get() {
  CHECK(g_memory_consumer_registry);
  return *g_memory_consumer_registry;
}

// static
void MemoryConsumerRegistry::Set(MemoryConsumerRegistry* instance) {
  CHECK_NE(bool(g_memory_consumer_registry), bool(instance));
  g_memory_consumer_registry = instance;
}

void MemoryConsumerRegistry::AddMemoryConsumer(std::string_view consumer_id,
                                               MemoryConsumerTraits traits,
                                               MemoryConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnMemoryConsumerAdded(consumer_id, traits,
                        RegisteredMemoryConsumer(consumer));
}

void MemoryConsumerRegistry::RemoveMemoryConsumer(std::string_view consumer_id,
                                                  MemoryConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnMemoryConsumerRemoved(consumer_id, RegisteredMemoryConsumer(consumer));
}

}  // namespace base
