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

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace cc {
class Scheduler;
}
namespace base::internal {
class TimerBase;
}
namespace content::responsiveness {
class Calculator;
}

namespace base {

// NOTE: All methods should be `ALWAYS_INLINE NO_STACK_PROTECTOR`.
// ALWAYS_INLINE: raw_ptr is meant to be a lightweight replacement of a raw
// pointer, hence performance is critical.
// NO_STACK_PROTECTOR: This annotation is required to avoid failures when a
// raw_ptr is inside a NO_STACK_PROTECTOR function.
// TODO(https://crbug.com/1274129): Remove NO_STACK_PROTECTOR.
#define RAW_PTR_FUNC_ATTRIBUTES ALWAYS_INLINE NO_STACK_PROTECTOR

namespace internal {
// These classes/structures are part of the raw_ptr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct RawPtrNoOpImpl {
  // Wraps a pointer.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static RAW_PTR_FUNC_ATTRIBUTES constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* Advance(T* wrapped_ptr,
                                            ptrdiff_t delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static RAW_PTR_FUNC_ATTRIBUTES void IncrementSwapCountForTest() {}
  static RAW_PTR_FUNC_ATTRIBUTES void
  IncrementPointerToMemberOperatorCountForTest() {}
};

#if BUILDFLAG(USE_BACKUP_REF_PTR)

#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
BASE_EXPORT void CheckThatAddressIsntWithinFirstPartitionPage(
    uintptr_t address);
#endif

struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  static RAW_PTR_FUNC_ATTRIBUTES bool IsSupportedAndNotNull(uintptr_t address) {
    // This covers the nullptr case, as address 0 is never in GigaCage.
    bool is_in_brp_pool = IsManagedByPartitionAllocBRPPool(address);

    // There are many situations where the compiler can prove that
    // ReleaseWrappedPtr is called on a value that is always nullptr, but the
    // way the check above is written, the compiler can't prove that nullptr is
    // not managed by PartitionAlloc; and so the compiler has to emit a useless
    // check and dead code.
    // To avoid that without making the runtime check slower, explicitly promise
    // to the compiler that is_in_brp_pool will always be false for nullptr.
    //
    // This condition would look nicer and might also theoretically be nicer for
    // the optimizer if it was written as "if (!address) { ... }", but
    // LLVM currently has issues with optimizing that away properly; see:
    // https://bugs.llvm.org/show_bug.cgi?id=49403
    // https://reviews.llvm.org/D97848
    // https://chromium-review.googlesource.com/c/chromium/src/+/2727400/2/base/memory/checked_ptr.h#120
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(address || !is_in_brp_pool);
#endif
#if HAS_BUILTIN(__builtin_assume)
    __builtin_assume(address || !is_in_brp_pool);
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
    // 1) On 64-bit systems, BRP pool is preceded by a forbidden region.
    // 2) On 32-bit systems, the guard pages and metadata of super pages in BRP
    //    pool aren't considered to be part of that pool.
    //
    // This allows us to make a stronger assertion that if
    // IsManagedByPartitionAllocBRPPool returns true for a valid pointer,
    // it must be at least partition page away from the beginning of a super
    // page.
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (is_in_brp_pool) {
      CheckThatAddressIsntWithinFirstPartitionPage(address);
    }
#endif

    return is_in_brp_pool;
  }

  // Wraps a pointer.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* WrapRawPtr(T* ptr) {
    uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
    if (IsSupportedAndNotNull(address)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(ptr != nullptr);
#endif
      AcquireInternal(address);
    }
#if !defined(PA_HAS_64_BITS_POINTERS)
    else {
      AddressPoolManagerBitmap::IncrementOutsideOfBRPPoolPtrRefCount(address);
    }
#endif

    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES void ReleaseWrappedPtr(T* wrapped_ptr) {
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      CHECK(wrapped_ptr != nullptr);
#endif
      ReleaseInternal(address);
    }
#if !defined(PA_HAS_64_BITS_POINTERS)
    else {
      AddressPoolManagerBitmap::DecrementOutsideOfBRPPoolPtrRefCount(address);
    }
#endif
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
      CHECK(wrapped_ptr != nullptr);
      CHECK(IsPointeeAlive(address));
    }
#endif
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static RAW_PTR_FUNC_ATTRIBUTES constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by |delta| bytes.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* Advance(T* wrapped_ptr,
                                            ptrdiff_t delta_elem) {
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    uintptr_t address = reinterpret_cast<uintptr_t>(wrapped_ptr);
    if (IsSupportedAndNotNull(address))
      CHECK(IsValidDelta(address, delta_elem * sizeof(T)));
#endif
    T* new_wrapped_ptr = WrapRawPtr(wrapped_ptr + delta_elem);
    ReleaseWrappedPtr(wrapped_ptr);
    return new_wrapped_ptr;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  // This method increments the reference count of the allocation slot.
  template <typename T>
  static RAW_PTR_FUNC_ATTRIBUTES T* Duplicate(T* wrapped_ptr) {
    return WrapRawPtr(wrapped_ptr);
  }

  // This is for accounting only, used by unit tests.
  static RAW_PTR_FUNC_ATTRIBUTES void IncrementSwapCountForTest() {}
  static RAW_PTR_FUNC_ATTRIBUTES void
  IncrementPointerToMemberOperatorCountForTest() {}

 private:
  // We've evaluated several strategies (inline nothing, various parts, or
  // everything in |Wrap()| and |Release()|) using the Speedometer2 benchmark
  // to measure performance. The best results were obtained when only the
  // lightweight |IsManagedByPartitionAllocBRPPool()| check was inlined.
  // Therefore, we've extracted the rest into the functions below and marked
  // them as NOINLINE to prevent unintended LTO effects.
  static BASE_EXPORT NOINLINE void AcquireInternal(uintptr_t address);
  static BASE_EXPORT NOINLINE void ReleaseInternal(uintptr_t address);
  static BASE_EXPORT NOINLINE bool IsPointeeAlive(uintptr_t address);
  static BASE_EXPORT NOINLINE bool IsValidDelta(uintptr_t address,
                                                ptrdiff_t delta_in_bytes);
};

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

}  // namespace internal

namespace raw_ptr_traits {

// IsSupportedType<T>::value answers whether raw_ptr<T> 1) compiles and 2) is
// always safe at runtime.  Templates that may end up using `raw_ptr<T>` should
// use IsSupportedType to ensure that raw_ptr is not used with unsupported
// types.  As an example, see how base::internal::StorageTraits uses
// IsSupportedType as a condition for using base::internal::UnretainedWrapper
// (which has a `ptr_` field that will become `raw_ptr<T>` after the Big
// Rewrite).
template <typename T, typename SFINAE = void>
struct IsSupportedType {
  static constexpr bool value = true;
};

// raw_ptr<T> is not compatible with function pointer types. Also, they don't
// even need the raw_ptr protection, because they don't point on heap.
template <typename T>
struct IsSupportedType<T, std::enable_if_t<std::is_function<T>::value>> {
  static constexpr bool value = false;
};

// This section excludes some types from raw_ptr<T> to avoid them from being
// used inside base::Unretained in performance sensitive places. These were
// identified from sampling profiler data. See crbug.com/1287151 for more info.
template <>
struct IsSupportedType<cc::Scheduler> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<base::internal::TimerBase> {
  static constexpr bool value = false;
};
template <>
struct IsSupportedType<content::responsiveness::Calculator> {
  static constexpr bool value = false;
};

#if __OBJC__
// raw_ptr<T> is not compatible with pointers to Objective-C classes for a
// multitude of reasons. They may fail to compile in many cases, and wouldn't
// work well with tagged pointers. Anyway, Objective-C objects have their own
// way of tracking lifespan, hence don't need the raw_ptr protection as much.
//
// Such pointers are detected by checking if they're convertible to |id| type.
template <typename T>
struct IsSupportedType<T,
                       std::enable_if_t<std::is_convertible<T*, id>::value>> {
  static constexpr bool value = false;
};
#endif  // __OBJC__

#if BUILDFLAG(IS_WIN)
// raw_ptr<HWND__> is unsafe at runtime - if the handle happens to also
// represent a valid pointer into a PartitionAlloc-managed region then it can
// lead to manipulating random memory when treating it as BackupRefPtr
// ref-count.  See also https://crbug.com/1262017.
//
// TODO(https://crbug.com/1262017): Cover other handle types like HANDLE,
// HLOCAL, HINTERNET, or HDEVINFO.  Maybe we should avoid using raw_ptr<T> when
// T=void (as is the case in these handle types).  OTOH, explicit,
// non-template-based raw_ptr<void> should be allowed.  Maybe this can be solved
// by having 2 traits: IsPointeeAlwaysSafe (to be used in templates) and
// IsPointeeUsuallySafe (to be used in the static_assert in raw_ptr).  The
// upside of this approach is that it will safely handle base::Bind closing over
// HANDLE.  The downside of this approach is that base::Bind closing over a
// void* pointer will not get UaF protection.
#define CHROME_WINDOWS_HANDLE_TYPE(name)   \
  template <>                              \
  struct IsSupportedType<name##__, void> { \
    static constexpr bool value = false;   \
  };
#include "base/win/win_handle_types_list.inc"
#undef CHROME_WINDOWS_HANDLE_TYPE
#endif

}  // namespace raw_ptr_traits

// `raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety
// over raw pointers.  It behaves just like a raw pointer on platforms where
// USE_BACKUP_REF_PTR is off, and almost like one when it's on (the main
// difference is that it's zero-initialized and cleared on destruction and
// move). Unlike `std::unique_ptr<T>`, `base::scoped_refptr<T>`, etc., it
// doesn’t manage ownership or lifetime of an allocated object - you are still
// responsible for freeing the object when no longer used, just as you would
// with a raw C++ pointer.
//
// Compared to a raw C++ pointer, on platforms where USE_BACKUP_REF_PTR is on,
// `raw_ptr<T>` incurs additional performance overhead for initialization,
// destruction, and assignment (including `ptr++` and `ptr += ...`).  There is
// no overhead when dereferencing a pointer.
//
// `raw_ptr<T>` is beneficial for security, because it can prevent a significant
// percentage of Use-after-Free (UaF) bugs from being exploitable.  `raw_ptr<T>`
// has limited impact on stability - dereferencing a dangling pointer remains
// Undefined Behavior.  Note that the security protection is not yet enabled by
// default.
template <typename T,
#if BUILDFLAG(USE_BACKUP_REF_PTR)
          typename Impl = internal::BackupRefPtrImpl>
#else
          typename Impl = internal::RawPtrNoOpImpl>
#endif
class TRIVIAL_ABI raw_ptr {
 public:
  static_assert(raw_ptr_traits::IsSupportedType<T>::value,
                "raw_ptr<T> doesn't work with this kind of pointee type T");

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr RAW_PTR_FUNC_ATTRIBUTES raw_ptr() noexcept
      : wrapped_ptr_(nullptr) {}

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = nullptr;
  }

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(const raw_ptr& p) {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    T* new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(raw_ptr&& p) {
    if (LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  RAW_PTR_FUNC_ATTRIBUTES ~raw_ptr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where raw_ptr is used after destruction.
    wrapped_ptr_ = nullptr;
  }

#else  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr RAW_PTR_FUNC_ATTRIBUTES raw_ptr() noexcept = default;

  // In addition to nullptr_t ctor above, raw_ptr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; raw_ptr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(const raw_ptr&) noexcept = default;
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(raw_ptr&&) noexcept = default;
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(const raw_ptr&) noexcept = default;
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(raw_ptr&&) noexcept = default;

  RAW_PTR_FUNC_ATTRIBUTES ~raw_ptr() = default;

#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr RAW_PTR_FUNC_ATTRIBUTES raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(T* p) noexcept
      : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(const raw_ptr<U, Impl>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr(raw_ptr<U, Impl>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
  }

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = nullptr;
    return *this;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(
      const raw_ptr<U, Impl>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ =
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator=(raw_ptr<U, Impl>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if DCHECK_IS_ON() || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    CHECK(reinterpret_cast<uintptr_t>(this) !=
          reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    ptr.wrapped_ptr_ = nullptr;
#endif
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  RAW_PTR_FUNC_ATTRIBUTES T* get() const { return GetForExtraction(); }

  explicit RAW_PTR_FUNC_ATTRIBUTES operator bool() const {
    return !!wrapped_ptr_;
  }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  RAW_PTR_FUNC_ATTRIBUTES U& operator*() const {
    return *GetForDereference();
  }
  RAW_PTR_FUNC_ATTRIBUTES T* operator->() const { return GetForDereference(); }

  // Disables `(my_raw_ptr->*pmf)(...)` as a workaround for
  // the ICE in GCC parsing the code, reported at
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103455
  template <typename PMF>
  void operator->*(PMF) const = delete;

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  RAW_PTR_FUNC_ATTRIBUTES operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit RAW_PTR_FUNC_ATTRIBUTES operator U*() const {
    // This operator may be invoked from static_cast, meaning the types may not
    // be implicitly convertible, hence the need for static_cast here.
    return static_cast<U*>(GetForExtraction());
  }

  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, 1);
    return *this;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -1);
    return *this;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr operator++(int /* post_increment */) {
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr operator--(int /* post_decrement */) {
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator+=(ptrdiff_t delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems);
    return *this;
  }
  RAW_PTR_FUNC_ATTRIBUTES raw_ptr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  // Comparison operators between raw_ptr and raw_ptr<U>/U*/std::nullptr_t.
  // Strictly speaking, it is not necessary to provide these: the compiler can
  // use the conversion operator implicitly to allow comparisons to fall back to
  // comparisons between raw pointers. However, `operator T*`/`operator U*` may
  // perform safety checks with a higher runtime cost, so to avoid this, provide
  // explicit comparison operators for all combinations of parameters.

  // Comparisons between `raw_ptr`s. This unusual declaration and separate
  // definition below is because `GetForComparison()` is a private method. The
  // more conventional approach of defining a comparison operator between
  // `raw_ptr` and `raw_ptr<U>` in the friend declaration itself does not work,
  // because a comparison operator defined inline would not be allowed to call
  // `raw_ptr<U>`'s private `GetForComparison()` method.
  template <typename U, typename V, typename I>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator==(const raw_ptr<U, I>& lhs,
                                                 const raw_ptr<V, I>& rhs);
  template <typename U>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator!=(const raw_ptr& lhs,
                                                 const raw_ptr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }

  // Comparisons with U*. These operators also handle the case where the RHS is
  // T*.
  template <typename U>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator==(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  template <typename U>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }

  // Comparisons with `std::nullptr_t`.
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator==(const raw_ptr& lhs,
                                                 std::nullptr_t) {
    return !lhs;
  }
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator!=(const raw_ptr& lhs,
                                                 std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator==(std::nullptr_t,
                                                 const raw_ptr& rhs) {
    return !rhs;
  }
  friend RAW_PTR_FUNC_ATTRIBUTES bool operator!=(std::nullptr_t,
                                                 const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend RAW_PTR_FUNC_ATTRIBUTES void swap(raw_ptr& lhs,
                                           raw_ptr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  RAW_PTR_FUNC_ATTRIBUTES T* GetForDereference() const {
    return Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_);
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  RAW_PTR_FUNC_ATTRIBUTES T* GetForExtraction() const {
    return Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  RAW_PTR_FUNC_ATTRIBUTES T* GetForComparison() const {
    return Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_);
  }

  T* wrapped_ptr_;

  template <typename U, typename V>
  friend class raw_ptr;
};

template <typename U, typename V, typename I>
RAW_PTR_FUNC_ATTRIBUTES bool operator==(const raw_ptr<U, I>& lhs,
                                        const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() == rhs.GetForComparison();
}

}  // namespace base

using base::raw_ptr;

#endif  // BASE_MEMORY_RAW_PTR_H_
