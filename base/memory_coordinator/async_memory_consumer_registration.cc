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
  MainThread(std::string observer_id,
             MemoryConsumerTraits traits,
             WeakPtr<AsyncMemoryConsumerRegistration> parent,
             scoped_refptr<SequencedTaskRunner> consumer_task_runner)
      : consumer_task_runner_(std::move(consumer_task_runner)),
        parent_(std::move(parent)),
        registration_(std::move(observer_id), traits, this) {}

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
  ScopedMemoryConsumerRegistration registration_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// AsyncMemoryConsumerRegistration ---------------------------------------------

AsyncMemoryConsumerRegistration::AsyncMemoryConsumerRegistration(
    scoped_refptr<SingleThreadTaskRunner> main_task_runner,
    std::string_view consumer_id,
    MemoryConsumerTraits traits,
    MemoryConsumer* consumer)
    : consumer_(consumer) {
  DCHECK(!main_task_runner->BelongsToCurrentThread());
  main_thread_.emplace(std::move(main_task_runner), std::string(consumer_id),
                       traits, weak_ptr_factory_.GetWeakPtr(),
                       SequencedTaskRunner::GetCurrentDefault());
}

AsyncMemoryConsumerRegistration::~AsyncMemoryConsumerRegistration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
