// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_REF_H_
#define BASE_MEMORY_RAW_REF_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

template <class T, class RawPtrType>
class raw_ref;

namespace internal {

template <class T>
struct is_raw_ref : std::false_type {};

template <class T, class I>
struct is_raw_ref<::base::raw_ref<T, I>> : std::true_type {};

template <class T>
constexpr inline bool is_raw_ref_v = is_raw_ref<T>::value;

}  // namespace internal

// A smart pointer for a pointer which can not be null, and which provides
// Use-after-Free protection in the same ways as raw_ptr. This class acts like a
// combination of std::reference_wrapper and raw_ptr.
//
// See raw_ptr and //base/memory/raw_ptr.md for more details on the
// Use-after-Free protection.
//
// # Use after move
//
// The raw_ref type will abort if used after being moved.
//
// # Constness
//
// Use a `const raw_ref<T>` when the smart pointer should not be able to rebind
// to a new reference. Use a `const raw_ref<const T>` do the same for a const
// reference, which is like `const T&`.
//
// Unlike a native `T&` reference, a mutable `raw_ref<T>` can be changed
// independent of the underlying `T`, similar to `std::reference_wrapper`. That
// means the reference inside it can be moved and reassigned.
template <class T, class RawPtrType = DefaultRawPtrType>
class TRIVIAL_ABI GSL_POINTER raw_ref {
  using Inner = raw_ptr<T, RawPtrType>;
  using Impl = typename raw_ptr_traits::RawPtrTypeToImpl<RawPtrType>::Impl;
  // These impls do not clear on move, which produces an inconsistent behaviour.
  // We want consistent behaviour such that using a raw_ref after move is caught
  // and aborts. Failure to clear would be indicated by the related death tests
  // not CHECKing appropriately.
  static constexpr bool need_clear_after_move =
      std::is_same_v<Impl, internal::RawPtrNoOpImpl> ||
#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
      std::is_same_v<Impl,
                     internal::MTECheckedPtrImpl<
                         internal::MTECheckedPtrImplPartitionAllocSupport>> ||
#endif  // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
      std::is_same_v<Impl, internal::AsanBackupRefPtrImpl>;

 public:
  ALWAYS_INLINE explicit raw_ref(T& p) noexcept : inner_(std::addressof(p)) {}

  ALWAYS_INLINE raw_ref& operator=(T& p) noexcept {
    inner_.operator=(&p);
    return *this;
  }

  // Disallow holding references to temporaries.
  raw_ref(const T&& p) = delete;
  raw_ref& operator=(const T&& p) = delete;

  ALWAYS_INLINE raw_ref(const raw_ref& p) noexcept : inner_(p.inner_) {
    CHECK(inner_.get());  // Catch use-after-move.
  }

  ALWAYS_INLINE raw_ref(raw_ref&& p) noexcept : inner_(std::move(p.inner_)) {
    CHECK(inner_.get());  // Catch use-after-move.
    if constexpr (need_clear_after_move)
      p.inner_ = nullptr;
  }

  ALWAYS_INLINE raw_ref& operator=(const raw_ref& p) noexcept {
    CHECK(p.inner_.get());  // Catch use-after-move.
    inner_.operator=(p.inner_);
    return *this;
  }

  ALWAYS_INLINE raw_ref& operator=(raw_ref&& p) noexcept {
    CHECK(p.inner_.get());  // Catch use-after-move.
    inner_.operator=(std::move(p.inner_));
    if constexpr (need_clear_after_move)
      p.inner_ = nullptr;
    return *this;
  }

  // Deliberately implicit in order to support implicit upcast.
  template <class U, class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ref(const raw_ref<U, RawPtrType>& p) noexcept
      : inner_(p.inner_) {
    CHECK(inner_.get());  // Catch use-after-move.
  }
  // Deliberately implicit in order to support implicit upcast.
  template <class U, class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ref(raw_ref<U, RawPtrType>&& p) noexcept
      : inner_(std::move(p.inner_)) {
    CHECK(inner_.get());  // Catch use-after-move.
    if constexpr (need_clear_after_move)
      p.inner_ = nullptr;
  }

  static ALWAYS_INLINE raw_ref from_ptr(T* ptr) noexcept {
    CHECK(ptr);
    return raw_ref(*ptr);
  }

  // Upcast assignment
  template <class U, class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  ALWAYS_INLINE raw_ref& operator=(const raw_ref<U, RawPtrType>& p) noexcept {
    CHECK(p.inner_.get());  // Catch use-after-move.
    inner_.operator=(p.inner_);
    return *this;
  }
  template <class U, class = std::enable_if_t<std::is_convertible_v<U&, T&>>>
  ALWAYS_INLINE raw_ref& operator=(raw_ref<U, RawPtrType>&& p) noexcept {
    CHECK(p.inner_.get());  // Catch use-after-move.
    inner_.operator=(std::move(p.inner_));
    if constexpr (need_clear_after_move)
      p.inner_ = nullptr;
    return *this;
  }

  ALWAYS_INLINE T& operator*() const {
    CHECK(inner_.get());  // Catch use-after-move.
    return inner_.operator*();
  }

  // This is an equivalent to operator*() that provides GetForExtraction rather
  // rather than GetForDereference semantics (see raw_ptr.h). This should be
  // used in place of operator*() when the memory referred to by the reference
  // is not immediately going to be accessed.
  ALWAYS_INLINE T& get() const {
    CHECK(inner_.get());  // Catch use-after-move.
    return *inner_.get();
  }

  ALWAYS_INLINE T* operator->() const ABSL_ATTRIBUTE_RETURNS_NONNULL {
    CHECK(inner_.get());  // Catch use-after-move.
    return inner_.operator->();
  }

  ALWAYS_INLINE T* operator&() const ABSL_ATTRIBUTE_RETURNS_NONNULL {
    CHECK(inner_.get());  // Catch use-after-move.
    return inner_.get();
  }

  friend ALWAYS_INLINE void swap(raw_ref& lhs, raw_ref& rhs) noexcept {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    swap(lhs.inner_, rhs.inner_);
  }

#if BUILDFLAG(PA_USE_BASE_TRACING)
  // If T can be serialised into trace, its alias is also
  // serialisable.
  template <class U = T>
  typename perfetto::check_traced_value_support<U>::type WriteIntoTrace(
      perfetto::TracedValue&& context) const {
    CHECK(inner_.get());  // Catch use-after-move.
    inner_.WriteIntoTrace(std::move(context));
  }
#endif  // BUILDFLAG(PA_USE_BASE_TRACING)

  template <class U>
  friend ALWAYS_INLINE bool operator==(const raw_ref& lhs,
                                       const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ == rhs.inner_;
  }
  template <class U>
  friend ALWAYS_INLINE bool operator!=(const raw_ref& lhs,
                                       const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ != rhs.inner_;
  }
  template <class U>
  friend ALWAYS_INLINE bool operator<(const raw_ref& lhs,
                                      const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ < rhs.inner_;
  }
  template <class U>
  friend ALWAYS_INLINE bool operator>(const raw_ref& lhs,
                                      const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ > rhs.inner_;
  }
  template <class U>
  friend ALWAYS_INLINE bool operator<=(const raw_ref& lhs,
                                       const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ <= rhs.inner_;
  }
  template <class U>
  friend ALWAYS_INLINE bool operator>=(const raw_ref& lhs,
                                       const raw_ref<U, RawPtrType>& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ >= rhs.inner_;
  }

  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator==(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ == &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator!=(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ != &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator<(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ < &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator>(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ > &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator<=(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ <= &rhs;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator>=(const raw_ref& lhs, const U& rhs) {
    CHECK(lhs.inner_.get());  // Catch use-after-move.
    return lhs.inner_ >= &rhs;
  }

  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator==(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs == rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator!=(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs != rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator<(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs < rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator>(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs > rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator<=(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs <= rhs.inner_;
  }
  template <class U, class = std::enable_if_t<!internal::is_raw_ref_v<U>, void>>
  friend ALWAYS_INLINE bool operator>=(const U& lhs, const raw_ref& rhs) {
    CHECK(rhs.inner_.get());  // Catch use-after-move.
    return &lhs >= rhs.inner_;
  }

 private:
  template <class U, class J>
  friend class raw_ref;

  Inner inner_;
};

// CTAD deduction guide.
template <class T>
raw_ref(T) -> raw_ref<T>;

// Template helpers for working with raw_ref<T>.
template <typename T>
struct IsRawRef : std::false_type {};

template <typename T, typename I>
struct IsRawRef<raw_ref<T, I>> : std::true_type {};

template <typename T>
inline constexpr bool IsRawRefV = IsRawRef<T>::value;

template <typename T>
struct RemoveRawRef {
  using type = T;
};

template <typename T, typename I>
struct RemoveRawRef<raw_ref<T, I>> {
  using type = T;
};

template <typename T>
using RemoveRawRefT = typename RemoveRawRef<T>::type;

}  // namespace base

using base::raw_ref;

namespace std {

// Override so set/map lookups do not create extra raw_ref. This also
// allows C++ references to be used for lookup.
template <typename T, typename RawPtrType>
struct less<raw_ref<T, RawPtrType>> {
  using Impl =
      typename base::raw_ptr_traits::RawPtrTypeToImpl<RawPtrType>::Impl;
  using is_transparent = void;

  bool operator()(const raw_ref<T, RawPtrType>& lhs,
                  const raw_ref<T, RawPtrType>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(T& lhs, const raw_ref<T, RawPtrType>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(const raw_ref<T, RawPtrType>& lhs, T& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }
};

}  // namespace std

#endif  // BASE_MEMORY_RAW_REF_H_
