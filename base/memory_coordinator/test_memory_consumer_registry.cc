// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/test_memory_consumer_registry.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/task/single_thread_task_runner.h"

namespace base {

TestMemoryConsumerRegistry::TestMemoryConsumerRegistry() {
  MemoryConsumerRegistry::Set(this);
}

TestMemoryConsumerRegistry::~TestMemoryConsumerRegistry() {
  NotifyDestruction();
  MemoryConsumerRegistry::Set(nullptr);

  CHECK(memory_consumers_.empty());
}

void TestMemoryConsumerRegistry::OnMemoryConsumerAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    std::optional<MemoryConsumerTraits> traits,
    MemoryConsumer* consumer) {
  CHECK(!memory_consumers_.HasObserver(consumer));
  memory_consumers_.AddObserver(consumer);
  size_++;
}

void TestMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    uint32_t consumer_id,
    MemoryConsumer* consumer) {
  CHECK(memory_consumers_.HasObserver(consumer));
  memory_consumers_.RemoveObserver(consumer);
  size_--;
}

void TestMemoryConsumerRegistry::NotifyUpdateMemoryLimit(int percentage) {
  for (MemoryConsumer& consumer : memory_consumers_) {
    MemoryConsumerRegistry::NotifyUpdateMemoryLimit(&consumer, percentage);
  }
}

void TestMemoryConsumerRegistry::NotifyReleaseMemory() {
  for (MemoryConsumer& consumer : memory_consumers_) {
    MemoryConsumerRegistry::NotifyReleaseMemory(&consumer);
  }
}

void TestMemoryConsumerRegistry::NotifyUpdateMemoryLimitAsync(
    int percentage,
    OnceClosure on_notification_sent_callback) {
  SingleThreadTaskRunner::GetMainThreadDefault()->PostTaskAndReply(
      FROM_HERE,
      BindOnce(&TestMemoryConsumerRegistry::NotifyUpdateMemoryLimit,
               weak_ptr_factory_.GetWeakPtr(), percentage),
      std::move(on_notification_sent_callback));
}

void TestMemoryConsumerRegistry::NotifyReleaseMemoryAsync(
    OnceClosure on_notification_sent_callback) {
  SingleThreadTaskRunner::GetMainThreadDefault()->PostTaskAndReply(
      FROM_HERE,
      BindOnce(&TestMemoryConsumerRegistry::NotifyReleaseMemory,
               weak_ptr_factory_.GetWeakPtr()),
      std::move(on_notification_sent_callback));
}

}  // namespace base
