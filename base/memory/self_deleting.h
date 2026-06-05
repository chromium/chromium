// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SELF_DELETING_H_
#define BASE_MEMORY_SELF_DELETING_H_

#include <concepts>
#include <utility>

#include "base/types/pass_key.h"

// Helpers to make it possible to instantiate self-deleting classes without
// using the `new` keyword. The helpers here make it harder for self-deleting
// classes to be constructed without `base::MakeSelfDeleting()`. They also
// ensure self-deleting classes cannot be directly deleted by others.
//
// See base/memory/self_deleting_unittest.cc for an example.

namespace base {

class SelfDeleting;
namespace internal {
class MakeSelfDeletingImpl;
}  // namespace internal

using SelfDeletingPassKey = PassKey<internal::MakeSelfDeletingImpl>;

namespace internal {

class MakeSelfDeletingImpl {
 public:
  template <typename T, typename... Args>
  static T* MakeSelfDeleting(Args&&... args) {
    static_assert(std::derived_from<T, SelfDeleting>,
                  "Must be derived from base::SelfDeleting.");
    static_assert(!std::destructible<T>,
                  "Destructor is public. Make it private to avoid misuse.");

    return new T(std::forward<Args>(args)..., SelfDeletingPassKey());
  }
};

}  // namespace internal

// Call this to create a self-deleting class, instead of using `new` directly.
// For this to compile, the self-deleting class must:
//
// 1) Inherit from class SelfDeleting below.
//
// 2) Take a SelfDeletingPassKey as the constructor's last parameter.
//
// 3) Not have a public destructor.
//
// Perform self-deletion normally with `delete this;`.
template <typename T, typename... Args>
T* MakeSelfDeleting(Args&&... args) {
  return internal::MakeSelfDeletingImpl::MakeSelfDeleting<T>(
      std::forward<Args>(args)...);
}

// The self-deleting class must inherit from this class:
//
// 1) Allows the self-deleting class to receive a SelfDeletingPassKey.
//
// 2) Makes it more obvious that the class is self-deleting.
class SelfDeleting {
 protected:
  explicit SelfDeleting(SelfDeletingPassKey) {}
  ~SelfDeleting() = default;
};

}  // namespace base

#endif  // BASE_MEMORY_SELF_DELETING_H_
