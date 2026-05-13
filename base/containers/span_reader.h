// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_READER_H_
#define BASE_CONTAINERS_SPAN_READER_H_

#include <concepts>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"

namespace base {

// A Reader to consume elements from the front of a span dynamically.
//
// SpanReader is used to split off prefix spans from a larger span, reporting
// errors if there's not enough room left (instead of crashing, as would happen
// with span directly).
template <class T>
class SpanReaderBase {
  STACK_ALLOCATED();

 public:
  // Construct an empty SpanReader.
  constexpr SpanReaderBase() : original_size_(0) {}

  // Construct SpanReader from a span.
  constexpr explicit SpanReaderBase(span<T> buf)
      : buf_(buf), original_size_(buf_.size()) {}

  // Returns a span over the next `n` objects, if there are enough objects left.
  // Otherwise, it returns nullopt and does nothing.
  constexpr std::optional<span<T>> Read(StrictNumeric<size_t> n) {
    if (n > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    buf_ = rhs;
    return lhs;
  }

  // Returns a fixed-size span over the next `N` objects, if there are enough
  // objects left. Otherwise, it returns nullopt and does nothing.
  template <size_t N>
  constexpr std::optional<span<T, N>> Read() {
    if (N > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.template split_at<N>();
    buf_ = rhs;
    return lhs;
  }

  // Returns true and writes a span over the next `n` objects into `out`, if
  // there are enough objects left. Otherwise, it returns false and does
  // nothing.
  constexpr bool ReadInto(StrictNumeric<size_t> n, span<T>& out) {
    if (n > remaining()) {
      return false;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    out = lhs;
    buf_ = rhs;
    return true;
  }

  // Returns true and copies objects into `out`, if there are enough objects
  // left to fill `out`. Otherwise, it returns false and does nothing.
  constexpr bool ReadCopy(span<std::remove_const_t<T>> out) {
    if (out.size() > remaining()) {
      return false;
    }
    auto [lhs, rhs] = buf_.split_at(out.size());
    out.copy_from(lhs);
    buf_ = rhs;
    return true;
  }

  // Returns true and skips over the next `n` objects, if there are enough
  // objects left. Otherwise, it returns false and does nothing.
  constexpr std::optional<span<T>> Skip(StrictNumeric<size_t> n) {
    if (n > remaining()) {
      return std::nullopt;
    }
    auto [lhs, rhs] = buf_.split_at(n);
    buf_ = rhs;
    return lhs;
  }

  // Returns the number of objects remaining to be read from the original span.
  constexpr size_t remaining() const { return buf_.size(); }
  // Returns the objects that have not yet been read, as a span.
  constexpr span<T> remaining_span() const { return buf_; }

  // Returns the number of objects read (or skipped) in the original span.
  constexpr size_t num_read() const { return original_size_ - buf_.size(); }

 protected:
  span<T> buf_;
  size_t original_size_;
};

// Primary template for SpanReader.
template <class T>
class SpanReader : public SpanReaderBase<T> {
 public:
  using SpanReaderBase<T>::SpanReaderBase;
};

// Partial specialization for byte types, allowing Big/Little/NativeEndian
// reads.
template <class T>
  requires(std::same_as<std::remove_const_t<T>, uint8_t>)
class SpanReader<T> : public SpanReaderBase<T> {
 public:
  using SpanReaderBase<T>::SpanReaderBase;

  // For a SpanReader over bytes, we can read integer values directly from those
  // bytes as a memcpy. The macros below implement the following methods:
  //
  // std::optional<uint8_t> ReadU8BigEndian()
  // bool ReadU8BigEndian(uint8_t& v)
  // std::optional<uint16_t> ReadU16BigEndian()
  // bool ReadU16BigEndian(uint16_t& v)
  // std::optional<uint32_t> ReadU32BigEndian()
  // bool ReadU32BigEndian(uint32_t& v)
  // std::optional<uint64_t> ReadU64BigEndian()
  // bool ReadU64BigEndian(uint64_t& v)
  // std::optional<int8_t> ReadI8BigEndian()
  // bool ReadI8BigEndian(int8_t& v)
  // std::optional<int16_t> ReadI16BigEndian()
  // bool ReadI16BigEndian(int16_t& v)
  // std::optional<int32_t> ReadI32BigEndian()
  // bool ReadI32BigEndian(int32_t& v)
  // std::optional<int64_t> ReadI64BigEndian()
  // bool ReadI64BigEndian(int64_t& v)
  //
  // And similar for Little and Native endianness.
#define BASE_SPANREADER_BOOL_FROM_OPTIONAL(name)                              \
  template <typename U>                                                       \
  constexpr bool Read##name(U& v) {                                           \
    return Read##name().transform([&](auto u) { return v = u; }).has_value(); \
  }

#define BASE_SPANREADER_READ(signchar, bitsize, endian, typeprefix) \
  constexpr std::optional<typeprefix##int##bitsize##_t>             \
      Read##signchar##bitsize##endian##Endian() {                   \
    return this->template Read<bitsize / 8>().transform(            \
        &signchar##bitsize##From##endian##Endian);                  \
  }                                                                 \
  BASE_SPANREADER_BOOL_FROM_OPTIONAL(signchar##bitsize##endian##Endian)

#define BASE_SPANREADER_READ_BOTH_SIGNS(bitsize, endian) \
  BASE_SPANREADER_READ(U, bitsize, endian, u)            \
  BASE_SPANREADER_READ(I, bitsize, endian, )

#define BASE_SPANREADER_READ_BOTH_SIGNS_ALL_SIZES(endian) \
  BASE_SPANREADER_READ_BOTH_SIGNS(8, endian)              \
  BASE_SPANREADER_READ_BOTH_SIGNS(16, endian)             \
  BASE_SPANREADER_READ_BOTH_SIGNS(32, endian)             \
  BASE_SPANREADER_READ_BOTH_SIGNS(64, endian)

  BASE_SPANREADER_READ_BOTH_SIGNS_ALL_SIZES(Big)
  BASE_SPANREADER_READ_BOTH_SIGNS_ALL_SIZES(Little)
  BASE_SPANREADER_READ_BOTH_SIGNS_ALL_SIZES(Native)

#undef BASE_SPANREADER_READ_BOTH_SIGNS_ALL_SIZES
#undef BASE_SPANREADER_READ_BOTH_SIGNS
#undef BASE_SPANREADER_READ
  // The macros below implement the following methods for non-integer types:
  //
  // std::optional<char> ReadChar()
  // bool ReadChar(char& v)
  // std::optional<float> ReadFloatNativeEndian()
  // bool ReadFloatNativeEndian(float& v)
  // std::optional<double> ReadDoubleNativeEndian()
  // bool ReadDoubleNativeEndian(double& v)

#define BASE_SPANREADER_READ_NON_INT(type, name, converter)          \
  constexpr std::optional<type> Read##name() {                       \
    return this->template Read<sizeof(type)>().transform(converter); \
  }                                                                  \
  BASE_SPANREADER_BOOL_FROM_OPTIONAL(name)

  BASE_SPANREADER_READ_NON_INT(char, Char, [](auto buf) {
    return static_cast<char>(buf[0u]);
  })
  BASE_SPANREADER_READ_NON_INT(float, FloatNativeEndian, &FloatFromNativeEndian)
  BASE_SPANREADER_READ_NON_INT(double,
                               DoubleNativeEndian,
                               &DoubleFromNativeEndian)

#undef BASE_SPANREADER_READ_NON_INT
#undef BASE_SPANREADER_BOOL_FROM_OPTIONAL
};

template <typename ElementType, size_t Extent, typename InternalPtrType>
SpanReader(span<ElementType, Extent, InternalPtrType>)
    -> SpanReader<ElementType>;

}  // namespace base

#endif  // BASE_CONTAINERS_SPAN_READER_H_
