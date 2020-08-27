// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_
#define BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_

#include <tuple>

#include "base/compiler_specific.h"

namespace base {

namespace internal {

// Helpers to simplify sharing templates between non-const and const methods.
// Normally, matching against a method pointer type requires defining both a
// `R (T::*)(Args...)` and a `R (T::*)(Args...) const` overload of the template
// function. Rather than doing that, these helpers allow extraction of `R` and
// `Args...` from a method pointer type deduced as `MethodPointerType`.

template <typename MethodPtrType>
struct MethodTraits;

template <typename R, typename T, typename... Args>
struct MethodTraits<R (T::*)(Args...)> {
  using ReturnType = R;
  using ArgsTuple = std::tuple<Args...>;
};

template <typename R, typename T, typename... Args>
struct MethodTraits<R (T::*)(Args...) const> {
  using ReturnType = R;
  using ArgsTuple = std::tuple<Args...>;
};

template <typename MethodPtrType>
using ExtractMethodReturnType =
    typename MethodTraits<MethodPtrType>::ReturnType;

template <typename MethodPtrType>
using ExtractMethodArgsTuple = typename MethodTraits<MethodPtrType>::ArgsTuple;

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_SEQUENCE_BOUND_INTERNAL_H_
