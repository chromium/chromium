// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
#define BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_

#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

// It is dangerous to post a task with a T* argument where T is a subtype of
// RefCounted(Base|ThreadSafeBase), since by the time the parameter is used, the
// object may already have been deleted since it was not held with a
// scoped_refptr. Example: http://crbug.com/27191
// The following set of traits are designed to generate a compile error
// whenever this antipattern is attempted.

namespace base::internal {

template <typename T>
concept IsRefCountedType = requires(T& x) {
  // There are no additional constraints on `AddRef()` and `Release()` since
  // `scoped_refptr`, for better or worse`, seamlessly interoperates with other
  // non-base types that happen to implement the same signatures (e.g. COM's
  // IUnknown).
  x.AddRef();
  x.Release();
};

// Human readable translation: you needed to be a scoped_refptr if you are a raw
// pointer type and are convertible to a RefCounted(Base|ThreadSafeBase) type.
template <typename T>
struct NeedsScopedRefptrButGetsRawPtr {
  static_assert(!std::is_reference_v<T>,
                "NeedsScopedRefptrButGetsRawPtr requires non-reference type.");

  // TODO(danakj): Should ban native references and
  // std::reference_wrapper here too.
  static constexpr bool value =
      (base::IsRawRef<T>::value && IsRefCountedType<base::RemoveRawRefT<T>>) ||
      (base::IsRawPointer<T> && IsRefCountedType<base::RemoveRawPointerT<T>>);
};

}  // namespace base::internal

#endif  // BASE_MEMORY_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
