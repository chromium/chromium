// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_CHECKED_PTR_H_
#define BASE_MEMORY_CHECKED_PTR_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/allocator/partition_allocator/checked_ptr_support.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#define ENABLE_CHECKED_PTR2_OR_MTE_IMPL 0
#if ENABLE_CHECKED_PTR2_OR_MTE_IMPL
static_assert(ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_MTE_CHECKED_PTR ||
                  ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR,
              "CheckedPtr2OrMTEImpl can only by used if tags are enabled");
#endif

#define ENABLE_BACKUP_REF_PTR_IMPL 0
#if ENABLE_BACKUP_REF_PTR_IMPL
static_assert(ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR,
              "BackupRefPtrImpl can only by used if PartitionRefCount is "
              "enabled");
#endif

#define CHECKED_PTR2_USE_NO_OP_WRAPPER 0
#define CHECKED_PTR2_USE_TRIVIAL_UNWRAPPER 0

// Set it to 1 to avoid branches when checking if per-pointer protection is
// enabled.
#define CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED 0
// Set it to 1 to avoid branches when dereferencing the pointer.
// Must be 1 if the above is 1.
#define CHECKED_PTR2_AVOID_BRANCH_WHEN_DEREFERENCING 0

namespace base {

// NOTE: All methods should be ALWAYS_INLINE. CheckedPtr is meant to be a
// lightweight replacement of a raw pointer, hence performance is critical.

namespace internal {
// These classes/structures are part of the CheckedPtr implementation.
// DO NOT USE THESE CLASSES DIRECTLY YOURSELF.

struct CheckedPtrNoOpImpl {
  // Wraps a pointer, and returns its uintptr_t representation.
  // Use |const volatile| to prevent compiler error. These will be dropped
  // anyway when casting to uintptr_t and brought back upon pointer extraction.
  static ALWAYS_INLINE uintptr_t WrapRawPtr(const volatile void* cv_ptr) {
    return reinterpret_cast<uintptr_t>(cv_ptr);
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(uintptr_t) {}

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr ALWAYS_INLINE uintptr_t GetWrappedNullPtr() {
    // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
    // which apparently isn't true in all environments.
    return 0;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr uintptr_t Upcast(uintptr_t wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    return reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE uintptr_t Advance(uintptr_t wrapped_ptr, size_t delta) {
    return wrapped_ptr + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE uintptr_t Duplicate(uintptr_t wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
};

#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

constexpr int kValidAddressBits = 48;
constexpr uintptr_t kAddressMask = (1ull << kValidAddressBits) - 1;
constexpr int kGenerationBits = sizeof(uintptr_t) * 8 - kValidAddressBits;
constexpr uintptr_t kGenerationMask = ~kAddressMask;
constexpr int kTopBitShift = 63;
constexpr uintptr_t kTopBit = 1ull << kTopBitShift;
static_assert(kTopBit << 1 == 0, "kTopBit should really be the top bit");
static_assert((kTopBit & kGenerationMask) > 0,
              "kTopBit bit must be inside the generation region");

#if BUILDFLAG(USE_PARTITION_ALLOC) && ENABLE_CHECKED_PTR2_OR_MTE_IMPL
// This functionality is outside of CheckedPtr2OrMTEImpl, so that it can be
// overridden by tests.
struct CheckedPtr2OrMTEImplPartitionAllocSupport {
  // Checks if the necessary support is enabled in PartitionAlloc for |ptr|.
  static ALWAYS_INLINE bool EnabledForPtr(void* ptr) {
    // CheckedPtr2 and MTECheckedPtr algorithms work only when memory is
    // allocated by PartitionAlloc, from normal buckets pool. CheckedPtr2
    // additionally requires that the pointer points to the beginning of the
    // allocated slot.
    //
    // TODO(bartekn): Allow direct-map buckets for MTECheckedPtr, once
    // PartitionAlloc supports it. (Currently not implemented for simplicity,
    // but there are no technological obstacles preventing it; whereas in case
    // of CheckedPtr2, PartitionAllocGetSlotOffset won't work with direct-map.)
    return IsManagedByPartitionAllocNormalBuckets(ptr)
    // Checking offset is not needed for ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR,
    // but call it anyway for apples-to-apples comparison with
    // ENABLE_TAG_FOR_CHECKED_PTR2.
#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR
           && base::internal::PartitionAllocGetSlotOffset(ptr) == 0
#endif
        ;
  }

  // Returns pointer to the tag that protects are pointed by |ptr|.
  static ALWAYS_INLINE void* TagPointer(void* ptr) {
    return PartitionTagPointer(ptr);
  }

#if CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
  // Returns offset of the tag from the beginning of the slot. Works only with
  // CheckedPtr2 algorithm.
  static constexpr size_t TagOffset() {
#if ENABLE_TAG_FOR_CHECKED_PTR2
    return kPartitionTagOffset;
#else
    // Unreachable, but can't use NOTREACHED() due to constexpr. Return
    // something weird so that the caller is very likely to crash.
    return 0x87654321FEDCBA98;
#endif
  }
#endif
};
#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && ENABLE_CHECKED_PTR2_OR_MTE_IMPL

template <typename PartitionAllocSupport>
struct CheckedPtr2OrMTEImpl {
  // This implementation assumes that pointers are 64 bits long and at least 16
  // top bits are unused. The latter is harder to verify statically, but this is
  // true for all currently supported 64-bit architectures (DCHECK when wrapping
  // will verify that).
  static_assert(sizeof(void*) >= 8, "Need 64-bit pointers");

  // Wraps a pointer, and returns its uintptr_t representation.
  static ALWAYS_INLINE uintptr_t WrapRawPtr(const volatile void* cv_ptr) {
    void* ptr = const_cast<void*>(cv_ptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
#if !CHECKED_PTR2_USE_NO_OP_WRAPPER
    // Make sure that the address bits that will be used for generation are 0.
    // If they aren't, they'd fool the unwrapper into thinking that the
    // protection is enabled, making it try to read and compare the generation.
    DCHECK_EQ(ExtractGeneration(addr), 0ull);

    // Return a not-wrapped |addr|, if it's either nullptr or if the protection
    // for this pointer is disabled.
    if (!PartitionAllocSupport::EnabledForPtr(ptr)) {
      return addr;
    }

    // Read the generation and place it in the top bits of the address.
    // Even if PartitionAlloc's tag has less than kGenerationBits, we'll read
    // what's given and pad the rest with 0s.
    static_assert(sizeof(PartitionTag) * 8 <= kGenerationBits, "");
    uintptr_t generation = *(static_cast<volatile PartitionTag*>(
        PartitionAllocSupport::TagPointer(ptr)));

    generation <<= kValidAddressBits;
    addr |= generation;
#if CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
    // Always set top bit to 1, to indicated that the protection is enabled.
    addr |= kTopBit;
#endif  // CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
#endif  // !CHECKED_PTR2_USE_NO_OP_WRAPPER
    return addr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  // No-op for CheckedPtr2OrMTEImpl.
  static ALWAYS_INLINE void ReleaseWrappedPtr(uintptr_t) {}

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr ALWAYS_INLINE uintptr_t GetWrappedNullPtr() {
    return kWrappedNullPtr;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(
      uintptr_t wrapped_ptr) {
#if CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
    // This variant can only be used with CheckedPtr2 algorithm, because it
    // relies on the generation to exist at a constant offset before the
    // allocation.
    static_assert(ENABLE_TAG_FOR_CHECKED_PTR2, "");

    // Top bit tells if the protection is enabled. Use it to decide whether to
    // read the word before the allocation, which exists only if the protection
    // is enabled. Otherwise it may crash, in which case read the data from the
    // beginning of the allocation instead and ignore it later. All this magic
    // is to avoid a branch, for performance reasons.
    //
    // A couple examples, assuming 64-bit system (continued below):
    //   Ex.1: wrapped_ptr=0x8442000012345678
    //           => enabled=0x8000000000000000
    //           => offset=1
    //   Ex.2: wrapped_ptr=0x0000000012345678
    //           => enabled=0x0000000000000000
    //           => offset=0
    uintptr_t enabled = wrapped_ptr & kTopBit;
    // We can't have protection disabled and generation set in the same time.
    DCHECK(!(enabled == 0 && (ExtractGeneration(wrapped_ptr)) != 0));
    uintptr_t offset = enabled >> kTopBitShift;  // 0 or 1
    // Use offset to decide if the generation should be read at the beginning or
    // before the allocation.
    // TODO(bartekn): Do something about 1-byte allocations. Reading 2-byte
    // generation at the allocation could crash. This case is executed
    // specifically for non-PartitionAlloc pointers, so we can't make
    // assumptions about alignment.
    //
    // Cast to volatile to ensure memory is read. E.g. in a tight loop, the
    // compiler could cache the value in a register and thus could miss that
    // another thread freed memory and cleared generation.
    //
    // Examples (continued):
    //   Ex.1: generation_ptr=0x0000000012345676
    //     a) if pointee wasn't freed, read e.g. generation=0x0442 (could be
    //        also 0x8442, the top bit is overwritten later)
    //     b) if pointee was freed, read e.g. generation=0x1234 (could be
    //        anything)
    //   Ex.2: generation_ptr=0x0000000012345678, read e.g. 0x2345 (doesn't
    //         matter what we read, as long as this read doesn't crash)
    volatile PartitionTag* generation_ptr =
        static_cast<volatile PartitionTag*>(ExtractPtr(wrapped_ptr)) -
        offset * (PartitionAllocSupport::TagOffset() / sizeof(PartitionTag));
    uintptr_t generation = *generation_ptr;
    // Shift generation into the right place and add back the enabled bit.
    //
    // Examples (continued):
    //   Ex.1:
    //     a) generation=0x8442000000000000
    //     a) generation=0x9234000000000000
    //   Ex.2: generation=0x2345000000000000
    generation <<= kValidAddressBits;
    generation |= enabled;

    // If the protection isn't enabled, clear top bits. Casting to a signed
    // type makes >> sign extend the last bit.
    //
    // Examples (continued):
    //   Ex.1: mask=0xffff000000000000
    //     a) generation=0x8442000000000000
    //     b) generation=0x9234000000000000
    //   Ex.2: mask=0x0000000000000000 => generation=0x0000000000000000
    uintptr_t mask = static_cast<intptr_t>(enabled) >> (kGenerationBits - 1);
    generation &= mask;

    // Use hardware to detect generation mismatch. CPU will crash if top bits
    // aren't all 0 (technically it won't if all bits are 1, but that's a kernel
    // mode address, which isn't allowed either... also, top bit will be always
    // zeroed out).
    //
    // Examples (continued):
    //   Ex.1:
    //     a) returning 0x0000000012345678
    //     b) returning 0x1676000012345678 (this will generate a desired crash)
    //   Ex.2: returning 0x0000000012345678
    static_assert(CHECKED_PTR2_AVOID_BRANCH_WHEN_DEREFERENCING, "");
    return reinterpret_cast<void*>(generation ^ wrapped_ptr);
#else  // CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
    uintptr_t ptr_generation = wrapped_ptr >> kValidAddressBits;
    if (ptr_generation > 0) {
      // Read the generation provided by PartitionAlloc.
      //
      // Cast to volatile to ensure memory is read. E.g. in a tight loop, the
      // compiler could cache the value in a register and thus could miss that
      // another thread freed memory and cleared generation.
      uintptr_t read_generation = *static_cast<volatile PartitionTag*>(
          PartitionAllocSupport::TagPointer(ExtractPtr(wrapped_ptr)));
#if CHECKED_PTR2_AVOID_BRANCH_WHEN_DEREFERENCING
      // Use hardware to detect generation mismatch. CPU will crash if top bits
      // aren't all 0 (technically it won't if all bits are 1, but that's a
      // kernel mode address, which isn't allowed either).
      read_generation <<= kValidAddressBits;
      return reinterpret_cast<void*>(read_generation ^ wrapped_ptr);
#else
      if (UNLIKELY(ptr_generation != read_generation))
        IMMEDIATE_CRASH();
      return reinterpret_cast<void*>(wrapped_ptr & kAddressMask);
#endif  // CHECKED_PTR2_AVOID_BRANCH_WHEN_DEREFERENCING
    }
    return reinterpret_cast<void*>(wrapped_ptr);
#endif  // CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(
      uintptr_t wrapped_ptr) {
#if CHECKED_PTR2_AVOID_BRANCH_WHEN_CHECKING_ENABLED
    // In this implementation, SafelyUnwrapPtrForDereference doesn't tolerate
    // nullptr, because it reads unconditionally to avoid branches. Handle the
    // nullptr case here.
    if (wrapped_ptr == kWrappedNullPtr)
      return nullptr;
    return SafelyUnwrapPtrForDereference(wrapped_ptr);
#else
    // In this implementation, SafelyUnwrapPtrForDereference handles nullptr
    // case well.
    return SafelyUnwrapPtrForDereference(wrapped_ptr);
#endif
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(
      uintptr_t wrapped_ptr) {
    return ExtractPtr(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE uintptr_t Upcast(uintptr_t wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");

#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR
    if (IsPtrUnaffectedByUpcast<To, From>())
      return wrapped_ptr;

    // CheckedPtr2 doesn't support a pointer pointing in the middle of an
    // allocated object, so disable the generation tag.
    //
    // Clearing tag is not needed for ENABLE_TAG_FOR_SINGLE_TAG_CHECKED_PTR,
    // but do it anyway for apples-to-apples comparison with
    // ENABLE_TAG_FOR_CHECKED_PTR2.
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(ExtractPtr(wrapped_ptr))));
    return base_addr;
#elif ENABLE_TAG_FOR_MTE_CHECKED_PTR
    // The top-bit generation tag must not affect the result of upcast.
    return reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(wrapped_ptr)));
#else
    static_assert(std::is_void<To>::value,  // Always false.
                  "Unknown tagging mode");
    return 0;
#endif
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE uintptr_t Advance(uintptr_t wrapped_ptr, size_t delta) {
    // Mask out the generation to disable the protection. It's not supported for
    // pointers inside an allocation.
    return ExtractAddress(wrapped_ptr) + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE uintptr_t Duplicate(uintptr_t wrapped_ptr) {
    return wrapped_ptr;
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}

 private:
  static ALWAYS_INLINE uintptr_t ExtractAddress(uintptr_t wrapped_ptr) {
    return wrapped_ptr & kAddressMask;
  }
  static ALWAYS_INLINE void* ExtractPtr(uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(ExtractAddress(wrapped_ptr));
  }
  static ALWAYS_INLINE uintptr_t ExtractGeneration(uintptr_t wrapped_ptr) {
    return wrapped_ptr & kGenerationMask;
  }

  template <typename To, typename From>
  static constexpr ALWAYS_INLINE bool IsPtrUnaffectedByUpcast() {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    uintptr_t d = 0x10000;
    From* dp = reinterpret_cast<From*>(d);
    To* bp = dp;
    uintptr_t b = reinterpret_cast<uintptr_t>(bp);
    return b == d;
  }

  // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
  // which apparently isn't true in some rare environments.
  static constexpr uintptr_t kWrappedNullPtr = 0;
};

#if ENABLE_BACKUP_REF_PTR_IMPL

struct BackupRefPtrImpl {
  // Note that `BackupRefPtrImpl` itself is not thread-safe. If multiple threads
  // modify the same smart pointer object without synchronization, a data race
  // will occur.

  // Wraps a pointer, and returns its uintptr_t representation.
  // Use |const volatile| to prevent compiler error. These will be dropped
  // anyway when casting to uintptr_t and brought back upon pointer extraction.
  static ALWAYS_INLINE uintptr_t WrapRawPtr(const volatile void* cv_ptr) {
    void* ptr = const_cast<void*>(cv_ptr);
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);

    if (LIKELY(IsManagedByPartitionAllocNormalBuckets(ptr)))
      PartitionRefCountPointer(ptr)->AddRef();

    return addr;
  }

  // Notifies the allocator when a wrapped pointer is being removed or replaced.
  static ALWAYS_INLINE void ReleaseWrappedPtr(uintptr_t wrapped_ptr) {
    void* ptr = reinterpret_cast<void*>(wrapped_ptr);

    // This check already covers the nullptr case.
    if (LIKELY(IsManagedByPartitionAllocNormalBuckets(ptr)))
      PartitionRefCountPointer(ptr)->Release();
  }

  // Returns equivalent of |WrapRawPtr(nullptr)|. Separated out to make it a
  // constexpr.
  static constexpr ALWAYS_INLINE uintptr_t GetWrappedNullPtr() {
    // This relies on nullptr and 0 being equal in the eyes of reinterpret_cast,
    // which apparently isn't true in all environments.
    return 0;
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function is allowed to crash on nullptr.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForDereference(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, while asserting that memory
  // hasn't been freed. The function must handle nullptr gracefully.
  static ALWAYS_INLINE void* SafelyUnwrapPtrForExtraction(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Unwraps the pointer's uintptr_t representation, without making an assertion
  // on whether memory was freed or not.
  static ALWAYS_INLINE void* UnsafelyUnwrapPtrForComparison(
      uintptr_t wrapped_ptr) {
    return reinterpret_cast<void*>(wrapped_ptr);
  }

  // Upcasts the wrapped pointer.
  template <typename To, typename From>
  static ALWAYS_INLINE constexpr uintptr_t Upcast(uintptr_t wrapped_ptr) {
    static_assert(std::is_convertible<From*, To*>::value,
                  "From must be convertible to To.");
    return reinterpret_cast<uintptr_t>(
        static_cast<To*>(reinterpret_cast<From*>(wrapped_ptr)));
  }

  // Advance the wrapped pointer by |delta| bytes.
  static ALWAYS_INLINE uintptr_t Advance(uintptr_t wrapped_ptr, size_t delta) {
    return wrapped_ptr + delta;
  }

  // Returns a copy of a wrapped pointer, without making an assertion
  // on whether memory was freed or not. This method increments the reference
  // count of the allocation slot.
  static ALWAYS_INLINE uintptr_t Duplicate(uintptr_t wrapped_ptr) {
    return WrapRawPtr(reinterpret_cast<void*>(wrapped_ptr));
  }

  // This is for accounting only, used by unit tests.
  static ALWAYS_INLINE void IncrementSwapCountForTest() {}
};

#endif  // ENABLE_BACKUP_REF_PTR_IMPL

#endif  // defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)

}  // namespace internal

// DO NOT USE! EXPERIMENTAL ONLY! This is helpful for local testing!
//
// CheckedPtr is meant to be a pointer wrapper, that will crash on
// Use-After-Free (UaF) to prevent security issues. This is very much in the
// experimental phase. More context in:
// https://docs.google.com/document/d/1pnnOAIz_DMWDI4oIOFoMAqLnf_MZ2GsrJNb_dbQ3ZBg
//
// For now, CheckedPtr is a no-op wrapper to aid local testing.
//
// Goals for this API:
// 1. Minimize amount of caller-side changes as much as physically possible.
// 2. Keep this class as small as possible, while still satisfying goal #1 (i.e.
//    we aren't striving to maximize compatibility with raw pointers, merely
//    adding support for cases encountered so far).
template <typename T,
#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL) && \
    BUILDFLAG(USE_PARTITION_ALLOC)

#if ENABLE_CHECKED_PTR2_OR_MTE_IMPL
          typename Impl = internal::CheckedPtr2OrMTEImpl<
              internal::CheckedPtr2OrMTEImplPartitionAllocSupport>>
#elif ENABLE_BACKUP_REF_PTR_IMPL
          typename Impl = internal::BackupRefPtrImpl>
#else
          typename Impl = internal::CheckedPtrNoOpImpl>
#endif

#else  // defined(ARCH_CPU_64_BITS) && !defined(OS_NACL) &&
       // BUILDFLAG(USE_PARTITION_ALLOC)
          typename Impl = internal::CheckedPtrNoOpImpl>
#endif
class CheckedPtr {
 public:
#if ENABLE_BACKUP_REF_PTR_IMPL

  // BackupRefPtr requires a non-trivial default constructor, destructor, etc.
  constexpr ALWAYS_INLINE CheckedPtr() noexcept
      : wrapped_ptr_(Impl::GetWrappedNullPtr()) {}

  CheckedPtr(const CheckedPtr& p) noexcept
      : wrapped_ptr_(Impl::Duplicate(p.wrapped_ptr_)) {}

  CheckedPtr(CheckedPtr&& p) noexcept {
    wrapped_ptr_ = p.wrapped_ptr_;
    p.wrapped_ptr_ = Impl::GetWrappedNullPtr();
  }

  CheckedPtr& operator=(const CheckedPtr& p) {
    // Duplicate before releasing, in case the pointer is assigned to itself.
    uintptr_t new_ptr = Impl::Duplicate(p.wrapped_ptr_);
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = new_ptr;
    return *this;
  }

  CheckedPtr& operator=(CheckedPtr&& p) {
    if (LIKELY(this != &p)) {
      Impl::ReleaseWrappedPtr(wrapped_ptr_);
      wrapped_ptr_ = p.wrapped_ptr_;
      p.wrapped_ptr_ = Impl::GetWrappedNullPtr();
    }
    return *this;
  }

  ALWAYS_INLINE ~CheckedPtr() noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    // Work around external issues where CheckedPtr is used after destruction.
    wrapped_ptr_ = Impl::GetWrappedNullPtr();
  }

#else  // ENABLE_BACKUP_REF_PTR_IMPL

  // CheckedPtr can be trivially default constructed (leaving |wrapped_ptr_|
  // uninitialized).  This is needed for compatibility with raw pointers.
  //
  // TODO(lukasza): Always initialize |wrapped_ptr_|.  Fix resulting build
  // errors.  Analyze performance impact.
  constexpr CheckedPtr() noexcept = default;

  // In addition to nullptr_t ctor above, CheckedPtr needs to have these
  // as |=default| or |constexpr| to avoid hitting -Wglobal-constructors in
  // cases like this:
  //     struct SomeStruct { int int_field; CheckedPtr<int> ptr_field; };
  //     SomeStruct g_global_var = { 123, nullptr };
  CheckedPtr(const CheckedPtr&) noexcept = default;
  CheckedPtr(CheckedPtr&&) noexcept = default;
  CheckedPtr& operator=(const CheckedPtr&) noexcept = default;
  CheckedPtr& operator=(CheckedPtr&&) noexcept = default;

  ~CheckedPtr() = default;

#endif  // ENABLE_BACKUP_REF_PTR_IMPL

  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr ALWAYS_INLINE CheckedPtr(std::nullptr_t) noexcept
      : wrapped_ptr_(Impl::GetWrappedNullPtr()) {}

  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE CheckedPtr(T* p) noexcept : wrapped_ptr_(Impl::WrapRawPtr(p)) {}

  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE CheckedPtr(const CheckedPtr<U, Impl>& ptr) noexcept
      : wrapped_ptr_(
            Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_))) {}
  // Deliberately implicit in order to support implicit upcast.
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ALWAYS_INLINE CheckedPtr(CheckedPtr<U, Impl>&& ptr) noexcept
      : wrapped_ptr_(Impl::template Upcast<T, U>(ptr.wrapped_ptr_)) {
#if ENABLE_BACKUP_REF_PTR_IMPL
    ptr.wrapped_ptr_ = Impl::GetWrappedNullPtr();
#endif
  }

  ALWAYS_INLINE CheckedPtr& operator=(std::nullptr_t) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::GetWrappedNullPtr();
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator=(T* p) noexcept {
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::WrapRawPtr(p);
    return *this;
  }

  // Upcast assignment
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE CheckedPtr& operator=(const CheckedPtr<U, Impl>& ptr) noexcept {
    DCHECK(reinterpret_cast<uintptr_t>(this) !=
           reinterpret_cast<uintptr_t>(&ptr));
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ =
        Impl::Duplicate(Impl::template Upcast<T, U>(ptr.wrapped_ptr_));
    return *this;
  }
  template <typename U,
            typename Unused = std::enable_if_t<
                std::is_convertible<U*, T*>::value &&
                !std::is_void<typename std::remove_cv<T>::type>::value>>
  ALWAYS_INLINE CheckedPtr& operator=(CheckedPtr<U, Impl>&& ptr) noexcept {
    DCHECK(reinterpret_cast<uintptr_t>(this) !=
           reinterpret_cast<uintptr_t>(&ptr));
    Impl::ReleaseWrappedPtr(wrapped_ptr_);
    wrapped_ptr_ = Impl::template Upcast<T, U>(ptr.wrapped_ptr_);
#if ENABLE_BACKUP_REF_PTR_IMPL
    ptr.wrapped_ptr_ = Impl::GetWrappedNullPtr();
#endif
    return *this;
  }

  // Avoid using. The goal of CheckedPtr is to be as close to raw pointer as
  // possible, so use it only if absolutely necessary (e.g. for const_cast).
  ALWAYS_INLINE T* get() const { return GetForExtraction(); }

  explicit ALWAYS_INLINE operator bool() const {
    return wrapped_ptr_ != Impl::GetWrappedNullPtr();
  }

  template <typename U = T,
            typename Unused = std::enable_if_t<
                !std::is_void<typename std::remove_cv<U>::type>::value>>
  ALWAYS_INLINE U& operator*() const {
    return *GetForDereference();
  }
  ALWAYS_INLINE T* operator->() const { return GetForDereference(); }
  // Deliberately implicit, because CheckedPtr is supposed to resemble raw ptr.
  // NOLINTNEXTLINE(runtime/explicit)
  ALWAYS_INLINE operator T*() const { return GetForExtraction(); }
  template <typename U>
  explicit ALWAYS_INLINE operator U*() const {
    return static_cast<U*>(GetForExtraction());
  }

  ALWAYS_INLINE CheckedPtr& operator++() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator--() {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, -sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr operator++(int /* post_increment */) {
    CheckedPtr result = *this;
    ++(*this);
    return result;
  }
  ALWAYS_INLINE CheckedPtr operator--(int /* post_decrement */) {
    CheckedPtr result = *this;
    --(*this);
    return result;
  }
  ALWAYS_INLINE CheckedPtr& operator+=(ptrdiff_t delta_elems) {
    wrapped_ptr_ = Impl::Advance(wrapped_ptr_, delta_elems * sizeof(T));
    return *this;
  }
  ALWAYS_INLINE CheckedPtr& operator-=(ptrdiff_t delta_elems) {
    return *this += -delta_elems;
  }

  // Be careful to cover all cases with CheckedPtr being on both sides, left
  // side only and right side only. If any case is missed, a more costly
  // |operator T*()| will get called, instead of |operator==|.
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs,
                                       const CheckedPtr& rhs) {
    return lhs.GetForComparison() == rhs.GetForComparison();
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs,
                                       const CheckedPtr& rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, T* rhs) {
    return lhs.GetForComparison() == rhs;
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, T* rhs) {
    return !(lhs == rhs);
  }
  friend ALWAYS_INLINE bool operator==(T* lhs, const CheckedPtr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  friend ALWAYS_INLINE bool operator!=(T* lhs, const CheckedPtr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for cases like |derived_ptr == base_ptr|. Without these, a more
  // costly |operator T*()| will get called, instead of |operator==|.
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs,
                                       const CheckedPtr<U, Impl>& rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() ==
           static_cast<std::add_cv_t<T>*>(rhs.GetForComparison());
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs,
                                       const CheckedPtr<U, Impl>& rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, U* rhs) {
    // Add |const volatile| when casting, in case |U| has any. Even if |T|
    // doesn't, comparison between |T*| and |const volatile T*| is fine.
    return lhs.GetForComparison() == static_cast<std::add_cv_t<T>*>(rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, U* rhs) {
    return !(lhs == rhs);
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator==(U* lhs, const CheckedPtr& rhs) {
    return rhs == lhs;  // Reverse order to call the operator above.
  }
  template <typename U>
  friend ALWAYS_INLINE bool operator!=(U* lhs, const CheckedPtr& rhs) {
    return rhs != lhs;  // Reverse order to call the operator above.
  }
  // Needed for comparisons against nullptr. Without these, a slightly more
  // costly version would be called that extracts wrapped pointer, as opposed
  // to plain comparison against 0.
  friend ALWAYS_INLINE bool operator==(const CheckedPtr& lhs, std::nullptr_t) {
    return !lhs;
  }
  friend ALWAYS_INLINE bool operator!=(const CheckedPtr& lhs, std::nullptr_t) {
    return !!lhs;  // Use !! otherwise the costly implicit cast will be used.
  }
  friend ALWAYS_INLINE bool operator==(std::nullptr_t, const CheckedPtr& rhs) {
    return !rhs;
  }
  friend ALWAYS_INLINE bool operator!=(std::nullptr_t, const CheckedPtr& rhs) {
    return !!rhs;  // Use !! otherwise the costly implicit cast will be used.
  }

  friend ALWAYS_INLINE void swap(CheckedPtr& lhs, CheckedPtr& rhs) noexcept {
    Impl::IncrementSwapCountForTest();
    std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
  }

 private:
  // This getter is meant for situations where the pointer is meant to be
  // dereferenced. It is allowed to crash on nullptr (it may or may not),
  // because it knows that the caller will crash on nullptr.
  ALWAYS_INLINE T* GetForDereference() const {
#if CHECKED_PTR2_USE_TRIVIAL_UNWRAPPER
    return static_cast<T*>(Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_));
#else
    return static_cast<T*>(Impl::SafelyUnwrapPtrForDereference(wrapped_ptr_));
#endif
  }
  // This getter is meant for situations where the raw pointer is meant to be
  // extracted outside of this class, but not necessarily with an intention to
  // dereference. It mustn't crash on nullptr.
  ALWAYS_INLINE T* GetForExtraction() const {
#if CHECKED_PTR2_USE_TRIVIAL_UNWRAPPER
    return static_cast<T*>(Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_));
#else
    return static_cast<T*>(Impl::SafelyUnwrapPtrForExtraction(wrapped_ptr_));
#endif
  }
  // This getter is meant *only* for situations where the pointer is meant to be
  // compared (guaranteeing no dereference or extraction outside of this class).
  // Any verifications can and should be skipped for performance reasons.
  ALWAYS_INLINE T* GetForComparison() const {
    return static_cast<T*>(Impl::UnsafelyUnwrapPtrForComparison(wrapped_ptr_));
  }

  // Store the pointer as |uintptr_t|, because depending on implementation, its
  // unused bits may be re-purposed to store extra information.
  uintptr_t wrapped_ptr_;

  template <typename U, typename V>
  friend class CheckedPtr;
};

}  // namespace base

using base::CheckedPtr;

#endif  // BASE_MEMORY_CHECKED_PTR_H_
