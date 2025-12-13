// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/mock_memory_consumer.h"

namespace base {

MockMemoryConsumer::MockMemoryConsumer() = default;

MockMemoryConsumer::~MockMemoryConsumer() = default;

RegisteredMockMemoryConsumer::RegisteredMockMemoryConsumer(
    std::string_view consumer_id,
    MemoryConsumerTraits traits)
    : registration_(consumer_id, traits, this) {}

RegisteredMockMemoryConsumer::~RegisteredMockMemoryConsumer() = default;

}  // namespace base
