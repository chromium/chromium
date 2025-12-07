// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines wrapper types for memory addresses that represent the start
// of a memory slot or slot span. These types (`SlotSpanStart`, `SlotStart`,
// `UntaggedSlotStart`) provide a more type-safe way to handle memory addresses
// within PartitionAlloc, especially considering Memory Tagging Extension (MTE).

#ifndef PARTITION_ALLOC_SLOT_START_H_
#define PARTITION_ALLOC_SLOT_START_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_dcheck_helper.h"
#include "partition_alloc/tagging.h"

namespace partition_alloc {

struct PartitionRoot;

namespace internal {

class UntaggedSlotStart;

// Represents an address of a slot span start.
// A slot span start is always also a slot start. This type is MTE-untagged.
// It can be safely converted to `UntaggedSlotStart` or `SlotStart`.
class SlotSpanStart {
 public:
  PA_ALWAYS_INLINE constexpr SlotSpanStart() = default;
  PA_ALWAYS_INLINE explicit constexpr SlotSpanStart(uintptr_t address)
      : address_(address) {}

  PA_ALWAYS_INLINE explicit constexpr operator bool() const {
    return address_ != 0;
  }
  PA_ALWAYS_INLINE constexpr uintptr_t value() const { return address_; }
  PA_ALWAYS_INLINE constexpr ptrdiff_t offset(uintptr_t other) const {
    return static_cast<ptrdiff_t>(other - address_);
  }
  PA_ALWAYS_INLINE constexpr ptrdiff_t offset(UntaggedSlotStart other) const;

  PA_ALWAYS_INLINE constexpr UntaggedSlotStart AsSlotStart() const;
  PA_ALWAYS_INLINE constexpr UntaggedSlotStart GetNthSlotStart(
      size_t n,
      size_t slot_size) const;

  PA_ALWAYS_INLINE constexpr bool operator==(SlotSpanStart other) const {
    return value() == other.value();
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(SlotSpanStart other) const {
    return !(*this == other);
  }

  PA_ALWAYS_INLINE constexpr bool operator==(uintptr_t other) const {
    return value() == other;
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(uintptr_t other) const {
    return value() != other;
  }

 private:
  uintptr_t address_ = 0;
};

static_assert(sizeof(SlotSpanStart) == sizeof(void*));
static_assert(std::is_trivially_copyable_v<SlotSpanStart>);
static_assert(std::is_trivially_destructible_v<SlotSpanStart>);

// Represents an address of a slot start, MTE-tagged.
// This type should be used when dealing with pointers that may carry an MTE
// tag. It provides methods for checked and unchecked construction, as well as
// untagging.
class SlotStart {
 public:
  PA_ALWAYS_INLINE constexpr SlotStart() = default;

  // `SlotStart::Checked()` may perform runtime check to confirm that it is
  // indeed a slot start. Prefer this variant for untrusted input or payload
  // which can be corrupted by Use-after-Free etc.  `SlotStart::Unchecked()` has
  // no check at all and faster.
  static SlotStart Checked(uintptr_t tagged_address,
                           const PartitionRoot* root) {
    SlotStart ret(tagged_address);
    ret.Check(root);
    return ret;
  }
  PA_ALWAYS_INLINE static SlotStart Checked(void* ptr,
                                            const PartitionRoot* root) {
    return Checked(reinterpret_cast<uintptr_t>(ptr), root);
  }
  PA_ALWAYS_INLINE static constexpr SlotStart Unchecked(
      uintptr_t tagged_address) {
    return SlotStart(tagged_address);
  }
  PA_ALWAYS_INLINE static constexpr SlotStart Unchecked(const void* ptr) {
    return SlotStart(reinterpret_cast<uintptr_t>(ptr));
  }

  PA_ALWAYS_INLINE explicit constexpr operator bool() const {
    return address_ != 0;
  }
  PA_ALWAYS_INLINE constexpr uintptr_t value() const { return address_; }

  // Check if this indeed points to the beginning of a slot.
  PA_EXPORT_IF_DCHECK_IS_ON()
  void Check(const PartitionRoot* root) const PA_EMPTY_BODY_IF_DCHECK_IS_OFF();

  PA_ALWAYS_INLINE constexpr UntaggedSlotStart Untag() const;

  template <typename T = unsigned char>
  PA_ALWAYS_INLINE T* ToObject() const {
    return reinterpret_cast<T*>(address_);
  }

  PA_ALWAYS_INLINE constexpr bool operator==(SlotStart other) const {
    return value() == other.value();
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(SlotStart other) const {
    return !(*this == other);
  }

  PA_ALWAYS_INLINE constexpr bool operator==(uintptr_t other) const {
    return value() == other;
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(uintptr_t other) const {
    return value() != other;
  }

 private:
  PA_ALWAYS_INLINE explicit constexpr SlotStart(uintptr_t address)
      : address_(address) {}

  uintptr_t address_ = 0;
};

static_assert(sizeof(SlotStart) == sizeof(void*));
static_assert(std::is_trivially_copyable_v<SlotStart>);
static_assert(std::is_trivially_destructible_v<SlotStart>);

// Represents an address of a slot start, MTE-untagged.
// This type should be used when the MTE tag bits are not relevant or have been
// explicitly removed. It provides methods for checked and unchecked
// construction, as well as tagging.
class UntaggedSlotStart {
 public:
  PA_ALWAYS_INLINE constexpr UntaggedSlotStart() = default;

  // `UntaggedSlotStart::Checked()` may perform runtime check to confirm that
  // it is indeed a slot start. Prefer this variant for untrusted input or
  // payload which can be corrupted by Use-after-Free etc.
  // `UntaggedSlotStart::Unchecked()` has no check at all and faster.
  static UntaggedSlotStart Checked(uintptr_t address,
                                   const PartitionRoot* root) {
    UntaggedSlotStart ret(address);
    ret.Check(root);
    return ret;
  }
  PA_ALWAYS_INLINE static constexpr UntaggedSlotStart Unchecked(
      uintptr_t address) {
    return UntaggedSlotStart(address);
  }

  PA_ALWAYS_INLINE explicit constexpr operator bool() const {
    return address_ != 0;
  }
  PA_ALWAYS_INLINE constexpr uintptr_t value() const { return address_; }
  PA_ALWAYS_INLINE constexpr ptrdiff_t offset(uintptr_t other) const {
    return static_cast<ptrdiff_t>(other - address_);
  }

  // Check if this indeed points to the beginning of a slot.
  PA_EXPORT_IF_DCHECK_IS_ON()
  void Check(const PartitionRoot* root) const PA_EMPTY_BODY_IF_DCHECK_IS_OFF();

  PA_ALWAYS_INLINE SlotStart Tag() const;

  PA_ALWAYS_INLINE constexpr bool operator==(UntaggedSlotStart other) const {
    return value() == other.value();
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(UntaggedSlotStart other) const {
    return !(*this == other);
  }

  PA_ALWAYS_INLINE constexpr bool operator==(uintptr_t other) const {
    return value() == other;
  }
  PA_ALWAYS_INLINE constexpr bool operator!=(uintptr_t other) const {
    return value() != other;
  }

 private:
  PA_ALWAYS_INLINE explicit constexpr UntaggedSlotStart(uintptr_t address)
      : address_(address) {}

  uintptr_t address_ = 0;
};

static_assert(sizeof(UntaggedSlotStart) == sizeof(void*));
static_assert(std::is_trivially_copyable_v<UntaggedSlotStart>);
static_assert(std::is_trivially_destructible_v<UntaggedSlotStart>);

constexpr ptrdiff_t SlotSpanStart::offset(UntaggedSlotStart other) const {
  return offset(other.value());
}

constexpr UntaggedSlotStart SlotSpanStart::AsSlotStart() const {
  return UntaggedSlotStart::Unchecked(address_);
}

constexpr UntaggedSlotStart SlotSpanStart::GetNthSlotStart(
    size_t n,
    size_t slot_size) const {
  return UntaggedSlotStart::Unchecked(address_ + n * slot_size);
}

SlotStart UntaggedSlotStart::Tag() const {
  return SlotStart::Unchecked(reinterpret_cast<uintptr_t>(TagAddr(address_)));
}

constexpr UntaggedSlotStart SlotStart::Untag() const {
  return UntaggedSlotStart::Unchecked(UntagAddr(address_));
}

}  // namespace internal
}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SLOT_START_H_
