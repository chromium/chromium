// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_

#include <stddef.h>

#include <map>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace chromeos {
namespace file_system_provider {

// Queues arbitrary tasks. At most |max_in_parallel_| tasks will be running at
// once.
//
// The common use case is:
// 1. Call NewToken() to obtain the token used bo all other methods.
// 2. Call Enqueue() to enqueue the task.
// 3. Call Complete() when the task is completed.
//
// If the task supports aborting (it's abort callback is not NULL), then an
// enqueued task can be aborted with Abort() at any time as long as the task is
// not completed.
//
// Once a task is executed, it must be marked as completed with Complete(). If
// it's aborted before executing, no call to Complete() can happen. Simply
// saying, just call Complete() from the completion callback of the task.
class Queue {
 public:
  using AbortableCallback = base::OnceCallback<AbortCallback(void)>;

  // Creates a queue with a maximum number of tasks running in parallel.
  explicit Queue(size_t max_in_parallel);

  virtual ~Queue();

  // Creates a token for enqueuing (and later aborting) tasks.
  size_t NewToken();

  // Enqueues a task using a token generated with NewToken(). The task will be
  // executed if there is space in the internal queue, otherwise it will wait
  // until another task is finished. Once the task is finished, Complete() must
  // be called. The callback's abort callback may be NULL. In such case, Abort()
  // must not be called.
  void Enqueue(size_t token, AbortableCallback callback);

  // Forcibly aborts a previously enqueued task. May be called at any time as
  // long as the task is still in the queue and is not marked as completed.
  void Abort(size_t token);

  // Marks an executed task with |token| as completed. Must be called once the
  // task is executed. Simply saying, in most cases it should be just called
  // from the task's completion callback.
  void Complete(size_t token);

 private:
  // Information about an enqueued task which hasn't been removed, nor aborted.
  struct Task {
    Task();
    Task(size_t token, AbortableCallback callback);
    Task(Task&& other);
    ~Task();

    Task& operator=(Task&& other);

    size_t token;
    AbortableCallback callback;
    AbortCallback abort_callback;
  };

  // Runs the next task from the pending queue if there is less than
  // |max_in_parallel_| tasks running at once.
  void MaybeRun();

  const size_t max_in_parallel_;
  size_t next_token_;
  base::circular_deque<Task> pending_;
  std::map<int, Task> executed_;

  base::WeakPtrFactory<Queue> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Queue);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_
