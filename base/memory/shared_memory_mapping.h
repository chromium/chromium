// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_MAPPING_H_
#define BASE_MEMORY_SHARED_MEMORY_MAPPING_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

#include "base/base_export.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/unguessable_token.h"

namespace base {

namespace subtle {

class PlatformSharedMemoryRegion;

// Constraints on types that are safe to copy across memory spaces. This is a
// non-exhaustive list and further constraints may be added in the future.

// `kIsAllowed` is true unless T is known to be dangerous over shared memory.
template <typename T>
struct SharedMemorySafetyChecker {
  // Copying non-trivially-copyable objects across memory spaces is dangerous.
  // This check isn't a separate specialization because many types that match
  // other specializations are also trivially copyable, introducing ambiguity.
  static constexpr bool kIsAllowed = std::is_trivially_copyable_v<T>;
};

// Pointers can't be shared across memory spaces.
template <typename T>
  requires(std::is_pointer_v<T> || std::is_member_pointer_v<T>)
struct SharedMemorySafetyChecker<T> {
  static constexpr bool kIsAllowed = false;
};

// Atomics are dangerous to share across memory spaces unless they're lock-free.
template <typename T>
struct SharedMemorySafetyChecker<std::atomic<T>> {
  static constexpr bool kIsAllowed = std::atomic<T>::is_always_lock_free &&
                                     SharedMemorySafetyChecker<T>::kIsAllowed;
};

// Each element of an array must itself be safe. Although arrays aren't outright
// banned, prefer to use GetMemoryAsSpan<T> for array-like access.
template <typename T, size_t N>
struct SharedMemorySafetyChecker<T[N]> {
  static constexpr bool kIsAllowed = SharedMemorySafetyChecker<T>::kIsAllowed;
};

template <typename T, size_t N>
struct SharedMemorySafetyChecker<std::array<T, N>> {
  static constexpr bool kIsAllowed = SharedMemorySafetyChecker<T>::kIsAllowed;
};

template <typename T>
concept AllowedOverSharedMemory = SharedMemorySafetyChecker<T>::kIsAllowed;

}  // namespace subtle

// Base class for scoped handles to a shared memory mapping created from a
// shared memory region. Created shared memory mappings remain valid even if the
// creator region is transferred or destroyed.
//
// Each mapping has an UnguessableToken that identifies the shared memory region
// it was created from. This is used for memory metrics, to avoid overcounting
// shared memory.
class BASE_EXPORT SharedMemoryMapping {
 public:
  // Default constructor initializes an invalid instance.
  SharedMemoryMapping();

  // Move operations are allowed.
  SharedMemoryMapping(SharedMemoryMapping&& mapping) noexcept;
  SharedMemoryMapping& operator=(SharedMemoryMapping&& mapping) noexcept;

  SharedMemoryMapping(const SharedMemoryMapping&) = delete;
  SharedMemoryMapping& operator=(const SharedMemoryMapping&) = delete;

  // Unmaps the region if the mapping is valid.
  virtual ~SharedMemoryMapping();

  // Returns true iff the mapping is valid. False means there is no
  // corresponding area of memory.
  bool IsValid() const { return !mapped_span_.empty(); }

  // Returns the logical size of the mapping in bytes. This is precisely the
  // size requested by whoever created the mapping, and it is always less than
  // or equal to |mapped_size()|. This is undefined for invalid instances.
  size_t size() const {
    DCHECK(IsValid());
    return size_;
  }

  // Returns the actual size of the mapping in bytes. This is always at least
  // as large as |size()| but may be larger due to platform mapping alignment
  // constraints. This is undefined for invalid instances.
  size_t mapped_size() const {
    DCHECK(IsValid());
    return mapped_span_.size();
  }

  // Returns 128-bit GUID of the region this mapping belongs to.
  const UnguessableToken& guid() const {
    DCHECK(IsValid());
    return guid_;
  }

 protected:
  SharedMemoryMapping(span<uint8_t> mapped_span,
                      size_t size,
                      const UnguessableToken& guid,
                      SharedMemoryMapper* mapper);

  // Returns a span over the full mapped memory.
  span<uint8_t> mapped_memory() const { return mapped_span_; }

 private:
  friend class SharedMemoryTracker;

  void Unmap();

  span<uint8_t> mapped_span_;
  size_t size_ = 0;
  UnguessableToken guid_;
  raw_ptr<SharedMemoryMapper> mapper_ = nullptr;
};

// Class modeling a read-only mapping of a shared memory region into the
// current process' address space. This is created by ReadOnlySharedMemoryRegion
// instances.
class BASE_EXPORT ReadOnlySharedMemoryMapping : public SharedMemoryMapping {
 public:
  // Default constructor initializes an invalid instance.
  ReadOnlySharedMemoryMapping();

  ReadOnlySharedMemoryMapping(const ReadOnlySharedMemoryMapping&) = delete;
  ReadOnlySharedMemoryMapping& operator=(const ReadOnlySharedMemoryMapping&) =
      delete;

  // Move operations are allowed.
  ReadOnlySharedMemoryMapping(ReadOnlySharedMemoryMapping&&) noexcept;
  ReadOnlySharedMemoryMapping& operator=(
      ReadOnlySharedMemoryMapping&&) noexcept;

  // Returns the base address of the read-only mapping. Returns nullptr for
  // invalid instances.
  //
  // Use `span(mapping)` to make a span of `uint8_t`, `GetMemoryAs<T>()` to
  // access the memory as a single `T` or `GetMemoryAsSpan<T>()` to access it as
  // an array of `T`.
  const uint8_t* data() const { return mapped_memory().data(); }

  // TODO(crbug.com/355451178): Deprecated. Use `span(mapping)` to make a span
  // of `uint8_t`, `GetMemoryAs<T>()` to access the memory as a single `T` or
  // `GetMemoryAsSpan<T>()` to access it as an array of `T`, or `data()` for an
  // unbounded pointer.
  const void* memory() const { return data(); }

  // Returns a pointer to a page-aligned const T if the mapping is valid and
  // large enough to contain a T, or nullptr otherwise.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  const T* GetMemoryAs() const {
    if (!IsValid())
      return nullptr;
    if (sizeof(T) > size())
      return nullptr;
    return reinterpret_cast<const T*>(mapped_memory().data());
  }

  // Returns a span of const T. The number of elements is autodeduced from the
  // size of the shared memory mapping. The number of elements may be
  // autodeduced as zero, i.e. the mapping is invalid or the size of the mapping
  // isn't large enough to contain even one T: in that case, an empty span
  // will be returned. The first element, if any, is guaranteed to be
  // page-aligned.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  span<const T> GetMemoryAsSpan() const {
    if (!IsValid())
      return span<const T>();
    size_t count = size() / sizeof(T);
    return GetMemoryAsSpan<T>(count);
  }

  // Returns a span of const T with |count| elements if the mapping is valid and
  // large enough to contain |count| elements, or an empty span otherwise. The
  // first element, if any, is guaranteed to be page-aligned.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  span<const T> GetMemoryAsSpan(size_t count) const {
    if (!IsValid())
      return span<const T>();
    if (size() / sizeof(T) < count)
      return span<const T>();
    // SAFETY: There is an internal invariant (enforced in the constructors)
    // that `size() <= mapped_memory().size()`. `count` is the number of objects
    // of type T that fit within size(), so the pointer given to span() points
    // to at least that many T objects.
    return UNSAFE_BUFFERS(
        span(reinterpret_cast<const T*>(mapped_memory().data()), count));
  }

 private:
  friend class ReadOnlySharedMemoryRegion;
  ReadOnlySharedMemoryMapping(span<uint8_t> mapped_span,
                              size_t size,
                              const UnguessableToken& guid,
                              SharedMemoryMapper* mapper);
};

// Class modeling a writable mapping of a shared memory region into the
// current process' address space. This is created by *SharedMemoryRegion
// instances.
class BASE_EXPORT WritableSharedMemoryMapping : public SharedMemoryMapping {
 public:
  // Default constructor initializes an invalid instance.
  WritableSharedMemoryMapping();

  WritableSharedMemoryMapping(const WritableSharedMemoryMapping&) = delete;
  WritableSharedMemoryMapping& operator=(const WritableSharedMemoryMapping&) =
      delete;

  // Move operations are allowed.
  WritableSharedMemoryMapping(WritableSharedMemoryMapping&&) noexcept;
  WritableSharedMemoryMapping& operator=(
      WritableSharedMemoryMapping&&) noexcept;

  // Returns the base address of the writable mapping. Returns nullptr for
  // invalid instances.
  //
  // Use `span(mapping)` to make a span of `uint8_t`, `GetMemoryAs<T>()` to
  // access the memory as a single `T` or `GetMemoryAsSpan<T>()` to access it as
  // an array of `T`.
  uint8_t* data() const { return mapped_memory().data(); }

  // TODO(crbug.com/355451178): Deprecated. Use `span(mapping)` to make a span
  // of `uint8_t`, `GetMemoryAs<T>()` to access the memory as a single `T`, or
  // `GetMemoryAsSpan<T>()` to access it as an array of `T` or `data()` for an
  // unbounded pointer.
  void* memory() const { return data(); }

  // Returns a pointer to a page-aligned T if the mapping is valid and large
  // enough to contain a T, or nullptr otherwise.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  T* GetMemoryAs() const {
    if (!IsValid())
      return nullptr;
    if (sizeof(T) > size())
      return nullptr;
    return reinterpret_cast<T*>(mapped_memory().data());
  }

  // Returns a span of T. The number of elements is autodeduced from the size of
  // the shared memory mapping. The number of elements may be autodeduced as
  // zero, i.e. the mapping is invalid or the size of the mapping isn't large
  // enough to contain even one T: in that case, an empty span will be returned.
  // The first element, if any, is guaranteed to be page-aligned.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  span<T> GetMemoryAsSpan() const {
    if (!IsValid())
      return span<T>();
    size_t count = size() / sizeof(T);
    return GetMemoryAsSpan<T>(count);
  }

  // Returns a span of T with |count| elements if the mapping is valid and large
  // enough to contain |count| elements, or an empty span otherwise. The first
  // element, if any, is guaranteed to be page-aligned.
  template <typename T>
    requires subtle::AllowedOverSharedMemory<T>
  span<T> GetMemoryAsSpan(size_t count) const {
    if (!IsValid())
      return span<T>();
    if (size() / sizeof(T) < count)
      return span<T>();
    // SAFETY: There is an internal invariant (enforced in the constructors)
    // that `size() <= mapped_memory().size()`. `count` is the number of objects
    // of type T that fit within size(), so the pointer given to span() points
    // to at least that many T objects.
    return UNSAFE_BUFFERS(
        span(reinterpret_cast<T*>(mapped_memory().data()), count));
  }

 private:
  friend WritableSharedMemoryMapping MapAtForTesting(
      subtle::PlatformSharedMemoryRegion* region,
      uint64_t offset,
      size_t size);
  friend class ReadOnlySharedMemoryRegion;
  friend class WritableSharedMemoryRegion;
  friend class UnsafeSharedMemoryRegion;
  WritableSharedMemoryMapping(span<uint8_t> mapped_span,
                              size_t size,
                              const UnguessableToken& guid,
                              SharedMemoryMapper* mapper);

  friend class DiscardableSharedMemory;  // Give access to mapped_memory().
};

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_MAPPING_H_
