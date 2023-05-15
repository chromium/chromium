// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <climits>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cxx20_is_constant_evaluated.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr_exclusion.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_WIN)
#include "base/allocator/partition_allocator/partition_alloc_base/win/win_handle_types.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/partition_alloc_base/check.h"
// Live implementation of MiraclePtr being built.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#define PA_RAW_PTR_CHECK(condition) PA_BASE_CHECK(condition)
#else
// No-op implementation of MiraclePtr being built.
// Note that `PA_BASE_DCHECK()` evaporates from non-DCHECK builds,
// minimizing impact of generated code.
#define PA_RAW_PTR_CHECK(condition) PA_BASE_DCHECK(condition)
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#else   // BUILDFLAG(USE_PARTITION_ALLOC)
// Without PartitionAlloc, there's no `PA_BASE_D?CHECK()` implementation
// available.
#define PA_RAW_PTR_CHECK(condition)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#include "base/allocator/partition_allocator/pointers/raw_ptr_backup_ref_impl.h"
#endif

#if BUILDFLAG(USE_ASAN_UNOWNED_PTR)
#include "base/allocator/partition_allocator/pointers/raw_ptr_asan_unowned_impl.h"
#endif

#if BUILDFLAG(USE_HOOKABLE_RAW_PTR)
#include "base/allocator/partition_allocator/pointers/raw_ptr_hookable_impl.h"
#endif

namespace cc {
class Scheduler;
}
namespace base::internal {
class DelayTimerBase;
}
namespace content::responsiveness {
class Calculator;
}

namespace base {

// NOTE: All methods should be `PA_ALWAYS_INLINE`. raw_ptr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

// This is a bitfield representing the different flags that can be applied to a
// raw_ptr.
//
// Internal use only: Developers shouldn't use those values directly.
//
// Housekeeping rules: Try not to change trait values, so that numeric trait
// values stay constant across builds (could be useful e.g. when analyzing stack
// traces). A reasonable exception to this rule are `*ForTest` traits. As a
// matter of fact, we propose that new non-test traits are added before the
// `*ForTest` traits.
enum class RawPtrTraits : unsigned {
  kEmpty = 0,

  // Disables dangling pointer detection, but keeps other raw_ptr protections.
  //
  // Don't use directly, use DisableDanglingPtrDetection or DanglingUntriaged
  // instead.
  kMayDangle = (1 << 0),

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
  // Disables any hooks, by switching to NoOpImpl in that case.
  //
  // Internal use only.
  kDisableHooks = (1 << 2),
#else
  kDisableHooks = kEmpty,
#endif

  // Pointer arithmetic is discouraged and disabled by default.
  //
  // Don't use directly, use AllowPtrArithmetic instead.
  kAllowPtrArithmetic = (1 << 3),

  // This pointer is evaluated by a separate, Ash-related experiment.
  //
  // Don't use directly, use ExperimentalAsh instead.
  kExperimentalAsh = (1 << 4),

  // *** ForTest traits below ***

  // Adds accounting, on top of the chosen implementation, for test purposes.
  // raw_ptr/raw_ref with this trait perform extra bookkeeping, e.g. to track
  // the number of times the raw_ptr is wrapped, unwrapped, etc.
  //
  // Test only.
  kUseCountingWrapperForTest = (1 << 10),

  // Helper trait that can be used to test raw_ptr's behaviour or conversions.
  //
  // Test only.
  kDummyForTest = (1 << 11),
};

// Used to combine RawPtrTraits:
constexpr RawPtrTraits operator|(RawPtrTraits a, RawPtrTraits b) {
  return static_cast<RawPtrTraits>(static_cast<unsigned>(a) |
                                   static_cast<unsigned>(b));
}
constexpr RawPtrTraits operator&(RawPtrTraits a, RawPtrTraits b) {
  return static_cast<RawPtrTraits>(static_cast<unsigned>(a) &
                                   static_cast<unsigned>(b));
}
constexpr RawPtrTraits operator~(RawPtrTraits a) {
  return static_cast<RawPtrTraits>(~static_cast<unsigned>(a));
}

namespace raw_ptr_traits {

constexpr bool Contains(RawPtrTraits a, RawPtrTraits b) {
  return (a & b) != RawPtrTraits::kEmpty;
}

constexpr RawPtrTraits Remove(RawPtrTraits a, RawPtrTraits b) {
  return a & ~b;
}

constexpr bool AreValid(RawPtrTraits traits) {
  return Remove(traits, RawPtrTraits::kMayDangle | RawPtrTraits::kDisableHooks |
                            RawPtrTraits::kAllowPtrArithmetic |
                            RawPtrTraits::kExperimentalAsh |
                            RawPtrTraits::kUseCountingWrapperForTest |
                            RawPtrTraits::kDummyForTest) ==
         RawPtrTraits::kEmpty;
}

template <RawPtrTraits Traits>
struct TraitsToImpl;

}  // namespace raw_ptr_traits

template <typename T, RawPtrTraits Traits = RawPtrTraits::kEmpty>
class raw_ptr;

}  // namespace base

// This type is to be used internally, or in callbacks arguments when it is
// known that they might receive dangling pointers. In any other cases, please
// use one of:
// - raw_ptr<T, DanglingUntriaged>
// - raw_ptr<T, DisableDanglingPtrDetection>
template <typename T, base::RawPtrTraits Traits = base::RawPtrTraits::kEmpty>
using MayBeDangling = base::raw_ptr<T, Traits | base::RawPtrTraits::kMayDangle>;

namespace base {

struct RawPtrGlobalSettings {
  static void EnableExperimentalAsh() {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    internal::BackupRefPtrGlobalSettings::EnableExperimentalAsh();
#endif
  }

  static void DisableExperimentalAshForTest() {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    internal::BackupRefPtrGlobalSettings::DisableExperimentalAshForTest();
#endif
  }
};

namespace internal {

struct RawPtrNoOpImpl {
  static constexpr bool kMustZeroOnInit = false;
  static constexpr bool kMustZeroOnMove = false;
  static constexpr bool kMustZeroOnDestruct = false;

  // Wraps a pointer.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  PA_ALWAYS_INLINE static constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  // Retreat the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Retreat(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr - delta_elems;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                            T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // `WrapRawPtrForDuplication` and `UnsafelyUnwrapPtrForDuplication` are used
  // to create a new raw_ptr<T> from another raw_ptr<T> of a different flavor.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtrForDuplication(T* ptr) {
    return ptr;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForDuplication(
      T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  PA_ALWAYS_INLINE constexpr static void IncrementSwapCountForTest() {}
  PA_ALWAYS_INLINE constexpr static void IncrementLessCountForTest() {}
  PA_ALWAYS_INLINE constexpr static void
  IncrementPointerToMemberOperatorCountForTest() {}
};

// Wraps a raw_ptr/raw_ref implementation, with a class of the same interface
// that provides accounting, for test purposes. raw_ptr/raw_ref that use it
// perform extra bookkeeping, e.g. to track the number of times the raw_ptr is
// wrapped, unrwapped, etc.
//
// Test only.
template <RawPtrTraits Traits>
struct RawPtrCountingImplWrapperForTest
    : public raw_ptr_traits::TraitsToImpl<Traits>::Impl {
  static_assert(
      !raw_ptr_traits::Contains(Traits,
                                RawPtrTraits::kUseCountingWrapperForTest));

  using SuperImpl = typename raw_ptr_traits::TraitsToImpl<Traits>::Impl;

  static constexpr bool kMustZeroOnInit = SuperImpl::kMustZeroOnInit;
  static constexpr bool kMustZeroOnMove = SuperImpl::kMustZeroOnMove;
  static constexpr bool kMustZeroOnDestruct = SuperImpl::kMustZeroOnDestruct;

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtr(T* ptr) {
    ++wrap_raw_ptr_cnt;
    return SuperImpl::WrapRawPtr(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr void ReleaseWrappedPtr(T* ptr) {
    ++release_wrapped_ptr_cnt;
    SuperImpl::ReleaseWrappedPtr(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    ++get_for_dereference_cnt;
    return SuperImpl::SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    ++get_for_extraction_cnt;
    return SuperImpl::SafelyUnwrapPtrForExtraction(wrapped_ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    ++get_for_comparison_cnt;
    return SuperImpl::UnsafelyUnwrapPtrForComparison(wrapped_ptr);
  }

  PA_ALWAYS_INLINE static constexpr void IncrementSwapCountForTest() {
    ++wrapped_ptr_swap_cnt;
  }

  PA_ALWAYS_INLINE static constexpr void IncrementLessCountForTest() {
    ++wrapped_ptr_less_cnt;
  }

  PA_ALWAYS_INLINE static constexpr void
  IncrementPointerToMemberOperatorCountForTest() {
    ++pointer_to_member_operator_cnt;
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtrForDuplication(T* ptr) {
    ++wrap_raw_ptr_for_dup_cnt;
    return SuperImpl::WrapRawPtrForDuplication(ptr);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForDuplication(
      T* wrapped_ptr) {
    ++get_for_duplication_cnt;
    return SuperImpl::UnsafelyUnwrapPtrForDuplication(wrapped_ptr);
  }

  static constexpr void ClearCounters() {
    wrap_raw_ptr_cnt = 0;
    release_wrapped_ptr_cnt = 0;
    get_for_dereference_cnt = 0;
    get_for_extraction_cnt = 0;
    get_for_comparison_cnt = 0;
    wrapped_ptr_swap_cnt = 0;
    wrapped_ptr_less_cnt = 0;
    pointer_to_member_operator_cnt = 0;
    wrap_raw_ptr_for_dup_cnt = 0;
    get_for_duplication_cnt = 0;
  }

  static inline int wrap_raw_ptr_cnt = INT_MIN;
  static inline int release_wrapped_ptr_cnt = INT_MIN;
  static inline int get_for_dereference_cnt = INT_MIN;
  static inline int get_for_extraction_cnt = INT_MIN;
  static inline int get_for_comparison_cnt = INT_MIN;
  static inline int wrapped_ptr_swap_cnt = INT_MIN;
  static inline int wrapped_ptr_less_cnt = INT_MIN;
  static inline int pointer_to_member_operator_cnt = INT_MIN;
  static inline int wrap_raw_ptr_for_dup_cnt = INT_MIN;
  static inline int get_for_duplication_cnt = INT_MIN;
};

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
struct IsSupportedType<base::internal::DelayTimerBase> {
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
#define PA_WINDOWS_HANDLE_TYPE(name)       \
  template <>                              \
  struct IsSupportedType<name##__, void> { \
    static constexpr bool value = false;   \
  };
#include "base/allocator/partition_allocator/partition_alloc_base/win/win_handle_types_list.inc"
#undef PA_WINDOWS_HANDLE_TYPE
#endif

template <RawPtrTraits Traits>
struct TraitsToImpl {
  static_assert(AreValid(Traits), "Unknown raw_ptr trait(s)");

 private:
  // UnderlyingImpl is the struct that provides the implementation of the
  // protections related to raw_ptr.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  using UnderlyingImpl = internal::RawPtrBackupRefImpl<
      /*AllowDangling=*/Contains(Traits, RawPtrTraits::kMayDangle),
      /*ExperimentalAsh=*/Contains(Traits, RawPtrTraits::kExperimentalAsh)>;

#elif BUILDFLAG(USE_ASAN_UNOWNED_PTR)
  using UnderlyingImpl = std::conditional_t<
      Contains(Traits, RawPtrTraits::kMayDangle),
      // No special bookkeeping required for this case,
      // just treat these as ordinary pointers.
      internal::RawPtrNoOpImpl,
      internal::RawPtrAsanUnownedImpl<
          Contains(Traits, RawPtrTraits::kAllowPtrArithmetic)>>;
#elif BUILDFLAG(USE_HOOKABLE_RAW_PTR)
  using UnderlyingImpl =
      std::conditional_t<Contains(Traits, RawPtrTraits::kDisableHooks),
                         internal::RawPtrNoOpImpl,
                         internal::RawPtrHookableImpl>;
#else
  using UnderlyingImpl = internal::RawPtrNoOpImpl;
#endif

 public:
  // Impl is the struct that implements raw_ptr functions. Think of raw_ptr as a
  // thin wrapper, that directs calls to Impl.
  // Impl may be different from UnderlyingImpl, because it may include a
  // wrapper.
  using Impl = std::conditional_t<
      Contains(Traits, RawPtrTraits::kUseCountingWrapperForTest),
      internal::RawPtrCountingImplWrapperForTest<
          Remove(Traits, RawPtrTraits::kUseCountingWrapperForTest)>,
      UnderlyingImpl>;
};

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
//
// raw_ptr<T> is marked as [[gsl::Pointer]] which allows the compiler to catch
// some bugs where the raw_ptr holds a dangling pointer to a temporary object.
// However the [[gsl::Pointer]] analysis expects that such types do not have a
// non-default move constructor/assignment. Thus, it's possible to get an error
// where the pointer is not actually dangling, and have to work around the
// compiler. We have not managed to construct such an example in Chromium yet.
template <typename T, RawPtrTraits Traits>
class PA_TRIVIAL_ABI PA_GSL_POINTER raw_ptr {
 public:
  using Impl = typename raw_ptr_traits::TraitsToImpl<Traits>::Impl;

#if !BUILDFLAG(USE_PARTITION_ALLOC)
  // See comment at top about `PA_RAW_PTR_CHECK()`.
  static_assert(std::is_same_v<Impl, internal::RawPtrNoOpImpl>);
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC)

  static_assert(raw_ptr_traits::IsSupportedType<T>::value,
                "raw_ptr<T> doesn't work with this kind of pointee type T");

  // TODO(bartekn): Turn on zeroing as much as possible, to reduce
  // pointer-related UBs. In the current implementation we do it only when the
  // underlying implementation needs it for correctness, for performance
  // reasons. There are two secnarios where it's important:
  // 1. When rewriting renderer, we don't want extra overhead get in the way of
  //    our perf evaluation.
  // 2. The same applies to rewriting 3rd party libraries, but also we want
  //    RawPtrNoOpImpl to be a true no-op, in case the library is linked with
  //    a product other than Chromium (this can be mitigated using
  //    `build_with_chromium` GN variable).
  static constexpr bool kZeroOnInit = Impl::kMustZeroOnInit;
  static constexpr bool kZeroOnMove = Impl::kMustZeroOnMove;
  static constexpr bool kZeroOnDestruct = Impl::kMustZeroOnDestruct;

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR)
  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  PA_ALWAYS_INLINE constexpr raw_ptr() noexcept {
    if constexpr (kZeroOnInit) {
      wrapped_ptr_ = nullptr;
    }
  }

  PA_ALWAYS_INLINE constexpr raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  PA_ALWAYS_INLINE constexpr raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    if constexpr (kZeroOnMove) {
      p.wrapped_ptr_ = nullptr;
    }
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(const raw_ptr& p) noexcept {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    //
    // Unlike the move version of this operator, don't add |this != &p| branch,
    // for performance reasons. Even though Duplicate() is not cheap, we
    // practically never assign a raw_ptr<T> to itself. We suspect that a
    // cumulative cost of a conditional branch, even if always correctly
    // predicted, would exceed that.
    T* new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(raw_ptr&& p) noexcept {
    // Unlike the the copy version of this operator, this branch is necessaty
    // for correctness.
    if (PA_LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      if constexpr (kZeroOnMove) {
        p.wrapped_ptr_ = nullptr;
      }
    }
    return *this;
  }

// Constexpr destructors were introduced in C++20. PartitionAlloc's minimum
// supported C++ version is C++17.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
  PA_ALWAYS_INLINE constexpr ~raw_ptr() noexcept {
#else
  PA_ALWAYS_INLINE ~raw_ptr() noexcept {
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where raw_ptr is used after destruction.
    if constexpr (kZeroOnDestruct) {
      wrapped_ptr_ = nullptr;
    }
  }

#else   // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR)

  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).
  PA_ALWAYS_INLINE constexpr raw_ptr() noexcept = default;

  // In addition to nullptr_t ctor above, raw_ptr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; raw_ptr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  PA_ALWAYS_INLINE raw_ptr(const raw_ptr&) noexcept = default;
  PA_ALWAYS_INLINE raw_ptr(raw_ptr&&) noexcept = default;
  PA_ALWAYS_INLINE raw_ptr& operator=(const raw_ptr&) noexcept = default;
  PA_ALWAYS_INLINE raw_ptr& operator=(raw_ptr&&) noexcept = default;

  PA_ALWAYS_INLINE ~raw_ptr() noexcept = default;

  // With default constructor, destructor and move operations, we don't have an
  // opportunity to zero the underlying pointer, so ensure this isn't expected.
  static_assert(!kZeroOnInit);
  static_assert(!kZeroOnMove);
  static_assert(!kZeroOnDestruct);
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR) || BUILDFLAG(USE_HOOKABLE_RAW_PTR)

  // Cross-kind copy constructor.
  // Move is not supported as different traits may use different ref-counts, so
  // let move operations degrade to copy, which handles it well.
  template <RawPtrTraits PassedTraits,
            typename Unused = std::enable_if_t<Traits != PassedTraits>>
  PA_ALWAYS_INLINE constexpr explicit raw_ptr(
      const raw_ptr<T, PassedTraits>& p) noexcept
      : wrapped_ptr_(Impl::WrapRawPtrForDuplication(
            raw_ptr_traits::TraitsToImpl<PassedTraits>::Impl::
                UnsafelyUnwrapPtrForDuplication(p.wrapped_ptr_))) {
    // Limit cross-kind conversions only to cases where kMayDangle gets added,
    // because that's needed for Unretained(Ref)Wrapper. Use a static_assert,
    // instead of disabling via SFINAE, so that the compiler catches other
    // conversions. Otherwise implicit raw_ptr<T> -> T* -> raw_ptr<> route will
    // be taken.
    static_assert(Traits == (PassedTraits | RawPtrTraits::kMayDangle));
  }

  // Cross-kind assignment.
  // Move is not supported as different traits may use different ref-counts, so
  // let move operations degrade to copy, which handles it well.
  template <RawPtrTraits PassedTraits,
            typename Unused = std::enable_if_t<Traits != PassedTraits>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr<T, PassedTraits>& p) noexcept {
    // Limit cross-kind assignments only to cases where kMayDangle gets added,
    // because that's needed for Unretained(Ref)Wrapper. Use a static_assert,
    // instead of disabling via SFINAE, so that the compiler catches other
    // conversions. Otherwise implicit raw_ptr<T> -> T* -> raw_ptr<> route will
    // be taken.
    static_assert(Traits == (PassedTraits | RawPtrTraits::kMayDangle));

    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtrForDuplication(
        raw_ptr_traits::TraitsToImpl<PassedTraits>::Impl::
            UnsafelyUnwrapPtrForDuplication(p.wrapped_ptr_));
    return *this;
  }

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // Ignore kZeroOnInit, because here the caller explicitly wishes to initialize
  // with nullptr. NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(T* p) noexcept
      : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(const raw_ptr<U, Traits>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr raw_ptr(raw_ptr<U, Traits>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
    if constexpr (kZeroOnMove) {
      ptr.wrapped_ptr_ = nullptr;
    }
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = nullptr;
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      const raw_ptr<U, Traits>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at raw_ptr address,
    // not its contained pointer value). The comparison is only needed when they
    // are the same type, otherwise they can't be the same raw_ptr object.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if constexpr (std::is_same_v<raw_ptr, std::decay_t<decltype(ptr)>>) {
      PA_RAW_PTR_CHECK(this != &ptr);
    }
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
  PA_ALWAYS_INLINE constexpr raw_ptr& operator=(
      raw_ptr<U, Traits>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at raw_ptr address,
    // not its contained pointer value). The comparison is only needed when they
    // are the same type, otherwise they can't be the same raw_ptr object.
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if constexpr (std::is_same_v<raw_ptr, std::decay_t<decltype(ptr)>>) {
      PA_RAW_PTR_CHECK(this != &ptr);
    }
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
    if constexpr (kZeroOnMove) {
      ptr.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  PA_ALWAYS_INLINE constexpr T* get() const { return GetForExtraction(); }

  PA_ALWAYS_INLINE constexpr explicit operator bool() const {
    return !!wrapped_ptr_;
  }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  PA_ALWAYS_INLINE constexpr U& operator*() const {
    return *GetForDereference();
  }
  PA_ALWAYS_INLINE constexpr T* operator->() const {
    return GetForDereference();
  }

  // Disables `(my_raw_ptr->*pmf)(...)` as a workaround for
  // the ICE in GCC parsing the code, reported at
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103455
  template <typename PMF>
  void operator->*(PMF) const = delete;

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE constexpr operator T*() const { return GetForExtraction(); }
  template <typename U>
  PA_ALWAYS_INLINE constexpr explicit operator U*() const {
    // This operator may be invoked from static_cast, meaning the types may not
    // be implicitly convertible, hence the need for static_cast here.
    return static_cast<U*>(GetForExtraction());
  }

  PA_ALWAYS_INLINE constexpr raw_ptr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, 1);
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr& operator--() {
    wrapped_ptr_ = Impl::Retreat(wrapped_ptr_, 1);
    return *this;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr operator++(int /* post_increment */) {
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  PA_ALWAYS_INLINE constexpr raw_ptr operator--(int /* post_decrement */) {
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  template <
      typename Z,
      typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator+=(Z delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems);
    return *this;
  }
  template <
      typename Z,
      typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
  PA_ALWAYS_INLINE constexpr raw_ptr& operator-=(Z delta_elems) {
    wrapped_ptr_ = Impl::Retreat(wrapped_ptr_, delta_elems);
    return *this;
  }

  // Do not disable operator+() and operator-().
  // They provide OOB checks, which prevent from assigning an arbitrary value to
  // raw_ptr, leading BRP to modifying arbitrary memory thinking it's ref-count.
  // Keep them enabled, which may be blocked later when attempting to apply the
  // += or -= operation, when disabled. In the absence of operators +/-, the
  // compiler is free to implicitly convert to the underlying T* representation
  // and perform ordinary pointer arithmetic, thus invalidating the purpose
  // behind disabling them.
  template <typename Z>
  PA_ALWAYS_INLINE friend constexpr raw_ptr operator+(const raw_ptr& p,
                                                      Z delta_elems) {
    raw_ptr result = p;
    return result += delta_elems;
  }
  template <typename Z>
  PA_ALWAYS_INLINE friend constexpr raw_ptr operator-(const raw_ptr& p,
                                                      Z delta_elems) {
    raw_ptr result = p;
    return result -= delta_elems;
  }

  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr& p1,
                                                        const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2.wrapped_ptr_);
  }
  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(T* p1,
                                                        const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1, p2.wrapped_ptr_);
  }
  PA_ALWAYS_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr& p1,
                                                        T* p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2);
  }

  // Stop referencing the underlying pointer and free its memory. Compared to
  // raw delete calls, this avoids the raw_ptr to be temporarily dangling
  // during the free operation, which will lead to taking the slower path that
  // involves quarantine.
  PA_ALWAYS_INLINE constexpr void ClearAndDelete() noexcept {
    delete GetForExtractionAndReset();
  }
  PA_ALWAYS_INLINE constexpr void ClearAndDeleteArray() noexcept {
    delete[] GetForExtractionAndReset();
  }

  // Clear the underlying pointer and return another raw_ptr instance
  // that is allowed to dangle.
  // This can be useful in cases such as:
  // ```
  //  ptr.ExtractAsDangling()->SelfDestroy();
  // ```
  // ```
  //  c_style_api_do_something_and_destroy(ptr.ExtractAsDangling());
  // ```
  // NOTE, avoid using this method as it indicates an error-prone memory
  // ownership pattern. If possible, use smart pointers like std::unique_ptr<>
  // instead of raw_ptr<>.
  // If you have to use it, avoid saving the return value in a long-lived
  // variable (or worse, a field)! It's meant to be used as a temporary, to be
  // passed into a cleanup & freeing function, and destructed at the end of the
  // statement.
  PA_ALWAYS_INLINE constexpr MayBeDangling<T, Traits>
  ExtractAsDangling() noexcept {
    MayBeDangling<T, Traits> res(std::move(*this));
    // Not all implementation clear the source pointer on move. Furthermore,
    // even for implemtantions that do, cross-kind conversions (that add
    // kMayDangle) fall back to a copy, instead of move. So do it here just in
    // case. Should be cheap.
    operator=(nullptr);
    return res;
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
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator==(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator!=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator<(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator>(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator<=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);
  template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
  friend bool operator>=(const raw_ptr<U, R1>& lhs, const raw_ptr<V, R2>& rhs);

  // Comparisons with U*. These operators also handle the case where the RHS is
  // T*.
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator==(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() < rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() <= rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() > rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() >= rhs;
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<(U* lhs, const raw_ptr& rhs) {
    return lhs < rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator<=(U* lhs, const raw_ptr& rhs) {
    return lhs <= rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>(U* lhs, const raw_ptr& rhs) {
    return lhs > rhs.GetForComparison();
  }
  template <typename U>
  PA_ALWAYS_INLINE friend bool operator>=(U* lhs, const raw_ptr& rhs) {
    return lhs >= rhs.GetForComparison();
  }

  // Comparisons with `std::nullptr_t`.
  PA_ALWAYS_INLINE friend bool operator==(const raw_ptr& lhs, std::nullptr_t) {
    return !lhs;
  }
  PA_ALWAYS_INLINE friend bool operator!=(const raw_ptr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  PA_ALWAYS_INLINE friend bool operator==(std::nullptr_t, const raw_ptr& rhs) {
    return !rhs;
  }
  PA_ALWAYS_INLINE friend bool operator!=(std::nullptr_t, const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  PA_ALWAYS_INLINE friend constexpr void swap(raw_ptr& lhs,
                                              raw_ptr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

  PA_ALWAYS_INLINE void ReportIfDangling() const noexcept {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    Impl::ReportIfDangling(wrapped_ptr_);
#endif
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  PA_ALWAYS_INLINE constexpr T* GetForDereference() const {
    return Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_);
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  PA_ALWAYS_INLINE constexpr T* GetForExtraction() const {
    return Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  PA_ALWAYS_INLINE constexpr T* GetForComparison() const {
    return Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_);
  }

  PA_ALWAYS_INLINE constexpr T* GetForExtractionAndReset() {
    T* ptr = GetForExtraction();
    operator=(nullptr);
    return ptr;
  }

  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #union, #global-scope, #constexpr-ctor-field-initializer
  RAW_PTR_EXCLUSION T* wrapped_ptr_;

  template <typename U, base::RawPtrTraits R>
  friend class raw_ptr;
};

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator==(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() == rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator!=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return !(lhs == rhs);
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<(const raw_ptr<U, Traits1>& lhs,
                                const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() < rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>(const raw_ptr<U, Traits1>& lhs,
                                const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() > rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator<=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() <= rhs.GetForComparison();
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
PA_ALWAYS_INLINE bool operator>=(const raw_ptr<U, Traits1>& lhs,
                                 const raw_ptr<V, Traits2>& rhs) {
  return lhs.GetForComparison() >= rhs.GetForComparison();
}

template <typename T>
struct IsRawPtr : std::false_type {};

template <typename T, RawPtrTraits Traits>
struct IsRawPtr<raw_ptr<T, Traits>> : std::true_type {};

template <typename T>
inline constexpr bool IsRawPtrV = IsRawPtr<T>::value;

template <typename T>
inline constexpr bool IsRawPtrMayDangleV = false;

template <typename T, RawPtrTraits Traits>
inline constexpr bool IsRawPtrMayDangleV<raw_ptr<T, Traits>> =
    raw_ptr_traits::Contains(Traits, RawPtrTraits::kMayDangle);

// Template helpers for working with T* or raw_ptr<T>.
template <typename T>
struct IsPointer : std::false_type {};

template <typename T>
struct IsPointer<T*> : std::true_type {};

template <typename T, RawPtrTraits Traits>
struct IsPointer<raw_ptr<T, Traits>> : std::true_type {};

template <typename T>
inline constexpr bool IsPointerV = IsPointer<T>::value;

template <typename T>
struct RemovePointer {
  using type = T;
};

template <typename T>
struct RemovePointer<T*> {
  using type = T;
};

template <typename T, RawPtrTraits Traits>
struct RemovePointer<raw_ptr<T, Traits>> {
  using type = T;
};

template <typename T>
using RemovePointerT = typename RemovePointer<T>::type;

}  // namespace base

using base::raw_ptr;

// DisableDanglingPtrDetection option for raw_ptr annotates
// "intentional-and-safe" dangling pointers. It is meant to be used at the
// margin, only if there is no better way to re-architecture the code.
//
// Usage:
// raw_ptr<T, DisableDanglingPtrDetection> dangling_ptr;
//
// When using it, please provide a justification about what guarantees that it
// will never be dereferenced after becoming dangling.
constexpr auto DisableDanglingPtrDetection = base::RawPtrTraits::kMayDangle;

// See `docs/dangling_ptr.md`
// Annotates known dangling raw_ptr. Those haven't been triaged yet. All the
// occurrences are meant to be removed. See https://crbug.com/1291138.
constexpr auto DanglingUntriaged = base::RawPtrTraits::kMayDangle;

// Unlike DanglingUntriaged, this annotates raw_ptrs that are known to
// dangle only occasionally on the CQ.
//
// These were found from CQ runs and analysed in this dashboard:
// https://docs.google.com/spreadsheets/d/1k12PQOG4y1-UEV9xDfP1F8FSk4cVFywafEYHmzFubJ8/
constexpr auto FlakyDanglingUntriaged = base::RawPtrTraits::kMayDangle;

// The use of pointer arithmetic with raw_ptr is strongly discouraged and
// disabled by default. Usually a container like span<> should be used
// instead of the raw_ptr.
constexpr auto AllowPtrArithmetic = base::RawPtrTraits::kAllowPtrArithmetic;

// Temporary flag for `raw_ptr` / `raw_ref`. This is used by finch experiments
// to differentiate pointers added recently for the ChromeOS ash rewrite.
//
// See launch plan:
// https://docs.google.com/document/d/105OVhNl-2lrfWElQSk5BXYv-nLynfxUrbC4l8cZ0CoU/edit
//
// This is not meant to be added manually. You can ignore this flag.
constexpr auto ExperimentalAsh = base::RawPtrTraits::kExperimentalAsh;

namespace std {

// Override so set/map lookups do not create extra raw_ptr. This also allows
// dangling pointers to be used for lookup.
template <typename T, base::RawPtrTraits Traits>
struct less<raw_ptr<T, Traits>> {
  using Impl = typename raw_ptr<T, Traits>::Impl;
  using is_transparent = void;

  bool operator()(const raw_ptr<T, Traits>& lhs,
                  const raw_ptr<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(T* lhs, const raw_ptr<T, Traits>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(const raw_ptr<T, Traits>& lhs, T* rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }
};

// Define for cases where raw_ptr<T> holds a pointer to an array of type T.
// This is consistent with definition of std::iterator_traits<T*>.
// Algorithms like std::binary_search need that.
template <typename T, base::RawPtrTraits Traits>
struct iterator_traits<raw_ptr<T, Traits>> {
  using difference_type = ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;
};

#if defined(_LIBCPP_VERSION)
// Specialize std::pointer_traits. The latter is required to obtain the
// underlying raw pointer in the std::to_address(pointer) overload.
// Implementing the pointer_traits is the standard blessed way to customize
// `std::to_address(pointer)` in C++20 [3].
//
// [1] https://wg21.link/pointer.traits.optmem

template <typename T, ::base::RawPtrTraits Traits>
struct pointer_traits<::raw_ptr<T, Traits>> {
  using pointer = ::raw_ptr<T, Traits>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = ::raw_ptr<U, Traits>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(&r);
  }

  static constexpr element_type* to_address(pointer p) noexcept {
    return p.get();
  }
};
#endif  // defined(_LIBCPP_VERSION)

}  // namespace std

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_POINTERS_RAW_PTR_H_
