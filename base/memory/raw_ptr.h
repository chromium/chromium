// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_H_
#define BASE_MEMORY_RAW_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <climits>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/allocator/partition_allocator/partition_tag_types.h"
#include "base/allocator/partition_allocator/tagging.h"
#endif  // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
// USE_BACKUP_REF_PTR implies USE_PARTITION_ALLOC, needed for code under
// allocator/partition_allocator/ to be built.
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

#if BUILDFLAG(IS_WIN)
#include "base/allocator/partition_allocator/partition_alloc_base/win/win_handle_types.h"
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/partition_alloc_base/check.h"
// Live implementation of MiraclePtr being built.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) ||       \
    defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
#define PA_RAW_PTR_CHECK(condition) PA_BASE_CHECK(condition)
#else
// No-op implementation of MiraclePtr being built.
// Note that `PA_BASE_DCHECK()` evaporates from non-DCHECK builds,
// minimizing impact of generated code.
#define PA_RAW_PTR_CHECK(condition) PA_BASE_DCHECK(condition)
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
        // || defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
#else   // BUILDFLAG(USE_PARTITION_ALLOC
// Without PartitionAlloc, there's no `PA_BASE_D?CHECK()` implementation
// available.
#define PA_RAW_PTR_CHECK(condition)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

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

// The following types are the different RawPtrType template option possible for
// a `raw_ptr`:
// - RawPtrMayDangle disables dangling pointers check when the object is
//   released.
// - RawPtrBanDanglingIfSupported may enable dangling pointers check on object
//   destruction.
//
// We describe those types here so that they can be used outside of `raw_ptr` as
// object markers, and their meaning might vary depending on where those markers
// are being used. For instance, we are using those in `UnretainedWrapper` to
// change behavior depending on RawPtrType.
struct RawPtrMayDangle {};
struct RawPtrBanDanglingIfSupported {};

struct RawPtrNoOp {};

namespace raw_ptr_traits {
template <typename T>
struct RawPtrTypeToImpl;
}

namespace internal {
// These classes/structures are part of the raw_ptr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

// This type trait verifies a type can be used as a pointer offset.
//
// We support pointer offsets in signed (ptrdiff_t) or unsigned (size_t) values.
// Smaller types are also allowed.
template <typename Z>
static constexpr bool offset_type =
    std::is_integral_v<Z> && sizeof(Z) <= sizeof(ptrdiff_t);

struct RawPtrNoOpImpl {
  // Wraps a pointer.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <typename T,
            typename Z,
            typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}
};

#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

constexpr int kValidAddressBits = 48;
constexpr uintptr_t kAddressMask = (1ull << kValidAddressBits) - 1;
constexpr int kTagBits = sizeof(uintptr_t) * 8 - kValidAddressBits;

// MTECheckedPtr has no business with the topmost bits reserved for the
// tag used by true ARM MTE, so we strip it out here.
constexpr uintptr_t kTagMask =
    ~kAddressMask & partition_alloc::internal::kPtrUntagMask;

constexpr int kTopBitShift = 63;
constexpr uintptr_t kTopBit = 1ull << kTopBitShift;
static_assert(kTopBit << 1 == 0, "kTopBit should really be the top bit");
static_assert((kTopBit & kTagMask) > 0,
              "kTopBit bit must be inside the tag region");

// This functionality is outside of MTECheckedPtrImpl, so that it can be
// overridden by tests.
struct MTECheckedPtrImplPartitionAllocSupport {
  // Checks if the necessary support is enabled in PartitionAlloc for `ptr`.
  template <typename T>
  static PA_ALWAYS_INLINE bool EnabledForPtr(T* ptr) {
    // Disambiguation: UntagPtr removes the hardware MTE tag, whereas this class
    // is responsible for handling the software MTE tag.
    auto addr = partition_alloc::UntagPtr(ptr);
    return partition_alloc::IsManagedByPartitionAlloc(addr);
  }

  // Returns pointer to the tag that protects are pointed by |addr|.
  static PA_ALWAYS_INLINE void* TagPointer(uintptr_t addr) {
    return partition_alloc::PartitionTagPointer(addr);
  }
};

template <typename PartitionAllocSupport>
struct MTECheckedPtrImpl {
  // This implementation assumes that pointers are 64 bits long and at least 16
  // top bits are unused. The latter is harder to verify statically, but this is
  // true for all currently supported 64-bit architectures (PA_DCHECK when
  // wrapping will verify that).
  static_assert(sizeof(void*) >= 8, "Need 64-bit pointers");

  // Wraps a pointer, and returns its uintptr_t representation.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    // Disambiguation: UntagPtr removes the hardware MTE tag, whereas this
    // function is responsible for adding the software MTE tag.
    uintptr_t addr = partition_alloc::UntagPtr(ptr);
    PA_BASE_DCHECK(ExtractTag(addr) == 0ull);

    // Return a not-wrapped |addr|, if it's either nullptr or if the protection
    // for this pointer is disabled.
    if (!PartitionAllocSupport::EnabledForPtr(ptr)) {
      return ptr;
    }

    // Read the tag and place it in the top bits of the address.
    // Even if PartitionAlloc's tag has less than kTagBits, we'll read
    // what's given and pad the rest with 0s.
    static_assert(sizeof(partition_alloc::PartitionTag) * 8 <= kTagBits, "");
    uintptr_t tag = *(static_cast<volatile partition_alloc::PartitionTag*>(
        PartitionAllocSupport::TagPointer(addr)));
    PA_BASE_DCHECK(tag);

    tag <<= kValidAddressBits;
    addr |= tag;
    // See the disambiguation comment above.
    // TODO(kdlee): Ensure that ptr's hardware MTE tag is preserved.
    // TODO(kdlee): Ensure that hardware and software MTE tags don't conflict.
    return static_cast<T*>(partition_alloc::internal::TagAddr(addr));
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  // No-op for MTECheckedPtrImpl.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    // Disambiguation: UntagPtr removes the hardware MTE tag, whereas this
    // function is responsible for removing the software MTE tag.
    uintptr_t wrapped_addr = partition_alloc::UntagPtr(wrapped_ptr);
    uintptr_t tag = ExtractTag(wrapped_addr);
    if (tag > 0) {
      // Read the tag provided by PartitionAlloc.
      //
      // Cast to volatile to ensure memory is read. E.g. in a tight loop, the
      // compiler could cache the value in a register and thus could miss that
      // another thread freed memory and changed tag.
      uintptr_t read_tag =
          *static_cast<volatile partition_alloc::PartitionTag*>(
              PartitionAllocSupport::TagPointer(ExtractAddress(wrapped_addr)));
      if (PA_UNLIKELY(tag != read_tag))
        PA_IMMEDIATE_CRASH();
      // See the disambiguation comment above.
      // TODO(kdlee): Ensure that ptr's hardware MTE tag is preserved.
      // TODO(kdlee): Ensure that hardware and software MTE tags don't conflict.
      return static_cast<T*>(
          partition_alloc::internal::TagAddr(ExtractAddress(wrapped_addr)));
    }
    return wrapped_ptr;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    // SafelyUnwrapPtrForDereference handles nullptr case well.
    return SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return ExtractPtr(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");

    // The top-bit tag must not affect the result of upcast.
    return static_cast<To*>(wrapped_ptr);
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <typename T,
            typename Z,
            typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    // Ensure that both pointers come from the same allocation.
    //
    // Disambiguation: UntagPtr removes the hardware MTE tag, whereas this
    // class is responsible for handling the software MTE tag.
    //
    // MTECheckedPtr doesn't use 0 as a valid tag; depending on which
    // subtraction operator is called, we may be getting the actual
    // untagged T* or the wrapped pointer (passed as a T*) in one or
    // both args. We can only check slot cohabitation when both args
    // come with tags.
    const uintptr_t tag1 = ExtractTag(partition_alloc::UntagPtr(wrapped_ptr1));
    const uintptr_t tag2 = ExtractTag(partition_alloc::UntagPtr(wrapped_ptr2));
    if (tag1 && tag2) {
      PA_BASE_CHECK(tag1 == tag2);
      return wrapped_ptr1 - wrapped_ptr2;
    }

    // If one or the other arg come untagged, we have to perform the
    // subtraction entirely without tags.
    return reinterpret_cast<T*>(
               ExtractAddress(partition_alloc::UntagPtr(wrapped_ptr1))) -
           reinterpret_cast<T*>(
               ExtractAddress(partition_alloc::UntagPtr(wrapped_ptr2)));
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  static PA_ALWAYS_INLINE uintptr_t ExtractAddress(uintptr_t wrapped_ptr) {
    return wrapped_ptr & kAddressMask;
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* ExtractPtr(T* wrapped_ptr) {
    // Disambiguation: UntagPtr/TagAddr handle the hardware MTE tag, whereas
    // this function is responsible for removing the software MTE tag.
    // TODO(kdlee): Ensure that wrapped_ptr's hardware MTE tag is preserved.
    // TODO(kdlee): Ensure that hardware and software MTE tags don't conflict.
    return static_cast<T*>(partition_alloc::internal::TagAddr(
        ExtractAddress(partition_alloc::UntagPtr(wrapped_ptr))));
  }

  static PA_ALWAYS_INLINE uintptr_t ExtractTag(uintptr_t wrapped_ptr) {
    return (wrapped_ptr & kTagMask) >> kValidAddressBits;
  }
};

#endif  // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
PA_COMPONENT_EXPORT(RAW_PTR)
void CheckThatAddressIsntWithinFirstPartitionPage(uintptr_t address);
#endif

template <bool AllowDangling = false>
struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  static PA_ALWAYS_INLINE bool IsSupportedAndNotNull(uintptr_t address) {
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
    bool is_in_brp_pool =
        partition_alloc::IsManagedByPartitionAllocBRPPool(address);

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
    if (is_in_brp_pool) {
      CheckThatAddressIsntWithinFirstPartitionPage(address);
    }
#endif

    return is_in_brp_pool;
  }

#if defined(PA_USE_OOB_POISON)
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
  static PA_ALWAYS_INLINE T* UnpoisonPtr(T* ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) &
                                ~OOB_POISON_BIT);
  }

  template <typename T>
  static PA_ALWAYS_INLINE bool IsPtrOOB(T* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) & OOB_POISON_BIT) ==
           OOB_POISON_BIT;
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* PoisonOOBPtr(T* ptr) {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) |
                                OOB_POISON_BIT);
  }
#else   // PA_USE_OOB_POISON
  template <typename T>
  static PA_ALWAYS_INLINE T* UnpoisonPtr(T* ptr) {
    return ptr;
  }
#endif  // PA_USE_OOB_POISON

  // Wraps a pointer.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    uintptr_t address = partition_alloc::UntagPtr(UnpoisonPtr(ptr));
    if (IsSupportedAndNotNull(address)) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
      PA_BASE_CHECK(ptr != nullptr);
#endif
      AcquireInternal(address);
    } else {
#if !defined(PA_HAS_64_BITS_POINTERS)
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
#endif  // !defined(PA_HAS_64_BITS_POINTERS)
    }

    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T* wrapped_ptr) {
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
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
#if defined(PA_USE_OOB_POISON)
    PA_BASE_CHECK(!IsPtrOOB(wrapped_ptr));
#endif
    uintptr_t address = partition_alloc::UntagPtr(wrapped_ptr);
    if (IsSupportedAndNotNull(address)) {
      PA_BASE_CHECK(wrapped_ptr != nullptr);
      PA_BASE_CHECK(IsPointeeAlive(address));
    }
#endif
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    T* unpoisoned_ptr = UnpoisonPtr(wrapped_ptr);
#if defined(PA_USE_OOB_POISON)
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
#endif
    return unpoisoned_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    // This may be used for unwrapping an end-of-allocation pointer to be used
    // as an endpoint in an iterative algorithm, so this removes the OOB poison
    // bit.
    return UnpoisonPtr(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <typename T,
            typename Z,
            typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
    T* unpoisoned_ptr = UnpoisonPtr(wrapped_ptr);
    T* new_ptr = unpoisoned_ptr + delta_elems;
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
    uintptr_t address = partition_alloc::UntagPtr(unpoisoned_ptr);
    // TODO(bartekn): Consider adding support for non-BRP pools too (without
    // removing the cross-pool migration check).
    if (IsSupportedAndNotNull(address)) {
      auto ptr_pos_within_alloc =
          IsValidDelta(address, delta_elems * static_cast<Z>(sizeof(T)));
      // No need to check that |new_ptr| is in the same pool, as IsValidDeta()
      // checks that it's within the same allocation, so must be the same pool.
      PA_BASE_CHECK(ptr_pos_within_alloc !=
                    partition_alloc::PtrPosWithinAlloc::kFarOOB);
#if defined(PA_USE_OOB_POISON)
      if (ptr_pos_within_alloc == partition_alloc::PtrPosWithinAlloc::kAllocEnd)
        new_ptr = PoisonOOBPtr(new_ptr);
#endif
    } else {
      // Check that the new address didn't migrate into the BRP pool, as it
      // would result in more pointers pointing to an allocation than its
      // ref-count reflects.
      PA_BASE_CHECK(!IsSupportedAndNotNull(partition_alloc::UntagPtr(new_ptr)));
    }
    return new_ptr;
#else   // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
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
    static_assert(false);
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    T* unpoisoned_ptr1 = UnpoisonPtr(wrapped_ptr1);
    T* unpoisoned_ptr2 = UnpoisonPtr(wrapped_ptr2);
    uintptr_t address1 = partition_alloc::UntagPtr(unpoisoned_ptr1);
    uintptr_t address2 = partition_alloc::UntagPtr(unpoisoned_ptr2);
    // Ensure that both pointers are within the same slot, and pool!
    // TODO(bartekn): Consider adding support for non-BRP pool too.
    if (IsSupportedAndNotNull(address1)) {
      PA_BASE_CHECK(IsSupportedAndNotNull(address2));
      PA_BASE_CHECK(IsValidDelta(address2, address1 - address2) !=
                    partition_alloc::PtrPosWithinAlloc::kFarOOB);
    } else {
      PA_BASE_CHECK(!IsSupportedAndNotNull(address2));
    }
    return unpoisoned_ptr1 - unpoisoned_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  // This method increments the reference count of the allocation slot.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return WrapRawPtr(wrapped_ptr);
  }

  // Report the current wrapped pointer if pointee isn't alive anymore.
  template <typename T>
  static PA_ALWAYS_INLINE void ReportIfDangling(T* wrapped_ptr) {
    ReportIfDanglingInternal(partition_alloc::UntagPtr(wrapped_ptr));
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  // We've evaluated several strategies (inline nothing, various parts, or
  // everything in |Wrap()| and |Release()|) using the Speedometer2 benchmark
  // to measure performance. The best results were obtained when only the
  // lightweight |IsManagedByPartitionAllocBRPPool()| check was inlined.
  // Therefore, we've extracted the rest into the functions below and marked
  // them as PA_NOINLINE to prevent unintended LTO effects.
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void AcquireInternal(uintptr_t address);
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void ReleaseInternal(uintptr_t address);
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      bool IsPointeeAlive(uintptr_t address);
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void ReportIfDanglingInternal(uintptr_t address);
  template <typename Z, typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE partition_alloc::PtrPosWithinAlloc IsValidDelta(
      uintptr_t address,
      Z delta_in_bytes) {
    if constexpr (std::is_signed_v<Z>)
      return IsValidSignedDelta(address, ptrdiff_t{delta_in_bytes});
    else
      return IsValidUnsignedDelta(address, size_t{delta_in_bytes});
  }
  static PA_COMPONENT_EXPORT(RAW_PTR)
      PA_NOINLINE partition_alloc::PtrPosWithinAlloc
      IsValidSignedDelta(uintptr_t address, ptrdiff_t delta_in_bytes);
  static PA_COMPONENT_EXPORT(RAW_PTR)
      PA_NOINLINE partition_alloc::PtrPosWithinAlloc
      IsValidUnsignedDelta(uintptr_t address, size_t delta_in_bytes);
};

#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
// Implementation that allows us to detect BackupRefPtr problems in ASan builds.
struct AsanBackupRefPtrImpl {
  // Wraps a pointer.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    AsanCheckIfValidInstantiation(ptr);
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T*) {}

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    AsanCheckIfValidDereference(wrapped_ptr);
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    AsanCheckIfValidExtraction(wrapped_ptr);
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <typename T,
            typename Z,
            typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}

 private:
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void AsanCheckIfValidInstantiation(void const volatile* ptr);
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void AsanCheckIfValidDereference(void const volatile* ptr);
  static PA_COMPONENT_EXPORT(RAW_PTR) PA_NOINLINE
      void AsanCheckIfValidExtraction(void const volatile* ptr);
};
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)

#if BUILDFLAG(USE_ASAN_UNOWNED_PTR)

struct AsanUnownedPtrImpl {
  // Wraps a pointer.
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    return ptr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T* wrapped_ptr) {
    ProbeForLowSeverityLifetimeIssue(wrapped_ptr);
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function is allowed to crash on nullptr.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    // ASAN will catch use of dereferenced ptr without additional probing.
    return wrapped_ptr;
  }

  // Unwraps the pointer, while asserting that memory hasn't been freed. The
  // function must handle nullptr gracefully.
  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    ProbeForLowSeverityLifetimeIssue(wrapped_ptr);
    return wrapped_ptr;
  }

  // Unwraps the pointer, without making an assertion on whether memory was
  // freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static PA_ALWAYS_INLINE constexpr To* Upcast(From* wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    // Note, this cast may change the address if upcasting to base that lies in
    // the middle of the derived object.
    return wrapped_ptr;
  }

  // Advance the wrapped pointer by `delta_elems`.
  template <typename T,
            typename Z,
            typename = std::enable_if_t<offset_type<Z>, void>>
  static PA_ALWAYS_INLINE T* Advance(T* wrapped_ptr, Z delta_elems) {
    return wrapped_ptr + delta_elems;
  }

  template <typename T>
  static PA_ALWAYS_INLINE ptrdiff_t GetDeltaElems(T* wrapped_ptr1,
                                                  T* wrapped_ptr2) {
    return wrapped_ptr1 - wrapped_ptr2;
  }

  // Returns a copy of a wrapped pointer, without making an assertion on whether
  // memory was freed or not.
  template <typename T>
  static PA_ALWAYS_INLINE T* Duplicate(T* wrapped_ptr) {
    return wrapped_ptr;
  }

  template <typename T>
  static void ProbeForLowSeverityLifetimeIssue(T* wrapped_ptr) {
    if (wrapped_ptr) {
      reinterpret_cast<const volatile uint8_t*>(wrapped_ptr)[0];
    }
  }

  // This is for accounting only, used by unit tests.
  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {}
  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {}
};

#endif  // BUILDFLAG(USE_ASAN_UNOWNED_PTR)

template <class Super>
struct RawPtrCountingImplWrapperForTest
    : public raw_ptr_traits::RawPtrTypeToImpl<Super>::Impl {
  using SuperImpl = typename raw_ptr_traits::RawPtrTypeToImpl<Super>::Impl;
  template <typename T>
  static PA_ALWAYS_INLINE T* WrapRawPtr(T* ptr) {
    ++wrap_raw_ptr_cnt;
    return SuperImpl::WrapRawPtr(ptr);
  }

  template <typename T>
  static PA_ALWAYS_INLINE void ReleaseWrappedPtr(T* ptr) {
    ++release_wrapped_ptr_cnt;
    SuperImpl::ReleaseWrappedPtr(ptr);
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForDereference(T* wrapped_ptr) {
    ++get_for_dereference_cnt;
    return SuperImpl::SafelyUnwrapPtrForDereference(wrapped_ptr);
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* SafelyUnwrapPtrForExtraction(T* wrapped_ptr) {
    ++get_for_extraction_cnt;
    return SuperImpl::SafelyUnwrapPtrForExtraction(wrapped_ptr);
  }

  template <typename T>
  static PA_ALWAYS_INLINE T* UnsafelyUnwrapPtrForComparison(T* wrapped_ptr) {
    ++get_for_comparison_cnt;
    return SuperImpl::UnsafelyUnwrapPtrForComparison(wrapped_ptr);
  }

  static PA_ALWAYS_INLINE void IncrementSwapCountForTest() {
    ++wrapped_ptr_swap_cnt;
  }

  static PA_ALWAYS_INLINE void IncrementLessCountForTest() {
    ++wrapped_ptr_less_cnt;
  }

  static PA_ALWAYS_INLINE void IncrementPointerToMemberOperatorCountForTest() {
    ++pointer_to_member_operator_cnt;
  }

  static void ClearCounters() {
    wrap_raw_ptr_cnt = 0;
    release_wrapped_ptr_cnt = 0;
    get_for_dereference_cnt = 0;
    get_for_extraction_cnt = 0;
    get_for_comparison_cnt = 0;
    wrapped_ptr_swap_cnt = 0;
    wrapped_ptr_less_cnt = 0;
    pointer_to_member_operator_cnt = 0;
  }

  static inline int wrap_raw_ptr_cnt = INT_MIN;
  static inline int release_wrapped_ptr_cnt = INT_MIN;
  static inline int get_for_dereference_cnt = INT_MIN;
  static inline int get_for_extraction_cnt = INT_MIN;
  static inline int get_for_comparison_cnt = INT_MIN;
  static inline int wrapped_ptr_swap_cnt = INT_MIN;
  static inline int wrapped_ptr_less_cnt = INT_MIN;
  static inline int pointer_to_member_operator_cnt = INT_MIN;
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

// IsRawPtrCountingImpl<T>::value answers whether T is a specialization of
// RawPtrCountingImplWrapperForTest, to know whether Impl is for testing
// purposes.
template <typename T>
struct IsRawPtrCountingImpl : std::false_type {};

template <typename T>
struct IsRawPtrCountingImpl<internal::RawPtrCountingImplWrapperForTest<T>>
    : std::true_type {};

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

template <typename T>
struct RawPtrTypeToImpl {};

template <typename T>
struct RawPtrTypeToImpl<internal::RawPtrCountingImplWrapperForTest<T>> {
  using Impl = internal::RawPtrCountingImplWrapperForTest<T>;
};

template <>
struct RawPtrTypeToImpl<RawPtrMayDangle> {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  using Impl = internal::BackupRefPtrImpl</*AllowDangling=*/true>;
#elif BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
  using Impl = internal::AsanBackupRefPtrImpl;
#elif BUILDFLAG(USE_ASAN_UNOWNED_PTR)
  // No special bookkeeping required for this case, just treat these
  // as ordinary pointers.
  using Impl = internal::RawPtrNoOpImpl;
#elif defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
  using Impl = internal::MTECheckedPtrImpl<
      internal::MTECheckedPtrImplPartitionAllocSupport>;
#else
  using Impl = internal::RawPtrNoOpImpl;
#endif
};

template <>
struct RawPtrTypeToImpl<RawPtrBanDanglingIfSupported> {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  using Impl = internal::BackupRefPtrImpl</*AllowDangling=*/false>;
#elif BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
  using Impl = internal::AsanBackupRefPtrImpl;
#elif BUILDFLAG(USE_ASAN_UNOWNED_PTR)
  using Impl = internal::AsanUnownedPtrImpl;
#elif defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)
  using Impl = internal::MTECheckedPtrImpl<
      internal::MTECheckedPtrImplPartitionAllocSupport>;
#else
  using Impl = internal::RawPtrNoOpImpl;
#endif
};

template <>
struct RawPtrTypeToImpl<RawPtrNoOp> {
  using Impl = internal::RawPtrNoOpImpl;
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

using DefaultRawPtrType = RawPtrBanDanglingIfSupported;

template <typename T, typename RawPtrType = DefaultRawPtrType>
class PA_TRIVIAL_ABI PA_GSL_POINTER raw_ptr {
  using Impl = typename raw_ptr_traits::RawPtrTypeToImpl<RawPtrType>::Impl;
  using DanglingRawPtr = std::conditional_t<
      raw_ptr_traits::IsRawPtrCountingImpl<Impl>::value,
      raw_ptr<T, internal::RawPtrCountingImplWrapperForTest<RawPtrMayDangle>>,
      raw_ptr<T, RawPtrMayDangle>>;

#if !BUILDFLAG(USE_PARTITION_ALLOC)
  // See comment at top about `PA_RAW_PTR_CHECK()`.
  static_assert(std::is_same_v<Impl, internal::RawPtrNoOpImpl>);
#endif  // !BUILDFLAG(USE_PARTITION_ALLOC)

 public:
  static_assert(raw_ptr_traits::IsSupportedType<T>::value,
                "raw_ptr<T> doesn't work with this kind of pointee type T");

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || BUILDFLAG(USE_ASAN_UNOWNED_PTR)
  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr PA_ALWAYS_INLINE raw_ptr() noexcept : wrapped_ptr_(nullptr) {}

  PA_ALWAYS_INLINE raw_ptr(const raw_ptr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  PA_ALWAYS_INLINE raw_ptr(raw_ptr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = nullptr;
  }

  PA_ALWAYS_INLINE raw_ptr& operator=(const raw_ptr& p) noexcept {
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

  PA_ALWAYS_INLINE raw_ptr& operator=(raw_ptr&& p) noexcept {
    // Unlike the the copy version of this operator, this branch is necessaty
    // for correctness.
    if (PA_LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = nullptr;
    }
    return *this;
  }

  PA_ALWAYS_INLINE ~raw_ptr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where raw_ptr is used after destruction.
    wrapped_ptr_ = nullptr;
  }

#else  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
       // BUILDFLAG(USE_ASAN_UNOWNED_PTR)

  // raw_ptr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr PA_ALWAYS_INLINE raw_ptr() noexcept = default;

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

#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) ||
        // BUILDFLAG(USE_ASAN_UNOWNED_PTR)

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr PA_ALWAYS_INLINE raw_ptr(std::nullptr_t) noexcept
      : wrapped_ptr_(nullptr) {}

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE raw_ptr(T* p) noexcept : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE raw_ptr(const raw_ptr<U, RawPtrType>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  PA_ALWAYS_INLINE raw_ptr(raw_ptr<U, RawPtrType>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    ptr.wrapped_ptr_ = nullptr;
#endif
  }

  PA_ALWAYS_INLINE raw_ptr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = nullptr;
    return *this;
  }
  PA_ALWAYS_INLINE raw_ptr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  PA_ALWAYS_INLINE raw_ptr& operator=(
      const raw_ptr<U, RawPtrType>& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_RAW_PTR_CHECK(reinterpret_cast<uintptr_t>(this) !=
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
  PA_ALWAYS_INLINE raw_ptr& operator=(raw_ptr<U, RawPtrType>&& ptr) noexcept {
    // Make sure that pointer isn't assigned to itself (look at pointer address,
    // not its value).
#if BUILDFLAG(PA_DCHECK_IS_ON) || BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS)
    PA_RAW_PTR_CHECK(reinterpret_cast<uintptr_t>(this) !=
                     reinterpret_cast<uintptr_t>(&ptr));
#endif
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    ptr.wrapped_ptr_ = nullptr;
#endif
    return *this;
  }

  // Avoid using. The goal of raw_ptr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  PA_ALWAYS_INLINE T* get() const { return GetForExtraction(); }

  explicit PA_ALWAYS_INLINE operator bool() const { return !!wrapped_ptr_; }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  PA_ALWAYS_INLINE U& operator*() const {
    return *GetForDereference();
  }
  PA_ALWAYS_INLINE T* operator->() const { return GetForDereference(); }

  // Disables `(my_raw_ptr->*pmf)(...)` as a workaround for
  // the ICE in GCC parsing the code, reported at
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=103455
  template <typename PMF>
  void operator->*(PMF) const = delete;

  // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  PA_ALWAYS_INLINE operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit PA_ALWAYS_INLINE operator U*() const {
    // This operator may be invoked from static_cast, meaning the types may not
    // be implicitly convertible, hence the need for static_cast here.
    return static_cast<U*>(GetForExtraction());
  }

  PA_ALWAYS_INLINE raw_ptr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, 1);
    return *this;
  }
  PA_ALWAYS_INLINE raw_ptr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -1);
    return *this;
  }
  PA_ALWAYS_INLINE raw_ptr operator++(int /* post_increment */) {
    raw_ptr result = *this;
    ++(*this);
    return result;
  }
  PA_ALWAYS_INLINE raw_ptr operator--(int /* post_decrement */) {
    raw_ptr result = *this;
    --(*this);
    return result;
  }
  template <typename Z, typename = std::enable_if_t<internal::offset_type<Z>>>
  PA_ALWAYS_INLINE raw_ptr& operator+=(Z delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems);
    return *this;
  }
  template <typename Z, typename = std::enable_if_t<internal::offset_type<Z>>>
  PA_ALWAYS_INLINE raw_ptr& operator-=(Z delta_elems) {
    return *this += -delta_elems;
  }

  template <typename Z, typename = std::enable_if_t<internal::offset_type<Z>>>
  friend PA_ALWAYS_INLINE raw_ptr operator+(const raw_ptr& p, Z delta_elems) {
    raw_ptr result = p;
    return result += delta_elems;
  }
  template <typename Z, typename = std::enable_if_t<internal::offset_type<Z>>>
  friend PA_ALWAYS_INLINE raw_ptr operator-(const raw_ptr& p, Z delta_elems) {
    raw_ptr result = p;
    return result -= delta_elems;
  }
  friend PA_ALWAYS_INLINE ptrdiff_t operator-(const raw_ptr& p1,
                                              const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2.wrapped_ptr_);
  }
  friend PA_ALWAYS_INLINE ptrdiff_t operator-(T* p1, const raw_ptr& p2) {
    return Impl::GetDeltaElems(p1, p2.wrapped_ptr_);
  }
  friend PA_ALWAYS_INLINE ptrdiff_t operator-(const raw_ptr& p1, T* p2) {
    return Impl::GetDeltaElems(p1.wrapped_ptr_, p2);
  }

  // Stop referencing the underlying pointer and free its memory. Compared to
  // raw delete calls, this avoids the raw_ptr to be temporarily dangling
  // during the free operation, which will lead to taking the slower path that
  // involves quarantine.
  PA_ALWAYS_INLINE void ClearAndDelete() noexcept {
    delete GetForExtractionAndReset();
  }
  PA_ALWAYS_INLINE void ClearAndDeleteArray() noexcept {
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
  PA_ALWAYS_INLINE DanglingRawPtr ExtractAsDangling() noexcept {
    if constexpr (std::is_same_v<
                      typename std::remove_reference<decltype(*this)>::type,
                      DanglingRawPtr>) {
      DanglingRawPtr res(std::move(*this));
      // Not all implementation clear the source pointer on move, so do it
      // here just in case. Should be cheap.
      operator=(nullptr);
      return res;
    } else {
      T* ptr = GetForExtraction();
      DanglingRawPtr res(ptr);
      operator=(nullptr);
      return res;
    }
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
  friend PA_ALWAYS_INLINE bool operator==(const raw_ptr<U, I>& lhs,
                                          const raw_ptr<V, I>& rhs);
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator!=(const raw_ptr& lhs,
                                          const raw_ptr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }
  template <typename U, typename V, typename I>
  friend PA_ALWAYS_INLINE bool operator<(const raw_ptr<U, I>& lhs,
                                         const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend PA_ALWAYS_INLINE bool operator>(const raw_ptr<U, I>& lhs,
                                         const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend PA_ALWAYS_INLINE bool operator<=(const raw_ptr<U, I>& lhs,
                                          const raw_ptr<V, I>& rhs);
  template <typename U, typename V, typename I>
  friend PA_ALWAYS_INLINE bool operator>=(const raw_ptr<U, I>& lhs,
                                          const raw_ptr<V, I>& rhs);

  // Comparisons with U*. These operators also handle the case where the RHS is
  // T*.
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator==(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator==(U* lhs, const raw_ptr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator!=(U* lhs, const raw_ptr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator<(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() < rhs;
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator<=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() <= rhs;
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator>(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() > rhs;
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator>=(const raw_ptr& lhs, U* rhs) {
    return lhs.GetForComparison() >= rhs;
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator<(U* lhs, const raw_ptr& rhs) {
    return lhs < rhs.GetForComparison();
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator<=(U* lhs, const raw_ptr& rhs) {
    return lhs <= rhs.GetForComparison();
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator>(U* lhs, const raw_ptr& rhs) {
    return lhs > rhs.GetForComparison();
  }
  template <typename U>
  friend PA_ALWAYS_INLINE bool operator>=(U* lhs, const raw_ptr& rhs) {
    return lhs >= rhs.GetForComparison();
  }

  // Comparisons with `std::nullptr_t`.
  friend PA_ALWAYS_INLINE bool operator==(const raw_ptr& lhs, std::nullptr_t) {
    return !lhs;
  }
  friend PA_ALWAYS_INLINE bool operator!=(const raw_ptr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend PA_ALWAYS_INLINE bool operator==(std::nullptr_t, const raw_ptr& rhs) {
    return !rhs;
  }
  friend PA_ALWAYS_INLINE bool operator!=(std::nullptr_t, const raw_ptr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend PA_ALWAYS_INLINE void swap(raw_ptr& lhs, raw_ptr& rhs) noexcept {
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
  PA_ALWAYS_INLINE T* GetForDereference() const {
    return Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_);
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  PA_ALWAYS_INLINE T* GetForExtraction() const {
    return Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_);
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  PA_ALWAYS_INLINE T* GetForComparison() const {
    return Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_);
  }

  PA_ALWAYS_INLINE T* GetForExtractionAndReset() {
    T* ptr = GetForExtraction();
    operator=(nullptr);
    return ptr;
  }

  T* wrapped_ptr_;

  template <typename U, typename V>
  friend class raw_ptr;
};

template <typename U, typename V, typename I>
PA_ALWAYS_INLINE bool operator==(const raw_ptr<U, I>& lhs,
                                 const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() == rhs.GetForComparison();
}

template <typename U, typename V, typename I>
PA_ALWAYS_INLINE bool operator<(const raw_ptr<U, I>& lhs,
                                const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() < rhs.GetForComparison();
}

template <typename U, typename V, typename I>
PA_ALWAYS_INLINE bool operator>(const raw_ptr<U, I>& lhs,
                                const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() > rhs.GetForComparison();
}

template <typename U, typename V, typename I>
PA_ALWAYS_INLINE bool operator<=(const raw_ptr<U, I>& lhs,
                                 const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() <= rhs.GetForComparison();
}

template <typename U, typename V, typename I>
PA_ALWAYS_INLINE bool operator>=(const raw_ptr<U, I>& lhs,
                                 const raw_ptr<V, I>& rhs) {
  return lhs.GetForComparison() >= rhs.GetForComparison();
}

template <typename T>
struct IsRawPtr : std::false_type {};

template <typename T, typename I>
struct IsRawPtr<raw_ptr<T, I>> : std::true_type {};

template <typename T>
inline constexpr bool IsRawPtrV = IsRawPtr<T>::value;

// Template helpers for working with T* or raw_ptr<T>.
template <typename T>
struct IsPointer : std::false_type {};

template <typename T>
struct IsPointer<T*> : std::true_type {};

template <typename T, typename I>
struct IsPointer<raw_ptr<T, I>> : std::true_type {};

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

template <typename T, typename I>
struct RemovePointer<raw_ptr<T, I>> {
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
// When using it, please provide a justification about what guarantees it will
// never be dereferenced after becoming dangling.
using DisableDanglingPtrDetection = base::RawPtrMayDangle;

// See `docs/dangling_ptr.md`
// Annotates known dangling raw_ptr. Those haven't been triaged yet. All the
// occurrences are meant to be removed. See https://crbug.com/1291138.
using DanglingUntriaged = DisableDanglingPtrDetection;

// The following template parameters are only meaningful when `raw_ptr`
// is `MTECheckedPtr` (never the case unless a particular GN arg is set
// true.) `raw_ptr` users need not worry about this and can refer solely
// to `DisableDanglingPtrDetection` and `DanglingUntriaged` above.
//
// The `raw_ptr` definition allows users to specify an implementation.
// When `MTECheckedPtr` is in play, we need to augment this
// implementation setting with another layer that allows the `raw_ptr`
// to degrade into the no-op version.
#if defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

// Direct pass-through to no-op implementation.
using DegradeToNoOpWhenMTE = base::RawPtrNoOp;

// As above, but with the "untriaged dangling" annotation.
using DanglingUntriagedDegradeToNoOpWhenMTE = base::RawPtrNoOp;

// As above, but with the "explicitly disable protection" annotation.
using DisableDanglingPtrDetectionDegradeToNoOpWhenMTE = base::RawPtrNoOp;

#else

// Direct pass-through to default implementation specified by `raw_ptr`
// template.
using DegradeToNoOpWhenMTE = base::RawPtrBanDanglingIfSupported;

// Direct pass-through to `DanglingUntriaged`.
using DanglingUntriagedDegradeToNoOpWhenMTE = DanglingUntriaged;

// Direct pass-through to `DisableDanglingPtrDetection`.
using DisableDanglingPtrDetectionDegradeToNoOpWhenMTE =
    DisableDanglingPtrDetection;

#endif  // defined(PA_ENABLE_MTE_CHECKED_PTR_SUPPORT_WITH_64_BITS_POINTERS)

namespace std {

// Override so set/map lookups do not create extra raw_ptr. This also allows
// dangling pointers to be used for lookup.
template <typename T, typename RawPtrType>
struct less<raw_ptr<T, RawPtrType>> {
  using Impl =
      typename base::raw_ptr_traits::RawPtrTypeToImpl<RawPtrType>::Impl;
  using is_transparent = void;

  bool operator()(const raw_ptr<T, RawPtrType>& lhs,
                  const raw_ptr<T, RawPtrType>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(T* lhs, const raw_ptr<T, RawPtrType>& rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }

  bool operator()(const raw_ptr<T, RawPtrType>& lhs, T* rhs) const {
    Impl::IncrementLessCountForTest();
    return lhs < rhs;
  }
};

// Define for cases where raw_ptr<T> holds a pointer to an array of type T.
// This is consistent with definition of std::iterator_traits<T*>.
// Algorithms like std::binary_search need that.
template <typename T, typename Impl>
struct iterator_traits<raw_ptr<T, Impl>> {
  using difference_type = ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;
};

}  // namespace std

#endif  // BASE_MEMORY_RAW_PTR_H_
