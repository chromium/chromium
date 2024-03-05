// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_HEAP_ARRAY_H_
#define BASE_CONTAINERS_HEAP_ARRAY_H_

#include <stddef.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

// HeapArray<T> is a replacement for std::unique_ptr<T[]> that keeps track
// of its size. It is intended to provide easy conversion to span<T> for most
// usage, but it also provides bounds-checked indexing.
//
// By default, elements in the array are either value-initialized (i.e. zeroed
// for primitive types) when the array is created using the WithSize()
// static method, or uninitialized when the array is created via the Uninit()
// static method.
template <typename T>
class TRIVIAL_ABI GSL_OWNER HeapArray {
 public:
  static_assert(!std::is_const_v<T>, "HeapArray cannot hold const types");
  static_assert(!std::is_reference_v<T>,
                "HeapArray cannot hold reference types");

  using iterator = base::span<T>::iterator;
  using const_iterator = base::span<const T>::iterator;

  // Allocates initialized memory capable of holding `size` elements. No memory
  // is allocated for zero-sized arrays.
  static HeapArray WithSize(size_t size)
    requires(std::constructible_from<T>)
  {
    if (!size) {
      return HeapArray();
    }
    return HeapArray(std::unique_ptr<T[]>(new T[size]()), size);
  }

  // Allocates uninitialized memory capable of holding `size` elements. T must
  // be trivially constructible and destructible. No memory is allocated for
  // zero-sized arrays.
  static HeapArray Uninit(size_t size)
    requires(std::is_trivially_constructible_v<T> &&
             std::is_trivially_destructible_v<T>)
  {
    if (!size) {
      return HeapArray();
    }
    return HeapArray(std::unique_ptr<T[]>(new T[size]), size);
  }

  static HeapArray CopiedFrom(base::span<const T> that) {
    auto result = HeapArray::Uninit(that.size());
    result.copy_from(that);
    return result;
  }

  // Constructs an empty array and does not allocate any memory.
  HeapArray()
    requires(std::constructible_from<T>)
  = default;

  // Move-only type since the memory is owned.
  HeapArray(const HeapArray&) = delete;
  HeapArray& operator=(const HeapArray&) = delete;

  // Move-construction leaves the moved-from object empty and containing
  // no allocated memory.
  HeapArray(HeapArray&& that)
      : data_(std::move(that.data_)), size_(std::exchange(that.size_, 0u)) {}

  // Move-assigment leaves the moved-from object empty and containing
  // no allocated memory.
  HeapArray& operator=(HeapArray&& that) {
    data_ = std::move(that.data_);
    size_ = std::exchange(that.size_, 0u);
    return *this;
  }
  ~HeapArray() = default;

  bool empty() const { return size_ == 0u; }
  size_t size() const { return size_; }

  // Prefer span-based methods below over data() where possible. The data()
  // method exists primarily to allow implicit constructions of spans.
  // Returns nullptr for a zero-sized (or moved-from) array.
  T* data() ABSL_ATTRIBUTE_LIFETIME_BOUND { return data_.get(); }
  const T* data() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return data_.get(); }

  iterator begin() ABSL_ATTRIBUTE_LIFETIME_BOUND { return as_span().begin(); }
  const_iterator begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().begin();
  }

  iterator end() ABSL_ATTRIBUTE_LIFETIME_BOUND { return as_span().end(); }
  const_iterator end() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().end();
  }

  T& operator[](size_t idx) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span()[idx];
  }
  const T& operator[](size_t idx) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span()[idx];
  }

  // Access the HeapArray via spans. Note that span<T> is implicilty
  // constructible from HeapArray<T>, so an explicit call to .as_span() is
  // most useful, say, when the compiler can't deduce a template
  // argument type.
  base::span<T> as_span() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base::span<T>(data_.get(), size_);
  }
  base::span<const T> as_span() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return base::span<const T>(data_.get(), size_);
  }

  // Convenience method to copy the contents of the entire array from a
  // span<>. Hard CHECK occurs in span<>::copy_from() if the HeapArray and
  // the span have different sizes.
  void copy_from(base::span<const T> other) { as_span().copy_from(other); }

  // Convenience methods to slice the vector into spans.
  // Returns a span over the HeapArray starting at `offset` of `count` elements.
  // If `count` is unspecified, all remaining elements are included. A CHECK()
  // occurs if any of the parameters results in an out-of-range position in
  // the HeapArray.
  base::span<T> subspan(size_t offset, size_t count = base::dynamic_extent)
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().subspan(offset, count);
  }
  base::span<const T> subspan(size_t offset,
                              size_t count = base::dynamic_extent) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().subspan(offset, count);
  }

  // Returns a span over the first `count` elements of the HeapArray. A CHECK()
  // occurs if the `count` is larger than size of the HeapArray.
  base::span<T> first(size_t count) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().first(count);
  }
  base::span<const T> first(size_t count) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().first(count);
  }

  // Returns a span over the last `count` elements of the HeapArray. A CHECK()
  // occurs if the `count` is larger than size of the HeapArray.
  base::span<T> last(size_t count) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().last(count);
  }
  base::span<const T> last(size_t count) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return as_span().last(count);
  }

  // Leaks the memory in the HeapArray so that it will never be freed, and
  // consumes the HeapArray, returning an unowning span that points to the
  // memory.
  base::span<T> leak() && {
    HeapArray<T> dropped = std::move(*this);
    T* leaked = dropped.data_.release();
    return make_span(leaked, dropped.size_);
  }

  // Delete the memory previously obtained from leak(). Argument is a pointer
  // rather than a span to facilitate use by callers that have lost track of
  // size information, as may happen when being passed through a C-style
  // function callback. The void* argument type makes its signature compatible
  // with typical void (*cb)(void*) C-style deletion callback.
  static void DeleteLeakedData(void* ptr) {
    // Memory is freed by unique ptr going out of scope.
    std::unique_ptr<T[]> deleter(static_cast<T*>(ptr));
  }

 private:
  HeapArray(std::unique_ptr<T[]> data, size_t size)
      : data_(std::move(data)), size_(size) {}

  std::unique_ptr<T[]> data_;
  size_t size_ = 0u;
};

}  // namespace base

#endif  // BASE_CONTAINERS_HEAP_ARRAY_H_
