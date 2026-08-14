// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "build/build_config.h"

namespace base {

// MemoryConsumer ---------------------------------------------------

MemoryConsumer::MemoryConsumer() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

bool MemoryConsumer::IsPassive() const {
  return false;
}

bool PassiveMemoryConsumer::IsPassive() const {
  return true;
}

void MemoryConsumer::ReleaseMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnReleaseMemory();
}

void MemoryConsumer::UpdateMemoryLimit(MemoryLimit memory_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateMemoryLimitNoNotification(memory_limit);
  OnUpdateMemoryLimit();
}

void MemoryConsumer::UpdateMemoryLimitNoNotification(MemoryLimit memory_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  memory_limit_ = memory_limit;
}

// MemoryConsumerRegistration ---------------------------------------

MemoryConsumerRegistration::MemoryConsumerRegistration(
    std::string_view consumer_name,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer,
    CheckUnregister check_unregister)
    : consumer_name_(consumer_name),
      consumer_(consumer),
      check_unregister_(check_unregister),
      registry_(MemoryConsumerRegistry::MaybeGet()) {
  if (!registry_) {
#if !BUILDFLAG(IS_IOS)
    // Enforce that the registry exists outside of tests to prevent components
    // from silently failing to respond to memory pressure.
    CHECK_IS_TEST()
        << ". The MemoryConsumerRegistry did not exist at the time the "
           "MemoryConsumerRegistration for "
        << consumer_name << " was created.";
#endif
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

ByteSize ScaleByMemoryLimit(ByteSize baseline, MemoryLimit memory_limit) {
  return memory_limit.Scale(baseline);
}

}  // namespace base
