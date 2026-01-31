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
    std::string_view consumer_id,
    MemoryConsumerTraits traits,
    RegisteredMemoryConsumer consumer) {
  CHECK(!std::ranges::contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);
}

void TestMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    std::string_view consumer_id,
    RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

void TestMemoryConsumerRegistry::NotifyUpdateMemoryLimit(int percentage) {
  for (RegisteredMemoryConsumer consumer : memory_consumers_) {
    consumer.UpdateMemoryLimit(percentage);
  }
}

void TestMemoryConsumerRegistry::NotifyReleaseMemory() {
  for (RegisteredMemoryConsumer consumer : memory_consumers_) {
    consumer.ReleaseMemory();
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
