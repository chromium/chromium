// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUNCTIONAL_IS_CALLBACK_H_
#define BASE_FUNCTIONAL_IS_CALLBACK_H_

#include <type_traits>

#include "base/functional/callback.h"

// TODO(thestig): Do IWYU for code that needs callback_helpers.h, but
// indirectly depends on this to provide it. Then remove this.
#include "base/functional/callback_helpers.h"

namespace base {

namespace internal {

template <typename T>
struct IsBaseCallbackImpl : std::false_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<OnceCallback<R(Args...)>> : std::true_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<RepeatingCallback<R(Args...)>> : std::true_type {};

}  // namespace internal

// IsBaseCallback<T> is satisfied if and only if T is an instantiation of
// base::OnceCallback<Signature> or base::RepeatingCallback<Signature>.
template <typename T>
concept IsBaseCallback = internal::IsBaseCallbackImpl<std::decay_t<T>>::value;

}  // namespace base

#endif  // BASE_FUNCTIONAL_IS_CALLBACK_H_
