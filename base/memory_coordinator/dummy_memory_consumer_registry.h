// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_DUMMY_MEMORY_CONSUMER_REGISTRY_H_
#define BASE_MEMORY_COORDINATOR_DUMMY_MEMORY_CONSUMER_REGISTRY_H_

#include <string_view>

#include "base/base_export.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"

namespace base {

// A placeholder implementation of MemoryConsumerRegistry for standalone
// executables outside of //content that do not participate in memory
// coordination.
class BASE_EXPORT DummyMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  DummyMemoryConsumerRegistry();
  ~DummyMemoryConsumerRegistry() override;

  // MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             MemoryConsumerTraits traits,
                             MemoryConsumer* consumer) override;
  void OnMemoryConsumerRemoved(uint32_t consumer_id,
                               MemoryConsumer* consumer) override;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_DUMMY_MEMORY_CONSUMER_REGISTRY_H_
