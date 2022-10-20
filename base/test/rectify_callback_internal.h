// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_
#define BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"

namespace base::internal {

// RectifyCallbackBinder performs either BindOnce or BindRepeating depending on
// callback type.

template <template <typename...> class CallbackType>
struct RectifyCallbackBinder;

template <>
struct RectifyCallbackBinder<RepeatingCallback> {
  template <typename... Args>
  static auto Bind(Args&&... args) {
    return BindRepeating(std::forward<Args>(args)...);
  }
};

template <>
struct RectifyCallbackBinder<OnceCallback> {
  template <typename... Args>
  static auto Bind(Args&&... args) {
    return BindOnce(std::forward<Args>(args)...);
  }
};

// RectifyCallbackWrapper binds a wrapper around the actual callback that
// discards the unused parameters and then calls the callback with the remaining
// arguments.

template <template <typename...> class CallbackType,
          typename PartialSignature,
          typename IgnoreSignature = void()>
struct RectifyCallbackWrapper;

template <template <typename...> class CallbackType,
          typename R,
          typename... PartialArgs,
          typename IgnoredArg,  // Required to differentiate from empty case.
          typename... IgnoredArgs>
struct RectifyCallbackWrapper<CallbackType,
                              R(PartialArgs...),
                              void(IgnoredArg, IgnoredArgs...)> {
  static CallbackType<R(IgnoredArg, IgnoredArgs..., PartialArgs...)> Rectify(
      CallbackType<R(PartialArgs...)> callback) {
    return RectifyCallbackBinder<CallbackType>::Bind(
        [](CallbackType<R(PartialArgs...)> callback, IgnoredArg, IgnoredArgs...,
           PartialArgs... args) {
          return std::move(callback).Run(std::forward<PartialArgs>(args)...);
        },
        std::move(callback));
  }
};

// Specialization that handles cases where no parameter reduction is required;
// this just returns the input.
template <template <typename...> class CallbackType, typename FullSignature>
struct RectifyCallbackWrapper<CallbackType, FullSignature, void()> {
  static CallbackType<FullSignature> Rectify(
      CallbackType<FullSignature> callback) {
    return callback;
  }
};

// RectifyCallbackSplitter figures out which arguments from the full signature
// need to be ignored, then delegates to RectifyCallbackWrapper to provide the
// conversion from actual to desired type.

template <template <typename...> class CallbackType,
          typename FullSignature,
          typename PartialSignature,
          typename IgnoreSignature = void()>
struct RectifyCallbackSplitter;

// Specialization that handles the case where all arguments are stripped away.
template <template <typename...> class CallbackType,
          typename R,
          typename... IgnoredArgs>
struct RectifyCallbackSplitter<CallbackType, R(), R(), void(IgnoredArgs...)>
    : RectifyCallbackWrapper<CallbackType, R(), void(IgnoredArgs...)> {};

// Specialization that handles the case where some number of arguments remain,
// and some additional arguments may need to be stripped away. Recursive until
// all arguments are stripped, then delegates to RectifyCallbackWrapper.
template <template <typename...> class CallbackType,
          typename R,
          typename First,
          typename... Rest,
          typename... PartialArgs,
          typename... IgnoredArgs>
struct RectifyCallbackSplitter<CallbackType,
                               R(First, Rest...),
                               R(PartialArgs...),
                               void(IgnoredArgs...)>
    : std::conditional_t<sizeof...(Rest) >= sizeof...(PartialArgs),
                         RectifyCallbackSplitter<CallbackType,
                                                 R(Rest...),
                                                 R(PartialArgs...),
                                                 void(IgnoredArgs..., First)>,
                         RectifyCallbackWrapper<CallbackType,
                                                R(PartialArgs...),
                                                void(IgnoredArgs...)>> {};

// RectifyCallbackImpl is the entry point which deduces the appropriate return
// type and then either provides Rectify() directly (for trivial callbacks) or
// delegates to RectifyCallbackSplitter to figure out which args should be
// eliminated and which passed on to the actual callback.

template <typename DesiredType, typename ActualType>
struct RectifyCallbackImpl;

// Specialization that handles an explicit callback type as the desired type.
// The input will be converted to the expected callback type if necessary
// (typically from RepeatingCallback to OnceCallback) and then reduced as
// appropriate.
template <template <typename...> class DesiredCallbackType,
          typename DesiredSignature,
          template <typename...>
          class ActualCallbackType,
          typename ActualSignature>
struct RectifyCallbackImpl<DesiredCallbackType<DesiredSignature>,
                           ActualCallbackType<ActualSignature>>
    : RectifyCallbackSplitter<DesiredCallbackType,
                              DesiredSignature,
                              ActualSignature> {};

// Specialization that handles a type signature as the desired type. The output
// in this case will be based on the type of callback passed in.
template <template <typename...> class ActualCallbackType,
          typename ActualSignature,
          typename R,
          typename... Args>
struct RectifyCallbackImpl<R(Args...), ActualCallbackType<ActualSignature>>
    : RectifyCallbackSplitter<ActualCallbackType, R(Args...), ActualSignature> {
};

// Fallback for things like DoNothing(), NullCallback(), etc. where a specific
// callback return type is provided. Delegates directly to the trivial
// implementation of RectifyCallbackWrapper.
template <template <typename...> class DesiredCallbackType,
          typename DesiredSignature,
          typename T>
struct RectifyCallbackImpl<DesiredCallbackType<DesiredSignature>, T>
    : RectifyCallbackWrapper<DesiredCallbackType, DesiredSignature> {};

// Fallback for things like DoNothing(), NullCallback(), etc. where only the
// signature of the return type is provided. Delegates directly to the trivial
// implementation of RectifyCallbackWrapper, and returns a RepeatingCallback as
// that can be used more flexibly.
template <typename R, typename... Args, typename T>
struct RectifyCallbackImpl<R(Args...), T>
    : RectifyCallbackWrapper<RepeatingCallback, R(Args...)> {};

}  // namespace base::internal

#endif  // BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_
