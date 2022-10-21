// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_
#define BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace base::internal {

// RectifyCallbackWrapper binds a wrapper around the actual callback that
// discards the unused parameters and then calls the callback with the remaining
// arguments.

template <template <typename> typename CallbackType,
          typename PartialSignature,
          typename IgnoreSignature>
struct RectifyCallbackWrapper;

template <template <typename> typename CallbackType,
          typename R,
          typename... PartialArgs,
          typename... IgnoredArgs>
struct RectifyCallbackWrapper<CallbackType,
                              R(PartialArgs...),
                              void(IgnoredArgs...)> {
  static CallbackType<R(IgnoredArgs..., PartialArgs...)> Rectify(
      CallbackType<R(PartialArgs...)> callback) {
    if constexpr (IsOnceCallback<CallbackType<void()>>::value) {
      return BindOnce(
          [](OnceCallback<R(PartialArgs...)> callback, IgnoredArgs...,
             PartialArgs... args) {
            return std::move(callback).Run(std::forward<PartialArgs>(args)...);
          },
          std::move(callback));
    } else {
      return BindRepeating(
          [](const RepeatingCallback<R(PartialArgs...)> callback,
             IgnoredArgs..., PartialArgs... args) {
            return callback.Run(std::forward<PartialArgs>(args)...);
          },
          std::move(callback));
    }
  }
};

// RectifyCallbackSplitter figures out which arguments from the full signature
// need to be ignored, then delegates to RectifyCallbackWrapper to provide the
// conversion from actual to desired type.

template <template <typename> typename CallbackType,
          typename FullSignature,
          typename PartialSignature,
          typename IgnoreSignature = void()>
struct RectifyCallbackSplitter;

// Specialization that handles the case where all arguments are stripped away.
template <template <typename> typename CallbackType,
          typename R,
          typename... IgnoredArgs>
struct RectifyCallbackSplitter<CallbackType, R(), R(), void(IgnoredArgs...)>
    : RectifyCallbackWrapper<CallbackType, R(), void(IgnoredArgs...)> {};

// Specialization that handles the case where some number of arguments remain,
// and some additional arguments may need to be stripped away. Recursive until
// all arguments are stripped, then delegates to RectifyCallbackWrapper.
template <template <typename> typename CallbackType,
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

template <typename DesiredType, typename ActualType, typename SFINAE = void>
struct RectifyCallbackImpl;

// Specialization that handles an explicit callback type as the desired type.
// The is the main specialization and does the heavy lifting; most of the other
// specializations adapt their arguments into callback types before delegating
// to this specialization. The input will be converted to the expected callback
// type if necessary (typically from RepeatingCallback to OnceCallback) and then
// reduced as appropriate.
template <template <typename> typename DesiredCallbackType,
          typename DesiredSignature,
          template <typename>
          typename ActualCallbackType,
          typename ActualSignature>
struct RectifyCallbackImpl<DesiredCallbackType<DesiredSignature>,
                           ActualCallbackType<ActualSignature>>
    : RectifyCallbackSplitter<DesiredCallbackType,
                              DesiredSignature,
                              ActualSignature> {};

// Specialization that handles the case when the desired callback type and the
// actual callback type already have a matching signature.
template <template <typename> typename DesiredCallbackType,
          template <typename>
          typename ActualCallbackType,
          typename Signature>
struct RectifyCallbackImpl<DesiredCallbackType<Signature>,
                           ActualCallbackType<Signature>> {
  static DesiredCallbackType<Signature> Rectify(
      ActualCallbackType<Signature> callback) {
    return callback;
  }
};

// Specialization that handles a type signature as the desired type. The output
// in this case will be based on the type of callback passed in.
template <typename R,
          typename... Args,
          template <typename>
          typename ActualCallbackType,
          typename ActualSignature>
struct RectifyCallbackImpl<R(Args...), ActualCallbackType<ActualSignature>>
    : RectifyCallbackImpl<ActualCallbackType<R(Args...)>,
                          ActualCallbackType<ActualSignature>> {};

// Fallback for things like DoNothing(), NullCallback(), etc. where a specific
// callback return type is provided.
template <template <typename> typename DesiredCallbackType,
          typename R,
          typename... Args,
          typename T>
struct RectifyCallbackImpl<DesiredCallbackType<R(Args...)>,
                           T,
                           std::enable_if_t<!IsBaseCallback<T>::value>>
    : RectifyCallbackImpl<DesiredCallbackType<R(Args...)>,
                          DesiredCallbackType<R(Args...)>> {};

// Fallback for things like DoNothing(), NullCallback(), etc. where only the
// signature of the return type is provided. In this case, `RepeatingCallback`
// is implicitly generated, as it can be used as both a `OnceCallback` or
// `RepeatingCallback`.
template <typename R, typename... Args, typename T>
struct RectifyCallbackImpl<R(Args...), T>
    : RectifyCallbackImpl<RepeatingCallback<R(Args...)>,
                          RepeatingCallback<R(Args...)>> {};

}  // namespace base::internal

#endif  // BASE_TEST_RECTIFY_CALLBACK_INTERNAL_H_
