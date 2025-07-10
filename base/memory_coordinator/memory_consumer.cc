// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer.h"

#include "base/check_op.h"
#include "base/memory_coordinator/memory_consumer_registry.h"

namespace base {

// MemoryConsumer ---------------------------------------------------

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

// ScopedMemoryConsumerRegistration ---------------------------------

ScopedMemoryConsumerRegistration::ScopedMemoryConsumerRegistration(
    std::string_view consumer_id,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer)
    : consumer_id_(consumer_id), consumer_(consumer) {
  MemoryConsumerRegistry::Get().AddMemoryConsumer(consumer_id, traits,
                                                  consumer_);
}

ScopedMemoryConsumerRegistration::~ScopedMemoryConsumerRegistration() {
  MemoryConsumerRegistry::Get().RemoveMemoryConsumer(consumer_id_, consumer_);
}

}  // namespace base
