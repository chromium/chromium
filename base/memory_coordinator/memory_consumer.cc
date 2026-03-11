// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/check_op.h"
#include "base/memory_coordinator/memory_consumer_registry.h"

namespace base {

// MemoryConsumer ---------------------------------------------------

MemoryConsumer::MemoryConsumer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void MemoryConsumer::ReleaseMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnReleaseMemory();
}

void MemoryConsumer::UpdateMemoryLimit(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The percentage can never be negative (but it can be higher than 100).
  CHECK_GE(percentage, 0);
  memory_limit_ = percentage;
  OnUpdateMemoryLimit();
}

// MemoryConsumerRegistration ---------------------------------------

MemoryConsumerRegistration::MemoryConsumerRegistration(
    std::string_view consumer_name,
    std::optional<MemoryConsumerTraits> traits,
    MemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists)
    : consumer_name_(consumer_name),
      consumer_(consumer),
      check_unregister_(check_unregister),
      registry_(MemoryConsumerRegistry::MaybeGet()) {
  if (!registry_) {
    CHECK_EQ(check_registry_exists, CheckRegistryExists::kDisabled)
        << ". The MemoryConsumerRegistry did not exist at the time the "
           "MemoryConsumerRegistration for "
        << consumer_name << " was created.";
    return;
  }

  registry_->AddDestructionObserver(PassKey(), this);
  registry_->AddMemoryConsumer(consumer_name, traits, consumer_);
}

MemoryConsumerRegistration::~MemoryConsumerRegistration() {
  if (registry_) {
    registry_->RemoveMemoryConsumer(consumer_name_, consumer_);
    registry_->RemoveDestructionObserver(PassKey(), this);
  }
}

void MemoryConsumerRegistration::SetAsyncHandleDestroyedFlag(
    const std::atomic<bool>* async_handle_destroyed_flag,
    base::PassKey<AsyncMemoryConsumerRegistration> pass_key) {
  CHECK(!async_handle_destroyed_flag_);
  async_handle_destroyed_flag_ = async_handle_destroyed_flag;
}

void MemoryConsumerRegistration::OnBeforeMemoryConsumerRegistryDestroyed() {
  // If this function is called, this means that the registry is being destroyed
  // before the unregistration. This is only acceptable if the check is
  // disabled or if it's an asynchronous registration whose handle has already
  // been destroyed.
  if (check_unregister_ == CheckUnregister::kEnabled) {
    if (async_handle_destroyed_flag_) {
      // Asynchronous registration case.
      const bool handle_destroyed =
          async_handle_destroyed_flag_->load(std::memory_order_acquire);
      CHECK(handle_destroyed)
          << ". The AsyncMemoryConsumerRegistration handle for "
          << consumer_name_
          << " must be destroyed before the global MemoryConsumerRegistry.";
    } else {
      // Synchronous registration case.
      CHECK(false)
          << ". The MemoryConsumerRegistration " << consumer_name_
          << " must be destroyed before the global MemoryConsumerRegistry.";
    }
  }

  registry_->RemoveMemoryConsumer(consumer_name_, consumer_);
  registry_->RemoveDestructionObserver(PassKey(), this);
  registry_ = nullptr;
}

}  // namespace base
