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
bool MemoryConsumerRegistry::Exists() {
  return g_memory_consumer_registry;
}

// static
MemoryConsumerRegistry& MemoryConsumerRegistry::Get() {
  CHECK(g_memory_consumer_registry);
  return *g_memory_consumer_registry;
}

// static
MemoryConsumerRegistry* MemoryConsumerRegistry::MaybeGet() {
  return g_memory_consumer_registry;
}

// static
void MemoryConsumerRegistry::Set(MemoryConsumerRegistry* instance) {
  CHECK_NE(bool(g_memory_consumer_registry), bool(instance));
  g_memory_consumer_registry = instance;
}

MemoryConsumerRegistry::MemoryConsumerRegistry() = default;

MemoryConsumerRegistry::~MemoryConsumerRegistry() {
  // Checks that implementations of this class correctly call
  // NotifyDestruction().
  CHECK(destruction_observers_notified_);

  // Checks that implementations of the destruction observer interface correctly
  // unregister themselves.
  CHECK(destruction_observers_.empty());
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

void MemoryConsumerRegistry::AddDestructionObserver(
    PassKey<MemoryConsumerRegistration>,
    MemoryConsumerRegistryDestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void MemoryConsumerRegistry::RemoveDestructionObserver(
    PassKey<MemoryConsumerRegistration>,
    MemoryConsumerRegistryDestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

void MemoryConsumerRegistry::NotifyDestruction() {
  destruction_observers_.Notify(&MemoryConsumerRegistryDestructionObserver::
                                    OnBeforeMemoryConsumerRegistryDestroyed);
  destruction_observers_notified_ = true;
}

}  // namespace base
