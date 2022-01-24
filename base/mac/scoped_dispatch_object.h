// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_DISPATCH_OBJECT_H_
#define BASE_MAC_SCOPED_DISPATCH_OBJECT_H_

#include <dispatch/dispatch.h>

#include "base/mac/scoped_typeref.h"

namespace base {

namespace internal {

template <typename T>
struct ScopedDispatchObjectTraits {
  static constexpr T InvalidValue() { return nullptr; }
  static T Retain(T object) {
    dispatch_retain(object);
    return object;
  }
  static void Release(T object) {
    dispatch_release(object);
  }
};

}  // namespace internal

template <typename T>
using ScopedDispatchObject =
    ScopedTypeRef<T, internal::ScopedDispatchObjectTraits<T>>;

}  // namespace base

#endif  // BASE_MAC_SCOPED_DISPATCH_OBJECT_H_
