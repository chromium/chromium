// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_
#define BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_

#include <string_view>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"

namespace base {

class MemoryConsumer;
class SingleThreadTaskRunner;

// Used to register a MemoryConsumer that does not live on the main thread,
// which means that the consumer will receive its notifications asynchronously.
class BASE_EXPORT AsyncMemoryConsumerRegistration {
 public:
  AsyncMemoryConsumerRegistration(
      scoped_refptr<SingleThreadTaskRunner> main_thread_task_runner,
      std::string_view consumer_id,
      MemoryConsumerTraits traits,
      MemoryConsumer* consumer);
  ~AsyncMemoryConsumerRegistration();

 private:
  class MainThread;

  void NotifyUpdateMemoryLimit(int percentage);
  void NotifyReleaseMemory();

  // A pointer to the actual consumer. Must outlive `this`.
  raw_ptr<MemoryConsumer> consumer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Parts of this class that lives on the main thread.
  SequenceBound<MainThread> main_thread_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  WeakPtrFactory<AsyncMemoryConsumerRegistration> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_ASYNC_MEMORY_CONSUMER_REGISTRATION_H_
