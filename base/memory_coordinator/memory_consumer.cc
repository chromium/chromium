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
    std::string_view consumer_id,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists)
    : consumer_id_(consumer_id),
      consumer_(consumer),
      check_unregister_(check_unregister),
      registry_(MemoryConsumerRegistry::MaybeGet()) {
  if (!registry_) {
    CHECK_EQ(check_registry_exists, CheckRegistryExists::kDisabled)
        << ". The MemoryConsumerRegistry did not exist at the time this "
           "MemoryConsumerRegistration was created.";
    return;
  }

  registry_->AddDestructionObserver(PassKey(), this);
  registry_->AddMemoryConsumer(consumer_id, traits, consumer_);
}

MemoryConsumerRegistration::~MemoryConsumerRegistration() {
  if (registry_) {
    registry_->RemoveMemoryConsumer(consumer_id_, consumer_);
    registry_->RemoveDestructionObserver(PassKey(), this);
  }
}

void MemoryConsumerRegistration::OnBeforeMemoryConsumerRegistryDestroyed() {
  // If this function is called, this means that the registry is being destroyed
  // before the unregistration. This is only acceptable if the check is
  // disabled.
  CHECK_EQ(check_unregister_, CheckUnregister::kDisabled)
      << ". The global MemoryConsumerRegistry was destroyed before this "
         "MemoryConsumerRegistration was destroyed.";
  registry_->RemoveMemoryConsumer(consumer_id_, consumer_);
  registry_->RemoveDestructionObserver(PassKey(), this);
  registry_ = nullptr;
}

}  // namespace base
