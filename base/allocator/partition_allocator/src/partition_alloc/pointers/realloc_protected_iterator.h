// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IWYU pragma: private, include "base/containers/realloc_protected_iterator.h"

#ifndef PARTITION_ALLOC_POINTERS_REALLOC_PROTECTED_ITERATOR_H_
#define PARTITION_ALLOC_POINTERS_REALLOC_PROTECTED_ITERATOR_H_

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>

#include "partition_alloc/partition_alloc_base/augmentations/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/types/to_address.h"
#include "partition_alloc/pointers/raw_ptr_exclusion.h"

namespace base {

namespace internal {

// Opaque handle to a PartitionAlloc slot that the wrapper has pinned.
// `slot_size == 0` means no slot is held (either the underlying allocation
// was not in a BRP-protected partition, or the wrapper is moved-from /
// default-constructed). Stored by value inside ReallocProtectedIterator.
struct WrappedBackingSlot {
  uintptr_t slot_start = 0;
  size_t slot_size = 0;

  explicit operator bool() const { return slot_size != 0; }
};

// Pins the BRP-protected slot containing `p`, returning a handle that must
// be released via UnwrapBackingSlot. If `p` is null or not inside a
// BRP-protected partition, the returned handle is empty (slot_size == 0) and
// UnwrapBackingSlot is a no-op.
PA_COMPONENT_EXPORT(RAW_PTR)
WrappedBackingSlot WrapBackingSlot(const void* p);

// Releases a slot previously pinned by WrapBackingSlot. Safe to call with an
// empty handle. CHECKs if the backing was freed while the wrapper held its
// ref (a realloc-during-iteration UAF), turning the bug into a fail-fast
// crash instead of a silent zapped-byte read.
PA_COMPONENT_EXPORT(RAW_PTR)
void UnwrapBackingSlot(WrappedBackingSlot slot);

}  // namespace internal

// Iterator wrapper that pins the containing allocation slot via BRP while
// the wrapper is alive. While at least one ReallocProtectedIterator points
// at a backing, freeing the backing puts it into BRP quarantine (zapped,
// deferred reuse) rather than letting it be immediately recycled.
//
// This mitigates UAF exploits that target the window between a container
// reallocating its backing and a still-live iterator dereferencing it.
//
// Hot path (deref, advance, compare) forwards directly to the inner iterator
// and inlines to the same instructions. The cost is paid at
// construction/destruction.
//
// WHEN TO USE: any time you keep a container iterator alive across operations
// that could trigger container reallocation. Common cases:
//
//  - Loops that add/insert/erase elements while iterating:
//      for (auto& x : realloc_protected_range(v)) { v.push_back(...); }
//
//  - Iterators that escape into code that may outlive container changes
//    (callbacks, async work):
//      auto it = realloc_protected_begin(v);
//      RunCallback([it] { ... });
//
//  - Iterators stored as a member variable across multiple operations on
//    the container:
//      class Cursor {
//        ReallocProtectedIterator<std::vector<T>::iterator> pos_;
//        ...
//      };
//
// THREAD SAFETY: This wrapper does not add thread-safety to container
// iteration. Standard library containers (std::vector, std::string, etc.)
// are not thread-safe; concurrent access with at least one mutation is
// undefined behavior regardless of whether iterators are wrapped. The
// wrapper's internal BRP ref-count operations are themselves atomic (same
// primitive raw_ptr<T> uses across threads), but that only protects the
// ref-count -- not the underlying container's data structures. Treat
// wrapped iterators the same way you treat the iterators they wrap.
template <typename Inner>
class ReallocProtectedIterator {
 public:
  using iterator_category =
      typename std::iterator_traits<Inner>::iterator_category;
  using value_type = typename std::iterator_traits<Inner>::value_type;
  using difference_type = typename std::iterator_traits<Inner>::difference_type;
  using pointer = typename std::iterator_traits<Inner>::pointer;
  using reference = typename std::iterator_traits<Inner>::reference;
  // TODO(crbug.com/345556942): Currently restricted to contiguous iterators
  // because Acquire() uses std::to_address. Non-contiguous iterators (e.g.
  // std::unordered_map's bucket iterator) could be supported by also pinning
  // the bucket-array allocation; that requires acquiring/releasing the slot
  // ref on each advance and is a meaningful change to the wrapper's contract.
  using iterator_concept = std::contiguous_iterator_tag;

  constexpr ReallocProtectedIterator() = default;

  explicit ReallocProtectedIterator(Inner inner) : inner_(inner) { Acquire(); }

  ReallocProtectedIterator(const ReallocProtectedIterator& other)
      : inner_(other.inner_) {
    Acquire();
  }

  ReallocProtectedIterator(ReallocProtectedIterator&& other) noexcept
      : inner_(other.inner_), slot_(other.slot_) {
    other.slot_ = {};
  }

  ~ReallocProtectedIterator() { internal::UnwrapBackingSlot(slot_); }

  ReallocProtectedIterator& operator=(const ReallocProtectedIterator& other) {
    if (this != &other) {
      internal::UnwrapBackingSlot(slot_);
      inner_ = other.inner_;
      slot_ = {};
      Acquire();
    }
    return *this;
  }

  ReallocProtectedIterator& operator=(
      ReallocProtectedIterator&& other) noexcept {
    if (this != &other) {
      internal::UnwrapBackingSlot(slot_);
      inner_ = other.inner_;
      slot_ = other.slot_;
      other.slot_ = {};
    }
    return *this;
  }

  // Hot path: pure forwarding to `inner_`. Operations that perform iterator
  // arithmetic are wrapped in PA_UNSAFE_BUFFERS because `Inner` may be a raw
  // pointer (where bounds are not statically tracked); for richer iterators
  // (e.g. `std::vector::iterator`) these are no-ops.
  reference operator*() const
    requires requires(const Inner& i) { *i; }
  {
    return *inner_;
  }

  pointer operator->() const
    requires requires(const Inner& i) {
      partition_alloc::internal::base::to_address(i);
    }
  {
    return partition_alloc::internal::base::to_address(inner_);
  }

  reference operator[](difference_type n) const
    requires requires(const Inner& i, difference_type d) { i[d]; }
  {
    // SAFETY: Inner is a contiguous iterator. Forwarding the indexing
    // operation to the underlying iterator is as safe as using the iterator
    // directly.
    return PA_UNSAFE_BUFFERS(inner_[n]);
  }

  ReallocProtectedIterator& operator++()
    requires requires(Inner& i) { ++i; }
  {
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(++inner_);
    return *this;
  }

  ReallocProtectedIterator operator++(int)
    requires requires(Inner& i) { i++; }
  {
    ReallocProtectedIterator tmp(*this);
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(++inner_);
    return tmp;
  }

  ReallocProtectedIterator& operator--()
    requires requires(Inner& i) { --i; }
  {
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(--inner_);
    return *this;
  }

  ReallocProtectedIterator operator--(int)
    requires requires(Inner& i) { i--; }
  {
    ReallocProtectedIterator tmp(*this);
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(--inner_);
    return tmp;
  }

  ReallocProtectedIterator& operator+=(difference_type n)
    requires requires(Inner& i, difference_type d) { i += d; }
  {
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(inner_ += n);
    return *this;
  }

  ReallocProtectedIterator& operator-=(difference_type n)
    requires requires(Inner& i, difference_type d) { i -= d; }
  {
    // SAFETY: Advancing the underlying contiguous iterator is safe.
    PA_UNSAFE_BUFFERS(inner_ -= n);
    return *this;
  }

  friend ReallocProtectedIterator operator+(ReallocProtectedIterator it,
                                            difference_type n)
    requires requires(Inner& i, difference_type d) { i += d; }
  {
    it += n;
    return it;
  }

  friend ReallocProtectedIterator operator+(difference_type n,
                                            ReallocProtectedIterator it)
    requires requires(Inner& i, difference_type d) { i += d; }
  {
    it += n;
    return it;
  }

  friend ReallocProtectedIterator operator-(ReallocProtectedIterator it,
                                            difference_type n)
    requires requires(Inner& i, difference_type d) { i -= d; }
  {
    it -= n;
    return it;
  }

  friend difference_type operator-(const ReallocProtectedIterator& a,
                                   const ReallocProtectedIterator& b)
    requires requires(const Inner& x, const Inner& y) { x - y; }
  {
    // SAFETY: Computing the distance between two contiguous iterators is safe.
    return PA_UNSAFE_BUFFERS(a.inner_ - b.inner_);
  }

  friend bool operator==(const ReallocProtectedIterator& a,
                         const ReallocProtectedIterator& b)
    requires std::equality_comparable<Inner>
  {
    return a.inner_ == b.inner_;
  }

  // Only declare <=> when the underlying iterator supports it. For the
  // contiguous_iterator concept (currently required) <=> is always present;
  // the constraint matters once we extend to non-contiguous (see TODO above).
  friend auto operator<=>(const ReallocProtectedIterator& a,
                          const ReallocProtectedIterator& b)
    requires std::three_way_comparable<Inner>
  {
    return a.inner_ <=> b.inner_;
  }

  // Read-only access to the inner iterator. Useful for interop with code that
  // expects the underlying iterator type.
  const Inner& base() const { return inner_; }

 private:
  void Acquire() {
    // `partition_alloc::internal::base::to_address` works for both raw pointers
    // and standard contiguous iterators.
    if (auto* p = partition_alloc::internal::base::to_address(inner_)) {
      slot_ = internal::WrapBackingSlot(p);
    }
  }

  Inner inner_{};
  internal::WrappedBackingSlot slot_{};
};

// Free helpers for ergonomic opt-in at escape points.
template <typename C>
auto realloc_protected_begin(C& c) {
  return ReallocProtectedIterator<decltype(c.begin())>(c.begin());
}

template <typename C>
auto realloc_protected_end(C& c) {
  return ReallocProtectedIterator<decltype(c.end())>(c.end());
}

// Range adapter:
//   for (auto& x : realloc_protected_range(v)) { ... }
template <typename C>
class ReallocProtectedRange {
 public:
  explicit ReallocProtectedRange(C&& c PA_LIFETIME_BOUND)
      : c_(std::forward<C>(c)) {}
  ReallocProtectedRange(const ReallocProtectedRange&) = default;
  ReallocProtectedRange& operator=(const ReallocProtectedRange&) = delete;

  auto begin() { return realloc_protected_begin(c_); }
  auto end() { return realloc_protected_end(c_); }

 private:
  // RAW_PTR_EXCLUSION: This adapter is only used inside for loops, mirroring
  // base::internal::ReversedAdapter. The container is held via a native
  // reference because the adapter is never stored across operations that
  // could invalidate it. Use ReallocProtectedIterator directly if you need
  // to store an iterator across calls.
  RAW_PTR_EXCLUSION C&& c_;
};

template <typename C>
auto realloc_protected_range(C&& c PA_LIFETIME_BOUND) {
  return ReallocProtectedRange<C>(std::forward<C>(c));
}

}  // namespace base

#endif  // PARTITION_ALLOC_POINTERS_REALLOC_PROTECTED_ITERATOR_H_
