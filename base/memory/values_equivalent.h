// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_VALUES_EQUIVALENT_H_
#define BASE_MEMORY_VALUES_EQUIVALENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {

// Compares two pointers for equality, returns the dereferenced value comparison
// if both are non-null.
// Behaves like std::optional<T>::operator==(const std::optional<T>&) but for
// pointers.
template <typename T>
bool ValuesEquivalent(const T* a, const T* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return *a == *b;
}

// Specialize for smart pointers like std::unique_ptr and base::scoped_refptr
// that provide a T* get() method.
// Example usage:
//   struct Example {
//     std::unique_ptr<Child> child;
//     bool operator==(const Example& other) const {
//       return base::ValuesEquivalent(child, other.child);
//     }
//   };
template <typename T,
          std::enable_if_t<
              std::is_pointer_v<decltype(std::declval<T>().get())>>* = nullptr>
bool ValuesEquivalent(const T& x, const T& y) {
  return ValuesEquivalent(x.get(), y.get());
}

// Specialize for smart pointers like blink::Persistent and blink::Member that
// provide a T* Get() method.
// Example usage:
//   namespace blink {
//   struct Example : public GarbageCollected<Example> {
//     Member<Child> child;
//     bool operator==(const Example& other) const {
//       return base::ValuesEquivalent(child, other.child);
//     }
//     void Trace(Visitor*) const;
//   };
//   }  // namespace blink
template <typename T,
          std::enable_if_t<
              std::is_pointer_v<decltype(std::declval<T>().Get())>>* = nullptr>
bool ValuesEquivalent(const T& x, const T& y) {
  return ValuesEquivalent(x.Get(), y.Get());
}

}  // namespace base

#endif  // BASE_MEMORY_VALUES_EQUIVALENT_H_
