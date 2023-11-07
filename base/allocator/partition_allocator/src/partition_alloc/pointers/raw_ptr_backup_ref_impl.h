// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_BACKUP_REF_IMPL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_BACKUP_REF_IMPL_H_

#include <stddef.h>

#include <type_traits>

#include "base/allocator/partition_allocator/src/partition_alloc/chromeos_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_address_space.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/cxx20_is_constant_evaluated.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_config.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/src/partition_alloc/tagging.h"
#include "build/build_config.h"

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager_bitmap.h"
#endif

#if !BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#error "Included under wrong build option"
#endif

namespace base::internal {

#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
PA_COMPONENT_EXPORT(RAW_PTR)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address);
#endif

class BackupRefPtrGlobalSettings {
 public:
  static void EnableExperimentalAsh() {
    PA_CHECK(!experimental_ash_raw_ptr_enabled_);
    experimental_ash_raw_ptr_enabled_ = true;
  }

  static void DisableExperimentalAshForTest() {
    PA_CHECK(experimental_ash_raw_ptr_enabled_);
    experimental_ash_raw_ptr_enabled_ = false;
  }

  PA_ALWAYS_INLINE static bool IsExperimentalAshEnabled() {
    return experimental_ash_raw_ptr_enabled_;
  }

 private:
  // Write-once settings that should be in its own cacheline, as they're
  // accessed frequently on a hot path.
  PA_ALIGNAS(partition_alloc::internal::kPartitionCachelineSize)
  static inline bool experimental_ash_raw_ptr_enabled_ = false;
  [[maybe_unused]] char
      padding_[partition_alloc::internal::kPartitionCachelineSize - 1];
};

// Note that `RawPtrBackupRefImpl` itself is not thread-safe. If multiple
// threads modify the same raw_ptr object without synchronization, a data race
// will occur.
template <bool AllowDangling = false, bool ExperimentalAsh = false>
struct RawPtrBackupRefImpl {
  // These are needed for correctness, or else we may end up manipulating
  // ref-count where we shouldn't, thus affecting the BRP's integrity. Unlike
  // the first two, kMustZeroOnDestruct wouldn't be needed if raw_ptr was used
  // correctly, but we already caught cases where a value is written after
  // destruction.
  static constexpr bool kMustZeroOnConstruct = true;
  static constexpr bool kMustZeroOnMove = true;
  static constexpr bool kMustZeroOnDestruct = true;

 private:
  PA_ALWAYS_INLINE static bool UseBrp(uintptr_t address) {
    // Pointer annotated with ExperimentalAsh are subject to a separate,
    // Ash-related experiment.
    //
    // Note that this can be enabled only before the BRP partition is created,
    // so it's impossible for this function to change its answer for a specific
    // pointer. (This relies on the original partition to not be BRP-enabled.)
    if constexpr (ExperimentalAsh) {
#if BUILDFLAG(PA_IS_CHROMEOS_ASH)
      if (!BackupRefPtrGlobalSettings::IsExperimentalAshEnabled()) {
        return false;
      }
#endif
    }
    return partition_alloc::IsManagedByPartitionAllocBRPPool(address);
  }

  PA_ALWAYS_INLINE static bool IsSupportedAndNotNull(uintptr_t address) {
    // There are many situations where the compiler can prove that
    // `ReleaseWrappedPtr` is called on a value that is always nullptr, but the
    // way `IsManagedByPartitionAllocBRPPool` is written, the compiler can't
    // prove that nullptr is not managed by PartitionAlloc; and so the compiler
    // has to emit a useless check and dead code. To avoid that without making
    // the runtime check slower, tell the compiler to skip
    // `IsManagedByPartitionAllocBRPPool` when it can statically determine that
    // address is nullptr.
#if PA_HAS_BUILTIN(__builtin_constant_p)
    if (__builtin_constant_p(address == 0) && (address == 0)) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      PA_BASE_CHECK(
          !partition_alloc::IsManagedByPartitionAllocBRPPool(address));
#endif  // BUILDFLAG(PA_DCHECK_IS_ON) ||
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      return false;
    }
#endif  // PA_HAS_BUILTIN(__builtin_constant_p)

    // This covers the nullptr case, as address 0 is never in any
    // PartitionAlloc pool.
    bool use_brp = UseBrp(address);

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
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    if (use_brp) {
      CheckThatAddressIsntWithinFirstPartitionPage(address);
    }
#endif

    return use_brp;
  }

#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  // Out-Of-Bounds (OOB) poison bit is set when the pointer has overflowed by
  // one byte.
#if defined(ARCH_CPU_X86_64)
  // Bit 63 is the only pointer bit that will work as the poison bit across both
  // LAM48 and LAM57. It also works when all unused linear address bits are
  // checked for canonicality.
  static constexpr uintptr_t OOB_POISON_BIT = static_cast<uintptr_t>(1) << 63;
#else
  // Avoid ARM's Top-Byte Ignore.
  static constexpr uintptr_t OOB_POISON_BIT = static_cast<uintptr_t>(1) << 55;
#endif

  template <typename T>
  PA_ALWAYS_INLINE static T* UnpoisonPtr(T* ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) &
                                ~OOB_POISON_BIT);
  }

  template <typename T>
  PA_ALWAYS_INLINE static bool IsPtrOOB(T* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & OOB_POISON_BIT) ==
           OOB_POISON_BIT;
  }

  template <typename T>
  PA_ALWAYS_INLINE static T* PoisonOOBPtr(T* ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) |
                                OOB_POISON_BIT);
  }
#else   // BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  template <typename T>
  PA_ALWAYS_INLINE static T* UnpoisonPtr(T* ptr) {
    return ptr;
  }
#endif  // BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)

 public:
  // Wraps a pointer.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtr(T* ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return ptr;
    }
    uintptr_t address = partition_alloc::UntagPtr(UnpoisonPtr(ptr));
    if (IsSupportedAndNotNull(address)) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      PA_BASE_CHECK(ptr != nullptr);
#endif
      AcquireInternal(address);
    } else {
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#if PA_HAS_BUILTIN(__builtin_constant_p)
      // Similarly to `IsSupportedAndNotNull` above, elide the
      // `BanSuperPageFromBRPPool` call if the compiler can prove that `address`
      // is zero since PA won't be able to map anything at that address anyway.
      bool known_constant_zero =
          __builtin_constant_p(address == 0) && (address == 0);
#else   // PA_HAS_BUILTIN(__builtin_constant_p)
      bool known_constant_zero = false;
#endif  // PA_HAS_BUILTIN(__builtin_constant_p)

      if (!known_constant_zero) {
        partition_alloc::internal::AddressPoolManagerBitmap::
            BanSuperPageFromBRPPool(address);
      }
#endif  // !BUILDFLAG(HAS_64_BIT_POINTERS)
    }

    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr void ReleaseWrappedPtr(T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return;
    }
    uintptr_t address = partition_alloc::UntagPtr(UnpoisonPtr(wrapped_ptr));
    if (IsSupportedAndNotNull(address)) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      PA_BASE_CHECK(wrapped_ptr != nullptr);
#endif
      ReleaseInternal(address);
    }
    // We are unable to counteract BanSuperPageFromBRPPool(), called from
    // WrapRawPtr(). We only use one bit per super-page and, thus can't tell if
    // there's more than one associated raw_ptr<T> at a given time. The risk of
    // exhausting the entire address space is minuscule, therefore, we couldn't
    // resist the perf gain of a single relaxed store (in the above mentioned
    // function) over much more expensive two CAS operations, which we'd have to
    // use if we were to un-ban a super-page.
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForDereference(
      T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr;
    }
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
    PA_BASE_CHECK(!IsPtrOOB(wrapped_ptr));
#endif
    uintptr_t address = partition_alloc::UntagPtr(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
      PA_BASE_CHECK(wrapped_ptr != nullptr);
      PA_BASE_CHECK(IsPointeeAlive(address));
    }
#endif  // BUILDFLAG(PA_DCHECK_IS_ON) ||
        // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* SafelyUnwrapPtrForExtraction(
      T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr;
    }
    T* unpoisoned_ptr = UnpoisonPtr(wrapped_ptr);
#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
    // Some code uses invalid pointer values as indicators, so those values must
    // be passed through unchanged during extraction. The following check will
    // pass invalid values through if those values do not fall within the BRP
    // pool after being unpoisoned.
    if (!IsSupportedAndNotNull(partition_alloc::UntagPtr(unpoisoned_ptr))) {
      return wrapped_ptr;
    }
    // Poison-based OOB checks do not extend to extracted pointers. The
    // alternative of retaining poison on extracted pointers could introduce new
    // OOB conditions, e.g., in code that extracts an end-of-allocation pointer
    // for use in a loop termination condition. The poison bit would make that
    // pointer appear to reference a very high address.
#endif  // BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
    return unpoisoned_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForComparison(
      T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr;
    }
    // This may be used for unwrapping an end-of-allocation pointer to be used
    // as an endpoint in an iterative algorithm, so this removes the OOB poison
    // bit.
    return UnpoisonPtr(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  PA_ALWAYS_INLINE static constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible_v<From*, To*>,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Verify the pointer stayed in the same slot, and return the poisoned version
  // of `new_ptr` if OOB poisoning is enabled.
  template <typename T>
  PA_ALWAYS_INLINE static T* VerifyAndPoisonPointerAfterAdvanceOrRetreat(
      T* unpoisoned_ptr,
      T* new_ptr) {
    // In the "before allocation" mode, on 32-bit, we can run into a problem
    // that the end-of-allocation address could fall outside of
    // PartitionAlloc's pools, if this is the last slot of the super page,
    // thus pointing to the guard page. This means the ref-count won't be
    // decreased when the pointer is released (leak).
    //
    // We could possibly solve it in a few different ways:
    // - Add the trailing guard page to the pool, but we'd have to think very
    //   hard if this doesn't create another hole.
    // - Add an address adjustment to "is in pool?" check, similar as the one in
    //   PartitionAllocGetSlotStartInBRPPool(), but that seems fragile, not to
    //   mention adding an extra instruction to an inlined hot path.
    // - Let the leak happen, since it should a very rare condition.
    // - Go back to the previous solution of rewrapping the pointer, but that
    //   had an issue of losing BRP protection in case the pointer ever gets
    //   shifted back before the end of allocation.
    //
    // We decided to cross that bridge once we get there... if we ever get
    // there. Currently there are no plans to switch back to the "before
    // allocation" mode.
    //
    // This problem doesn't exist in the "previous slot" mode, or any mode that
    // involves putting extras after the allocation, because the
    // end-of-allocation address belongs to the same slot.
    static_assert(BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT));

    // First check if the new address didn't migrate in/out the BRP pool, and
    // that it lands within the same allocation. An end-of-allocation address is
    // ok, too, and that may lead to the pointer being poisoned if the relevant
    // feature is enabled. These checks add a non-trivial cost, but they're
    // cheaper and more secure than the previous implementation that rewrapped
    // the pointer (wrapped the new pointer and unwrapped the old one).
    //
    // Note, the value of these checks goes beyond OOB protection. They're
    // important for integrity of the BRP algorithm. Without these, an attacker
    // could make the pointer point to another allocation, and cause its
    // ref-count to go to 0 upon this pointer's destruction, even though there
    // may be another pointer still pointing to it, thus making it lose the BRP
    // protection prematurely.
    const uintptr_t before_addr = partition_alloc::UntagPtr(unpoisoned_ptr);
    const uintptr_t after_addr = partition_alloc::UntagPtr(new_ptr);
    // TODO(bartekn): Consider adding support for non-BRP pools too (without
    // removing the cross-pool migration check).
    if (IsSupportedAndNotNull(before_addr)) {
      constexpr size_t size = sizeof(T);
      [[maybe_unused]] const bool is_end =
          CheckPointerWithinSameAlloc(before_addr, after_addr, size);
#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
      if (is_end) {
        new_ptr = PoisonOOBPtr(new_ptr);
      }
#endif  // BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
    } else {
      // Check that the new address didn't migrate into the BRP pool, as it
      // would result in more pointers pointing to an allocation than its
      // ref-count reflects.
      PA_BASE_CHECK(!IsSupportedAndNotNull(after_addr));
    }
    return new_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Advance(T* wrapped_ptr, Z delta_elems) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr + delta_elems;
    }
    T* unpoisoned_ptr = UnpoisonPtr(wrapped_ptr);
    return VerifyAndPoisonPointerAfterAdvanceOrRetreat(
        unpoisoned_ptr, unpoisoned_ptr + delta_elems);
  }

  // Retreat the wrapped pointer by `delta_elems`.
  template <
      typename T,
      typename Z,
      typename =
          std::enable_if_t<partition_alloc::internal::is_offset_type<Z>, void>>
  PA_ALWAYS_INLINE static constexpr T* Retreat(T* wrapped_ptr, Z delta_elems) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr - delta_elems;
    }
    T* unpoisoned_ptr = UnpoisonPtr(wrapped_ptr);
    return VerifyAndPoisonPointerAfterAdvanceOrRetreat(
        unpoisoned_ptr, unpoisoned_ptr - delta_elems);
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                            T* wrapped_ptr2) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr1 - wrapped_ptr2;
    }

    T* unpoisoned_ptr1 = UnpoisonPtr(wrapped_ptr1);
    T* unpoisoned_ptr2 = UnpoisonPtr(wrapped_ptr2);
#if BUILDFLAG(ENABLE_POINTER_SUBTRACTION_CHECK)
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return unpoisoned_ptr1 - unpoisoned_ptr2;
    }
    uintptr_t address1 = partition_alloc::UntagPtr(unpoisoned_ptr1);
    uintptr_t address2 = partition_alloc::UntagPtr(unpoisoned_ptr2);
    // Ensure that both pointers are within the same slot, and pool!
    // TODO(bartekn): Consider adding support for non-BRP pool too.
    if (IsSupportedAndNotNull(address1)) {
      PA_BASE_CHECK(IsSupportedAndNotNull(address2));
      PA_BASE_CHECK(partition_alloc::internal::IsPtrWithinSameAlloc(
                        address2, address1, sizeof(T)) !=
                    partition_alloc::internal::PtrPosWithinAlloc::kFarOOB);
    } else {
      PA_BASE_CHECK(!IsSupportedAndNotNull(address2));
    }
#endif  // BUILDFLAG(ENABLE_POINTER_SUBTRACTION_CHECK)
    return unpoisoned_ptr1 - unpoisoned_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  // This method increments the reference count of the allocation slot.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* Duplicate(T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr;
    }
    return WrapRawPtr(wrapped_ptr);
  }

  // Report the current wrapped pointer if pointee isn't alive anymore.
  template <typename T>
  PA_ALWAYS_INLINE static void ReportIfDangling(T* wrapped_ptr) {
    ReportIfDanglingInternal(partition_alloc::UntagPtr(wrapped_ptr));
  }

  // `WrapRawPtrForDuplication` and `UnsafelyUnwrapPtrForDuplication` are used
  // to create a new raw_ptr<T> from another raw_ptr<T> of a different flavor.
  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* WrapRawPtrForDuplication(T* ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return ptr;
    } else {
      return WrapRawPtr(ptr);
    }
  }

  template <typename T>
  PA_ALWAYS_INLINE static constexpr T* UnsafelyUnwrapPtrForDuplication(
      T* wrapped_ptr) {
    if (partition_alloc::internal::base::is_constant_evaluated()) {
      return wrapped_ptr;
    } else {
      return UnpoisonPtr(wrapped_ptr);
    }
  }

  // This is for accounting only, used by unit tests.
  PA_ALWAYS_INLINE static constexpr void IncrementSwapCountForTest() {}
  PA_ALWAYS_INLINE static constexpr void IncrementLessCountForTest() {}

 private:
  // We've evaluated several strategies (inline nothing, various parts, or
  // everything in |Wrap()| and |Release()|) using the Speedometer2 benchmark
  // to measure performance. The best results were obtained when only the
  // lightweight |IsManagedByPartitionAllocBRPPool()| check was inlined.
  // Therefore, we've extracted the rest into the functions below and marked
  // them as PA_NOINLINE to prevent unintended LTO effects.
  PA_NOINLINE static PA_COMPONENT_EXPORT(RAW_PTR) void AcquireInternal(
      uintptr_t address);
  PA_NOINLINE static PA_COMPONENT_EXPORT(RAW_PTR) void ReleaseInternal(
      uintptr_t address);
  PA_NOINLINE static PA_COMPONENT_EXPORT(RAW_PTR) bool IsPointeeAlive(
      uintptr_t address);
  PA_NOINLINE static PA_COMPONENT_EXPORT(RAW_PTR) void ReportIfDanglingInternal(
      uintptr_t address);

  // CHECK if `before_addr` and `after_addr` are in the same allocation, for a
  // given `type_size`.
  // If BACKUP_REF_PTR_POISON_OOB_PTR is enabled, return whether the allocation
  // is at the end.
  // If BACKUP_REF_PTR_POISON_OOB_PTR is disable, return false.
  PA_NOINLINE static PA_COMPONENT_EXPORT(
      RAW_PTR) bool CheckPointerWithinSameAlloc(uintptr_t before_addr,
                                                uintptr_t after_addr,
                                                size_t type_size);
};

}  // namespace base::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_POINTERS_RAW_PTR_BACKUP_REF_IMPL_H_
