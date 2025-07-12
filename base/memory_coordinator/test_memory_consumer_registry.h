// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_
#define BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_

#include <stddef.h>

#include <vector>

#include "base/memory_coordinator/memory_consumer_registry.h"

namespace base {

// A test class that allows registration of MemoryConsumers in unit tests. Do
// not instantiate in browser tests.
class TestMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  TestMemoryConsumerRegistry();
  ~TestMemoryConsumerRegistry() override;

  // MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             MemoryConsumerTraits traits,
                             RegisteredMemoryConsumer consumer) override;
  void OnMemoryConsumerRemoved(std::string_view consumer_id,
                               RegisteredMemoryConsumer consumer) override;

  // Invokes UpdateMemoryLimit(percentage) on all consumers.
  void NotifyUpdateMemoryLimit(int percentage);

  // Invokes DoReleaseMemory() on all consumers.
  void NotifyReleaseMemory();

  size_t size() const { return memory_consumers_.size(); }

 private:
  std::vector<RegisteredMemoryConsumer> memory_consumers_;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_
