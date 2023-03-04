// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_

#include <functional>
#include <memory>
#include <type_traits>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

// TODO(tzik): Merge this file to base/task/bind_post_task.h.

namespace sync_file_system {
namespace drive_backend {

namespace internal {

template <typename Signature, typename... Args>
base::OnceClosure MakeClosure(base::RepeatingCallback<Signature>* callback,
                              Args&&... args) {
  return base::BindOnce(*callback, std::forward<Args>(args)...);
}

template <typename Signature, typename... Args>
base::OnceClosure MakeClosure(base::OnceCallback<Signature>* callback,
                              Args&&... args) {
  return base::BindOnce(std::move(*callback), std::forward<Args>(args)...);
}

template <typename CallbackType>
class CallbackHolder {
 public:
  CallbackHolder(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::Location& from_here,
                 CallbackType callback)
      : task_runner_(task_runner),
        from_here_(from_here),
        callback_(std::move(callback)) {
    DCHECK(task_runner_.get());
  }

  CallbackHolder(const CallbackHolder&) = delete;
  CallbackHolder& operator=(const CallbackHolder&) = delete;

  ~CallbackHolder() {
    if (callback_) {
      task_runner_->PostTask(from_here_,
                             base::BindOnce(&CallbackHolder::DeleteCallback,
                                            std::move(callback_)));
    }
  }

  template <typename... Args>
  void Run(Args... args) {
    task_runner_->PostTask(
        from_here_, MakeClosure(&callback_, std::forward<Args>(args)...));
  }

 private:
  static void DeleteCallback(CallbackType callback) {}

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Location from_here_;
  CallbackType callback_;
};

}  // namespace internal

template <typename... Args>
base::OnceCallback<void(Args...)> RelayCallbackToTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Location& from_here,
    base::OnceCallback<void(Args...)> callback) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());

  if (callback.is_null())
    return {};

  using CallbackType = base::OnceCallback<void(Args...)>;
  using HelperType = internal::CallbackHolder<CallbackType>;
  using RunnerType = void (HelperType::*)(Args...);
  RunnerType run = &HelperType::Run;
  return base::BindOnce(run, std::make_unique<HelperType>(
                                 task_runner, from_here, std::move(callback)));
}

template <typename... Args>
base::RepeatingCallback<void(Args...)> RelayCallbackToTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Location& from_here,
    base::RepeatingCallback<void(Args...)> callback) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());

  if (callback.is_null())
    return {};

  using CallbackType = base::RepeatingCallback<void(Args...)>;
  using HelperType = internal::CallbackHolder<CallbackType>;
  using RunnerType = void (HelperType::*)(Args...);
  RunnerType run = &HelperType::Run;
  return base::BindRepeating(
      run, std::make_unique<HelperType>(task_runner, from_here,
                                        std::move(callback)));
}

template <typename CallbackType>
CallbackType RelayCallbackToCurrentThread(const base::Location& from_here,
                                          CallbackType callback) {
  return RelayCallbackToTaskRunner(
      base::SingleThreadTaskRunner::GetCurrentDefault(), from_here,
      std::move(callback));
}

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_
