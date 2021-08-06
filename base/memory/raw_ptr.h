// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_H_
#define BASE_MEMORY_RAW_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(USE_BACKUP_REF_PTR)
// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/base_export.h"
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

namespace base {

// NOTE: All methods should be ALWAYS_INLINE. raw_ptr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

namespace internal {
// These classes/structures are part of the raw_ptr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct RawPtrNoOpImpl {
  // Wraps a pointer.
  static ALWAYS_INLINE void* WrapRawPtr(void* ptr) { return ptr; }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(void*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr void* Upcast(void* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // The outer static_cast may change the address if upcasting to base that
    // lies in the middle of the derived object.
    return const_cast<typename std::remove_cv<To>::type*>(
        static_cast<To*>(static_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE void* Advance(void* wrapped_ptr, ptrdiff_t delta) {
    return static_cast<char*>(wrapped_ptr) + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  static ALWAYS_INLINE void* Duplicate(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
};

#if BUILDFLAG(USE_BACKUP_REF_PTR)

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
void CheckThatAddressIsntWithinFirstPartitionPage(void* ptr);
#endif

struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  static ALWAYS_INLINE bool IsSupportedAndNotNull(void* ptr) {
    // This covers the nullptr case, as address 0 is never in GigaCage.
    bool ret = IsManagedByPartitionAllocBRPPool(ptr);

    // There are many situations where the compiler can prove that
    // ReleaseWrappedPtr is called on a value that is always NULL, but the way
    // the check above is written, the compiler can't prove that NULL is not
    // managed by PartitionAlloc; and so the compiler has to emit a useless
    // check and dead code.
    // To avoid that without making the runtime check slower, explicitly promise
    // to the compiler that ret will always be false for NULL pointers.
    //
    // This condition would look nicer and might also theoretically be nicer for
    // the optimizer if it was written as "if (ptr == nullptr) { ... }", but
    // LLVM currently has issues with optimizing that away properly; see:
    // https://bugs.llvm.org/show_bug.cgi?id=49403
    // https://reviews.llvm.org/D97848
    // https://chromium-review.googlesource.com/c/chromium/src/+/2727400/2/base/memory/checked_ptr.h#120
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(ptr != nullptr || !ret);
#endif
#if HAS_BUILTIN(__builtin_assume)
    __builtin_assume(ptr != nullptr || !ret);
#endif

    // There may be pointers immediately after the allocation, e.g.
    //   {
    //     // Assume this allocation happens outside of PartitionAlloc.
    //     raw_ptr<T> ptr = new T[20];
    //     for (size_t i = 0; i < 20; i ++) { ptr++; }
    //   }
    //
    // Such pointers are *not* at risk of accidentally falling into BRP pool,
    // because:
    // 1) On 64-bit systems, BRP pool is preceded by non-BRP pool.
    // 2) On 32-bit systems, the guard pages and metadata of super pages in BRP
    //    pool aren't considered to be part of that pool.
    //
    // This allows us to make a stronger assertion that if
    // IsManagedByPartitionAllocBRPPool returns true for a valid pointer,
    // it must be at least partition page away from the beginning of a super
    // page.
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (ret) {
      CheckThatAddressIsntWithinFirstPartitionPage(ptr);
    }
#endif

    return ret;
  }

  // Wraps a pointer.
  static ALWAYS_INLINE void* WrapRawPtr(void* ptr) {
    if (IsSupportedAndNotNull(ptr)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(ptr != nullptr);
#endif
      AcquireInternal(ptr);
    }
#if !defined(PA_HAS_64_BITS_POINTERS)
    else
      AddressPoolManagerBitmap::IncrementOutsideOfBRPPoolPtrRefCount(ptr);
#endif

    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(void* wrapped_ptr) {
    if (IsSupportedAndNotNull(wrapped_ptr)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(wrapped_ptr != nullptr);
#endif
      ReleaseInternal(wrapped_ptr);
    }
#if !defined(PA_HAS_64_BITS_POINTERS)
    else
      AddressPoolManagerBitmap::DecrementOutsideOfBRPPoolPtrRefCount(
          wrapped_ptr);
#endif
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(void* wrapped_ptr) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (IsSupportedAndNotNull(wrapped_ptr)) {
      CHECK(wrapped_ptr != nullptr);
      CHECK(IsPointeeAlive(wrapped_ptr));
    }
#endif
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(void* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr void* Upcast(void* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // The outer static_cast may change the address if upcasting to base that
    // lies in the middle of the derived object.
    return const_cast<typename std::remove_cv<To>::type*>(
        static_cast<To*>(static_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE void* Advance(void* wrapped_ptr, ptrdiff_t delta) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (IsSupportedAndNotNull(wrapped_ptr))
      CHECK(IsValidDelta(wrapped_ptr, delta));
#endif
    void* new_wrapped_ptr =
        WrapRawPtr(reinterpret_cast<char*>(wrapped_ptr) + delta);
    ReleaseWrappedPtr(wrapped_ptr);
    return new_wrapped_ptr;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  // This method increments the reference count of the allocation slot.
  static ALWAYS_INLINE void* Duplicate(void* wrapped_ptr) {
    return WrapRawPtr(wrapped_ptr);
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}

 private:
  // We've evaluated several strategies (inline nothing, various parts, or
  // everything in |Wrap()| and |Release()|) using the Speedometer2 benchmark
  // to measure performance. The best results were obtained when only the
  // lightweight |IsManagedByPartitionAllocBRPPool()| check was inlined.
  // Therefore, we've extracted the rest into the functions below and marked
  // them as NOINLINE to prevent unintended LTO effects.
  static BASE_EXPORT NOINLINE void AcquireInternal(void* ptr);
  static BASE_EXPORT NOINLINE void ReleaseInternal(void* ptr);
  static BASE_EXPORT NOINLINE bool IsPointeeAlive(void* ptr);
  static BASE_EXPORT NOINLINE bool IsValidDelta(void* ptr, ptrdiff_t delta);
};

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

}  // namespace internal

// DO NOT USE! EXPERIMENTAL ONLY! This is helpful for local testing!
//
// raw_ptr<T> (formerly known as CheckedPtr<T>) is meant to be a raw pointer
// wrapper, that makes Use-After-Free (UaF) unexploitable, to prevent security
// issues. This is very much in the experimental phase. More context in:
// https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg
//
// By default, raw_ptr is a no-op wrapper (RawPtrNoOpImpl) to aid local testing.
// USE_BACKUP_REF_PTR switches to BackupRefPtrImpl and enables necessary support
// in PartitionAlloc, to enabled the UaF protection.
//
// Goals for this API:
// 1. Minimize amount of caller-side changes as much as physically possible.
// 2. Keep this class as small as possible, while still satisfying goal #1 (i.e.
//    we aren't striving to maximize compatibility with raw pointers, merely
//    adding support for cases encountered so far).
template <typename T,
#if BUILDFLAG(USE_BACKUP_REF_PTR)
          typename Impl = internal::BackupRefPtrImpl>
#else
          typename Impl = internal::RawPtrNoOpImpl>
#endif
class raw_ptr {
 public:
#if BUILDFLAG(USE_BACKUP_REF_PTR)

  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr ALWAYS_INLINE raw_ptr() noexcept : wrapped_ptr_(nullptr) {}

  raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(CastFromVoidPtr(Impl::Duplicate(p.AsVoidPtr()))) {}

  raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = nullptr;
  }

  raw_ptr& operator=(const raw_ptr& p) {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    T* new_ptr = CastFromVoidPtr(Impl::Duplicate(p.AsVoidPtr()));
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  raw_ptr& operator=(raw_ptr&& p) {
    if (LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(AsVoidPtr());
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  ALWAYS_INLINE ~raw_ptr() noexcept {
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    // Work around external issues where raw_ptr is used after destruction.
    wrapped_ptr_ = nullptr;
  }

#else  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr raw_ptr() noexcept = default;

  // In addition to nullptr_t ctor above, raw_ptr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; raw_ptr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  raw_ptr(const raw_ptr&) noexcept = default;
  raw_ptr(raw_ptr&&) noexcept = default;
  raw_ptr& operator=(const raw_ptr&) noexcept = default;
  raw_ptr& operator=(raw_ptr&&) noexcept = default;

  ~raw_ptr() = default;

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ALWAYS_INLINE raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(T* p) noexcept
      : wrapped_ptr_(CastFromVoidPtr(Impl::WrapRawPtr(CastToVoidPtr(p)))) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(const raw_ptr<U, Impl>& ptr) noexcept
      : wrapped_ptr_(CastFromVoidPtr(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.AsVoidPtr())))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE raw_ptr(raw_ptr<U, Impl>&& ptr) noexcept
      : wrapped_ptr_(
            CastFromVoidPtr(Impl::template Upcast<T, U>(ptr.AsVoidPtr()))) {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
  }

  ALWAYS_INLINE raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    wrapped_ptr_ = nullptr;
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    SetFromVoidPtr(Impl::WrapRawPtr(CastToVoidPtr(p)));
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE raw_ptr& operator=(const raw_ptr<U, Impl>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    SetFromVoidPtr(
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.AsVoidPtr())));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE raw_ptr& operator=(raw_ptr<U, Impl>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(AsVoidPtr());
    SetFromVoidPtr(Impl::template Upcast<T, U>(ptr.AsVoidPtr()));
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  ALWAYS_INLINE T* get() const { return GetForExtraction(); }

  explicit ALWAYS_INLINE operator bool() const { return !!wrapped_ptr_; }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  ALWAYS_INLINE U& operator*() const {
    return *GetForDereference();
  }
  ALWAYS_INLINE T* operator->() const { return GetForDereference(); }
  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit ALWAYS_INLINE operator U*() const {
    return static_cast<U*>(GetForExtraction());
  }

  ALWAYS_INLINE raw_ptr& operator++() {
    SetFromVoidPtr(Impl::Advance(AsVoidPtr(), sizeof(T)));
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator--() {
    SetFromVoidPtr(Impl::Advance(AsVoidPtr(), -sizeof(T)));
    return *this;
  }
  ALWAYS_INLINE raw_ptr operator++(int /* post_increment */) {
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  ALWAYS_INLINE raw_ptr operator--(int /* post_decrement */) {
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  ALWAYS_INLINE raw_ptr& operator+=(ptrdiff_t delta_elems) {
    SetFromVoidPtr(Impl::Advance(AsVoidPtr(), delta_elems * sizeof(T)));
    return *this;
  }
  ALWAYS_INLINE raw_ptr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  // Be careful to cover all cases with raw_ptr being on both sides, left
  // side only and right side only. If any case is missed, a more costly
  // |operator T*()| will get called, instead of |operator==|.
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, const raw_ptr& rhs) {
    return lhs.GetForComparison() == rhs.GetForComparison();
  }
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, const raw_ptr& rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, T* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, T* rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(T* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  friend ALWAYS_INLINE bool operator!=(T* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for cases like |derived_ptr == base_ptr|. Without these, a more
  // costly |operator T*()| will get called, instead of |operator==|.
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs,
                                       const raw_ptr<U, Impl>& rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() ==
           static_cast<std::add_cv_t<T>*>(rhs.GetForComparison());
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs,
                                       const raw_ptr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, U* rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() == static_cast<std::add_cv_t<T>*>(rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for comparisons against nullptr. Without these, a slightly more
  // costly version would be called that extracts wrapped pointer, as opposed
  // to plain comparison against 0.
  friend ALWAYS_INLINE bool operator==(const raw_ptr& lhs, std::nullptr_t) {
    return !lhs;
  }
  friend ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend ALWAYS_INLINE bool operator==(std::nullptr_t, const raw_ptr& rhs) {
    return !rhs;
  }
  friend ALWAYS_INLINE bool operator!=(std::nullptr_t, const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend ALWAYS_INLINE void swap(raw_ptr& lhs, raw_ptr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

 private:
  ALWAYS_INLINE static constexpr T* CastFromVoidPtr(void* ptr) {
    return static_cast<T*>(ptr);
  }
  ALWAYS_INLINE static void* CastToVoidPtr(T* ptr) {
    return static_cast<void*>(
        const_cast<typename std::remove_cv<T>::type*>(ptr));
  }
  ALWAYS_INLINE void* AsVoidPtr() const { return CastToVoidPtr(wrapped_ptr_); }
  ALWAYS_INLINE void SetFromVoidPtr(void* ptr) {
    wrapped_ptr_ = CastFromVoidPtr(ptr);
  }

  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  ALWAYS_INLINE T* GetForDereference() const {
    return CastFromVoidPtr(Impl::SafelyUnwrapPtrForDereference(AsVoidPtr()));
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  ALWAYS_INLINE T* GetForExtraction() const {
    return CastFromVoidPtr(Impl::SafelyUnwrapPtrForExtraction(AsVoidPtr()));
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  ALWAYS_INLINE T* GetForComparison() const {
    return CastFromVoidPtr(Impl::UnsafelyUnwrapPtrForComparison(AsVoidPtr()));
  }

  T* wrapped_ptr_;

  template <typename U, typename V>
  friend class raw_ptr;
};

}  // namespace base

using base::raw_ptr;

#endif  // BASE_MEMORY_RAW_PTR_H_
