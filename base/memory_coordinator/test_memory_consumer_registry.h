// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_
#define BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_

#include <stddef.h>

#include <cstddef>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/observer_list.h"

namespace base {

// A test class that allows registration of MemoryConsumers in unit tests. Do
// not instantiate in browser tests.
class TestMemoryConsumerRegistry : public MemoryConsumerRegistry {
 public:
  TestMemoryConsumerRegistry();
  ~TestMemoryConsumerRegistry() override;

  // MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             std::optional<MemoryConsumerTraits> traits,
                             MemoryConsumer* consumer) override;
  void OnMemoryConsumerRemoved(uint32_t consumer_id,
                               MemoryConsumer* consumer) override;

  // Invokes UpdateMemoryLimit(percentage) on all consumers.
  void NotifyUpdateMemoryLimit(int percentage);

  // Invokes DoReleaseMemory() on all consumers.
  void NotifyReleaseMemory();

  void NotifyUpdateMemoryLimitAsync(int percentage,
                                    OnceClosure on_notification_sent_callback);
  void NotifyReleaseMemoryAsync(OnceClosure on_notification_sent_callback);

  size_t size() const { return size_; }

 private:
  ObserverList<MemoryConsumer> memory_consumers_;
  size_t size_ = 0;

  WeakPtrFactory<TestMemoryConsumerRegistry> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_TEST_MEMORY_CONSUMER_REGISTRY_H_
