// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_
#define BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_

#include <memory>
#include <string_view>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"

namespace base {

class SingleThreadTaskRunner;

// Used to register a MemoryConsumer that does not live on the main thread,
// which means that the consumer will receive its notifications asynchronously.
class BASE_EXPORT AsyncMemoryConsumerRegistration {
 public:
  using CheckUnregister = MemoryConsumerRegistration::CheckUnregister;
  using CheckRegistryExists = MemoryConsumerRegistration::CheckRegistryExists;

  AsyncMemoryConsumerRegistration(
      std::string_view consumer_id,
      MemoryConsumerTraits traits,
      MemoryConsumer* consumer,
      CheckUnregister check_unregister = CheckUnregister::kEnabled,
      CheckRegistryExists check_registry_exists =
          CheckRegistryExists::kEnabled);
  ~AsyncMemoryConsumerRegistration();

 private:
  class MainThread;

  void NotifyUpdateMemoryLimit(int percentage);
  void NotifyReleaseMemory();

  // A pointer to the actual consumer. Must outlive `this`.
  raw_ptr<MemoryConsumer> consumer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Handle to the main thread's task runner. This is cached because it might no
  // longer be registered at the time this instance is destroyed.
  scoped_refptr<SingleThreadTaskRunner> main_thread_task_runner_;

  // Parts of this class that lives on the main thread.
  std::unique_ptr<MainThread> main_thread_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  WeakPtrFactory<AsyncMemoryConsumerRegistration> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_
