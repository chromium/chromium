// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_WRITER_H_
#define BASE_CONTAINERS_SPAN_WRITER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/numerics/byte_conversions.h"

namespace base {

// A Writer to write into and consume elements from the front of a span
// dynamically.
//
// SpanWriter is used to split off prefix spans from a larger span, reporting
// errors if there's not enough room left (instead of crashing, as would happen
// with span directly).
template <class T>
class SpanWriter {
  static_assert(!std::is_const_v<T>,
                "SpanWriter needs mutable access to its buffer");

 public:
  // Construct SpanWriter that writes to `buf`.
  explicit SpanWriter(span<T> buf) : buf_(buf), original_size_(buf_.size()) {}

  // Returns true and writes the span `data` into the front of the inner span,
  // if there is enough room left. Otherwise, it returns false and does
  // nothing.
  bool Write(span<const T> data) {
    if (data.size() > remaining()) {
      return false;
    }
    auto [lhs, rhs] = buf_.split_at(data.size());
    lhs.copy_from(data);
    buf_ = rhs;
    return true;
  }

  // Returns true and writes `value` into the front of the inner span if there
  // is space remaining. Otherwise, it returns false and does nothing.
  template <typename V>
    requires(std::same_as<T, std::remove_cvref_t<V>>)
  bool Write(V&& value) {
    if (!remaining()) {
      return false;
    }
    buf_[0] = std::forward<V>(value);
    buf_ = buf_.last(remaining() - 1);
    return true;
  }

  // Skips over the next `n` objects, and returns a span that points to the
  // skipped objects, if there are enough objects left. Otherwise, it returns
  // nullopt and does nothing.
  std::optional<span<T>> Skip(StrictNumeric<size_t> n) {
    if (n > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    buf_ = rhs;
    return lhs;
  }
  // Skips over the next `N` objects, and returns a fixed-size span that points
  // to the skipped objects, if there are enough objects left. Otherwise, it
  // returns nullopt and does nothing.
  template <size_t N>
  std::optional<span<T, N>> Skip() {
    if (N > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.template split_at<N>();
    buf_ = rhs;
    return lhs;
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in big endian order.
  bool WriteU8BigEndian(uint8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U8ToBigEndian(value));
  }
  bool WriteU16BigEndian(uint16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U16ToBigEndian(value));
  }
  bool WriteU32BigEndian(uint32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U32ToBigEndian(value));
  }
  bool WriteU64BigEndian(uint64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U64ToBigEndian(value));
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in little endian order.
  bool WriteU8LittleEndian(uint8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U8ToLittleEndian(value));
  }
  bool WriteU16LittleEndian(uint16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U16ToLittleEndian(value));
  }
  bool WriteU32LittleEndian(uint32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U32ToLittleEndian(value));
  }
  bool WriteU64LittleEndian(uint64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U64ToLittleEndian(value));
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in native endian order. Note that this
  // is almost never what you want to do. Native ordering only makes sense for
  // byte buffers that are only meant to stay in memory and never be written to
  // the disk or network.
  bool WriteU8NativeEndian(uint8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U8ToNativeEndian(value));
  }
  bool WriteU16NativeEndian(uint16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U16ToNativeEndian(value));
  }
  bool WriteU32NativeEndian(uint32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U32ToNativeEndian(value));
  }
  bool WriteU64NativeEndian(uint64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(U64ToNativeEndian(value));
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in big endian order.
  bool WriteI8BigEndian(int8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I8ToBigEndian(value));
  }
  bool WriteI16BigEndian(int16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I16ToBigEndian(value));
  }
  bool WriteI32BigEndian(int32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I32ToBigEndian(value));
  }
  bool WriteI64BigEndian(int64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I64ToBigEndian(value));
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in little endian order.
  bool WriteI8LittleEndian(int8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I8ToLittleEndian(value));
  }
  bool WriteI16LittleEndian(int16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I16ToLittleEndian(value));
  }
  bool WriteI32LittleEndian(int32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I32ToLittleEndian(value));
  }
  bool WriteI64LittleEndian(int64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I64ToLittleEndian(value));
  }

  // For a SpanWriter over bytes, we can write integer values directly to those
  // bytes as a memcpy. Returns true if there was room remaining and the bytes
  // were written.
  //
  // These copy the bytes into the buffer in native endian order. Note that this
  // is almost never what you want to do. Native ordering only makes sense for
  // byte buffers that are only meant to stay in memory and never be written to
  // the disk or network.
  bool WriteI8NativeEndian(int8_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I8ToNativeEndian(value));
  }
  bool WriteI16NativeEndian(int16_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I16ToNativeEndian(value));
  }
  bool WriteI32NativeEndian(int32_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I32ToNativeEndian(value));
  }
  bool WriteI64NativeEndian(int64_t value)
    requires(std::same_as<T, uint8_t>)
  {
    return Write(I64ToNativeEndian(value));
  }

  // Returns the number of objects remaining to be written to the original span.
  size_t remaining() const { return buf_.size(); }
  // Returns the objects that have not yet been written to, as a span.
  span<T> remaining_span() const { return buf_; }

  // Returns the number of objects written (or skipped) in the original span.
  size_t num_written() const { return original_size_ - buf_.size(); }

 private:
  raw_span<T> buf_;
  size_t original_size_;
};

template <class T, size_t N>
SpanWriter(span<T, N>) -> SpanWriter<T>;

}  // namespace base

#endif  // BASE_CONTAINERS_SPAN_WRITER_H_
