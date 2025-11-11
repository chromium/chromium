// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/async_memory_consumer_registration.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace base {

// AsyncMemoryConsumerRegistration::MainThread ---------------------------------

class AsyncMemoryConsumerRegistration::MainThread : public MemoryConsumer {
 public:
  MainThread() { DETACH_FROM_THREAD(thread_checker_); }

  void Init(std::string consumer_id,
            MemoryConsumerTraits traits,
            CheckUnregister check_unregister,
            CheckRegistryExists check_registry_exists,
            WeakPtr<AsyncMemoryConsumerRegistration> parent,
            scoped_refptr<SequencedTaskRunner> consumer_task_runner) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    consumer_task_runner_ = std::move(consumer_task_runner);
    parent_ = std::move(parent);
    registration_.emplace(consumer_id, traits, this, check_unregister,
                          check_registry_exists);
  }

 private:
  // MemoryConsumer:
  void OnUpdateMemoryLimit() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    consumer_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&AsyncMemoryConsumerRegistration::NotifyUpdateMemoryLimit,
                 parent_, memory_limit()));
  }
  void OnReleaseMemory() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    consumer_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&AsyncMemoryConsumerRegistration::NotifyReleaseMemory,
                 parent_));
  }

  // The task runner on which the off-sequence consumer lives.
  scoped_refptr<SequencedTaskRunner> consumer_task_runner_
      GUARDED_BY_CONTEXT(thread_checker_);

  // A pointer to the consumer that lives on `consumer_task_runner_`.
  WeakPtr<AsyncMemoryConsumerRegistration> parent_
      GUARDED_BY_CONTEXT(thread_checker_);

  // The registration with the MemoryConsumerRegistry.
  std::optional<MemoryConsumerRegistration> registration_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// AsyncMemoryConsumerRegistration ---------------------------------------------

AsyncMemoryConsumerRegistration::AsyncMemoryConsumerRegistration(
    std::string_view consumer_id,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer,
    CheckUnregister check_unregister,
    CheckRegistryExists check_registry_exists)
    : consumer_(consumer) {
  main_thread_task_runner_ = SingleThreadTaskRunner::GetMainThreadDefault();
  main_thread_ = std::make_unique<MainThread>();
  main_thread_task_runner_->PostTask(
      FROM_HERE, BindOnce(&MainThread::Init, Unretained(main_thread_.get()),
                          std::string(consumer_id), traits, check_unregister,
                          check_registry_exists, weak_ptr_factory_.GetWeakPtr(),
                          SequencedTaskRunner::GetCurrentDefault()));
}

AsyncMemoryConsumerRegistration::~AsyncMemoryConsumerRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (main_thread_) {
    // To ensure |main_thread_| is deleted on the correct thread, we transfer
    // ownership to a no-op task. The object is deleted with the task, even
    // if it's cancelled before it can run.
    main_thread_task_runner_->PostTask(
        FROM_HERE, BindOnce([](std::unique_ptr<MainThread> main_thread) {},
                            std::move(main_thread_)));
  }
}

void AsyncMemoryConsumerRegistration::NotifyUpdateMemoryLimit(int percentage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_->UpdateMemoryLimit(percentage);
}

void AsyncMemoryConsumerRegistration::NotifyReleaseMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_->ReleaseMemory();
}

}  // namespace base
