// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SAFE_REF_H_
#define BASE_MEMORY_SAFE_REF_H_

#include "base/check.h"
#include "base/memory/weak_ptr.h"

#include <utility>

namespace base {

// SafeRef smart pointers are used to represent a non-owning pointer to an
// object, where the pointer is always intended to be valid. These are useful in
// the same cases that a raw pointer `T*` (or a `T&`) would traditionally be
// used, as the owner of the SafeRef knows the lifetime of the pointed-to object
// from other means and will not use the pointer after the pointed-to object is
// destroyed. However, unlike a `T*` or `T&`, a logic bug will manifest as a
// benign crash instead of as a Use-after-Free.
//
// SafeRef pointers can not be null (as expressed by the "Ref" suffix instead of
// "Ptr"). A SafeRef can be wrapped in an absl::optional if it should not always
// point to something valid. (A SafePtr sibling type can be introduced if this
// is problematic, or if consuming moves are needed!)
//
// If code wants to track the lifetime of the object directly through its
// pointer, and dynamically handle the case of the pointer outliving the object
// it points to, then base::WeakPtr should be used instead.
//
// The SafeRef pointer is constructed from a base::WeakPtrFactory's GetSafeRef()
// method. Since it is tied to the base::WeakPtrFactory, it will consider its
// pointee invalid when the base::WeakPtrFactory is invalidated, in the same way
// as base::WeakPtr does, including after a call to InvalidateWeakPtrs().
//
// THREAD SAFETY: SafeRef pointers (like base::WeakPtr) may only be used on the
// sequence (or thread) where the associated base::WeakPtrFactory will be
// invalidated and/or destroyed. They are safe to passively hold or to destroy
// on any thread though.
//
// This class is expected to one day be replaced by a more flexible and safe
// smart pointer abstraction which is not tied to base::WeakPtrFactory, such as
// raw_ptr<T> from the MiraclePtr project (though perhaps a non-nullable raw_ref
// equivalent).
template <typename T>
class SafeRef {
 public:
  // No default constructor, since there's no null state. Use an optional
  // SafeRef if the pointer may not be present.

  // Copy construction and assignment.
  SafeRef(const SafeRef& p) : w_(p.w_) {
    // Avoid use-after-move.
    CHECK(w_);
  }
  SafeRef& operator=(const SafeRef& p) {
    w_ = p.w_;
    // Avoid use-after-move.
    CHECK(w_);
    return *this;
  }

  // Move construction and assignment.
  SafeRef(SafeRef&& p) : w_(std::move(p.w_)) { CHECK(w_); }
  SafeRef& operator=(SafeRef&& p) {
    w_ = std::move(p.w_);
    // Avoid use-after-move.
    CHECK(w_);
    return *this;
  }

  // Copy conversion from SafeRef<U>.
  template <typename U>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SafeRef(const SafeRef<U>& p) : w_(p.w_) {
    // Avoid use-after-move.
    CHECK(w_);
  }
  template <typename U>
  SafeRef& operator=(const SafeRef<U>& p) {
    w_ = p.w_;
    // Avoid use-after-move.
    CHECK(w_);
    return *this;
  }

  // Move conversion from SafeRef<U>.
  template <typename U>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SafeRef(SafeRef<U>&& p) : w_(std::move(p.w_)) {
    // Avoid use-after-move.
    CHECK(w_);
  }
  template <typename U>
  SafeRef& operator=(SafeRef<U>&& p) {
    w_ = std::move(p.w_);
    // Avoid use-after-move.
    CHECK(w_);
    return *this;
  }

  // Call methods on the underlying T. Will CHECK() if the T pointee is no
  // longer alive.
  T* operator->() const {
    // We rely on WeakPtr<T> to CHECK() on a bad deref; tests verify this.
    return w_.operator->();
  }

  // Provide access to the underlying T as a reference. Will CHECK() if the T
  // pointee is no longer alive.
  T& operator*() const { return *operator->(); }

 private:
  template <typename U>
  friend class SafeRef;
  template <typename U>
  friend SafeRef<U> internal::MakeSafeRefFromWeakPtrInternals(
      const internal::WeakReference& ref,
      U* ptr);

  // Construction from a from WeakPtr. Will CHECK() if the WeakPtr is already
  // invalid.
  explicit SafeRef(WeakPtr<T> w) : w_(std::move(w)) { CHECK(w_); }

  WeakPtr<T> w_;
};

namespace internal {
template <typename T>
SafeRef<T> MakeSafeRefFromWeakPtrInternals(const internal::WeakReference& ref,
                                           T* ptr) {
  CHECK(ptr);
  return SafeRef<T>(WeakPtr<T>(ref, ptr));
}
}  // namespace internal

}  // namespace base

#endif  // BASE_MEMORY_SAFE_REF_H_
