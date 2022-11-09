// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_
#define BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace base::internal {

struct DefaultCrossThreadBindTraits {
  template <typename Signature>
  using CrossThreadTask = OnceCallback<Signature>;

  template <typename Functor, typename... Args>
  static inline auto BindOnce(Functor&& functor, Args&&... args) {
    return base::BindOnce(std::forward<Functor>(functor),
                          std::forward<Args>(args)...);
  }

  template <typename T>
  static inline auto Unretained(T ptr) {
    return base::Unretained(ptr);
  }

  static inline bool PostTask(SequencedTaskRunner& task_runner,
                              const Location& location,
                              OnceClosure&& task) {
    return task_runner.PostTask(location, std::move(task));
  }

  static inline bool PostTaskAndReply(SequencedTaskRunner& task_runner,
                                      const Location& location,
                                      OnceClosure&& task,
                                      OnceClosure&& reply) {
    return task_runner.PostTaskAndReply(location, std::move(task),
                                        std::move(reply));
  }

  template <typename TaskReturnType, typename ReplyArgType>
  static inline bool PostTaskAndReplyWithResult(
      SequencedTaskRunner& task_runner,
      const Location& location,
      OnceCallback<TaskReturnType()>&& task,
      OnceCallback<void(ReplyArgType)>&& reply) {
    return task_runner.PostTaskAndReplyWithResult(location, std::move(task),
                                                  std::move(reply));
  }

  // Accept RepeatingCallback here since it's convertible to a OnceCallback.
  template <template <typename> class CallbackType>
  using EnableIfIsCrossThreadTask = EnableIfIsBaseCallback<CallbackType>;
};

}  // namespace base::internal

#endif  // BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_
