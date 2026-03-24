// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_POST_TASK_AND_REPLY_WITH_RESULT_INTERNAL_H_
#define BASE_TASK_POST_TASK_AND_REPLY_WITH_RESULT_INTERNAL_H_

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"

namespace base {

namespace internal {

// Converts a type T to a tuple by wrapping in tuple<T>, unless it's already
// a tuple.
template <typename T>
struct ensure_tuple {
  using type = std::tuple<T>;
};

template <typename... Args>
struct ensure_tuple<std::tuple<Args...>> {
  using type = std::tuple<Args...>;
};

// Adapts a function that produces a result via a return value to one that
// returns via an output parameter.
template <typename ReplyStorageType, typename TaskReturnType>
void ReturnAsParamAdapter(OnceCallback<TaskReturnType()> func,
                          std::unique_ptr<ReplyStorageType>* result) {
  *result = std::make_unique<ReplyStorageType>(std::move(func).Run());
}

// Adapts a std::tuple result to a callback that expects unwrapped argument(s).
template <typename TaskReturnTuple, typename... ReplyArgTypes>
void ReplyAdapter(OnceCallback<void(ReplyArgTypes...)> callback,
                  std::unique_ptr<TaskReturnTuple>* result) {
  DCHECK(result->get());
  std::apply(
      [&callback](auto&&... args) {
        std::move(callback).Run(std::forward<decltype(args)>(args)...);
      },
      std::move(**result));
}

}  // namespace internal

}  // namespace base

#endif  // BASE_TASK_POST_TASK_AND_REPLY_WITH_RESULT_INTERNAL_H_
