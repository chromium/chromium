// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIG_ENDIAN_H_
#define BASE_BIG_ENDIAN_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <optional>
#include <type_traits>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/strings/string_piece.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"

namespace base {

namespace internal {

// ByteSwapIfLittleEndian performs ByteSwap if this platform is little-endian,
// otherwise it is a no-op.

#if defined(ARCH_CPU_LITTLE_ENDIAN)

template <typename T>
inline auto ByteSwapIfLittleEndian(T val) -> decltype(ByteSwap(val)) {
  return ByteSwap(val);
}

#else

// The use of decltype ensures this is only enabled for types for which
// ByteSwap() is defined, so the same set of overloads will work on both
// little-endian and big-endian platforms.

template <typename T>
inline auto ByteSwapIfLittleEndian(T val) -> decltype(ByteSwap(val)) {
  return val;
}

#endif

// We never need to byte-swap a single-byte value, but it's convenient to have
// this overload to avoid a special case.
inline uint8_t ByteSwapIfLittleEndian(uint8_t val) {
  return val;
}

}  // namespace internal

// Read an integer (signed or unsigned) from |buf| in Big Endian order.
// Note: this loop is unrolled with -O1 and above.
// NOTE(szym): glibc dns-canon.c use ntohs(*(uint16_t*)ptr) which is
// potentially unaligned.
// This would cause SIGBUS on ARMv5 or earlier and ARMv6-M.
//
// DEPRECATED: Use base::numerics::*FromBigEndian to convert big-endian byte
// encoding to primitives.
template <typename T>
inline void ReadBigEndian(span<const uint8_t, sizeof(T)> buffer, T* out) {
  static_assert(std::is_integral_v<T>, "T has to be an integral type.");
  // Make an unsigned version of the output type to make shift possible
  // without UB.
  std::make_unsigned_t<T> raw;
  byte_span_from_ref(raw).copy_from(buffer);
  *out = static_cast<T>(internal::ByteSwapIfLittleEndian(raw));
}

// TODO(crbug.com/40284755): Remove this function when there are no callers.
template <typename T>
inline void ReadBigEndian(const uint8_t buf[], T* out) {
  ReadBigEndian(span<const uint8_t, sizeof(T)>(buf, sizeof(T)), out);
}

// Write an integer (signed or unsigned) `val` to `buffer` in Big Endian order.
// The `buffer` must be the same size (in bytes) as the integer `val`.
//
// DEPRECATED: Use base::numerics::*ToBigEndian to convert primitives to big-
// endian byte encoding.
template <typename T>
  requires(std::is_integral_v<T>)
inline void WriteBigEndian(span<uint8_t, sizeof(T)> buffer, T val) {
  const auto unsigned_val = static_cast<std::make_unsigned_t<T>>(val);
  const auto raw = internal::ByteSwapIfLittleEndian(unsigned_val);
  buffer.copy_from(byte_span_from_ref(raw));
}

// TODO(crbug.com/40284755): Remove this function when there are no callers.
template <typename T>
  requires(std::is_integral_v<T>)
inline void WriteBigEndian(char buf[], T val) {
  return WriteBigEndian(
      as_writable_bytes(span<char, sizeof(T)>(buf, sizeof(T))), val);
}

// Allows reading integers in network order (big endian) while iterating over
// an underlying buffer. All the reading functions advance the internal pointer.
class BASE_EXPORT BigEndianReader {
 public:
  static BigEndianReader FromStringPiece(base::StringPiece string_piece);

  explicit BigEndianReader(base::span<const uint8_t> buffer);

  // TODO(crbug.com/40284755): Remove this overload.
  UNSAFE_BUFFER_USAGE BigEndianReader(const uint8_t* buf, size_t len);

  ~BigEndianReader();

  // Returns a span over all unread bytes.
  span<const uint8_t> remaining_bytes() const { return buffer_; }

  // TODO(crbug.com/40284755): Remove this method.
  const uint8_t* ptr() const { return buffer_.data(); }
  // TODO(crbug.com/40284755): Remove this method.
  size_t remaining() const { return buffer_.size(); }

  bool Skip(size_t len);
  bool ReadBytes(void* out, size_t len);
  // Creates a StringPiece in |out| that points to the underlying buffer.
  bool ReadPiece(base::StringPiece* out, size_t len);

  // Returns a span over `n` bytes from the buffer and moves the internal state
  // past those bytes, or returns nullopt and if there are not `n` bytes
  // remaining in the buffer.
  std::optional<span<const uint8_t>> ReadSpan(base::StrictNumeric<size_t> n);

  // Returns a span over `N` bytes from the buffer and moves the internal state
  // past those bytes, or returns nullopt and if there are not `N` bytes
  // remaining in the buffer.
  template <size_t N>
  std::optional<span<const uint8_t, N>> ReadFixedSpan() {
    if (remaining() < N) {
      return std::nullopt;
    }
    auto [consume, remain] = buffer_.split_at<N>();
    buffer_ = remain;
    return {consume};
  }

  bool ReadU8(uint8_t* value);
  bool ReadU16(uint16_t* value);
  bool ReadU32(uint32_t* value);
  bool ReadU64(uint64_t* value);

  // Reads a length-prefixed region:
  // 1. reads a big-endian length L from the buffer;
  // 2. sets |*out| to a StringPiece over the next L many bytes
  // of the buffer (beyond the end of the bytes encoding the length); and
  // 3. skips the main reader past this L-byte substring.
  //
  // Fails if reading a U8 or U16 fails, or if the parsed length is greater
  // than the number of bytes remaining in the stream.
  //
  // On failure, leaves the stream at the same position
  // as before the call.
  bool ReadU8LengthPrefixed(base::StringPiece* out);
  bool ReadU16LengthPrefixed(base::StringPiece* out);

 private:
  // TODO(danakj): Switch to raw_span in its own CL.
  span<const uint8_t> buffer_;
};

// Allows writing integers in network order (big endian) while iterating over
// an underlying buffer. All the writing functions advance the internal pointer.
class BASE_EXPORT BigEndianWriter {
 public:
  // Constructs a BigEndianWriter that will write into the given buffer.
  BigEndianWriter(span<uint8_t> buffer);

  // TODO(crbug.com/40284755): Remove this overload.
  UNSAFE_BUFFER_USAGE BigEndianWriter(char* buf, size_t len);

  ~BigEndianWriter();

  char* ptr() const { return reinterpret_cast<char*>(buffer_.data()); }
  size_t remaining() const { return buffer_.size(); }

  // Returns a span over all unwritten bytes.
  span<uint8_t> remaining_bytes() const { return buffer_; }

  bool Skip(size_t len);
  // TODO(crbug.com/40284755): WriteBytes() calls should be replaced with
  // WriteSpan().
  bool WriteBytes(const void* buf, size_t len);
  bool WriteU8(uint8_t value);
  bool WriteU16(uint16_t value);
  bool WriteU32(uint32_t value);
  bool WriteU64(uint64_t value);

  // Writes the span of bytes to the backing buffer. If there is not enough
  // room, it writes nothing and returns false.
  bool WriteSpan(base::span<const uint8_t> bytes);

  // Writes `N` bytes to the backing buffer. If there is not enough room, it
  // writes nothing and returns false.
  template <size_t N>
  bool WriteFixedSpan(base::span<const uint8_t, N> bytes) {
    if (remaining() < N) {
      return false;
    }
    auto [write, remain] = buffer_.split_at<N>();
    write.copy_from(bytes);
    buffer_ = remain;
    return true;
  }

 private:
  raw_span<uint8_t, DanglingUntriaged> buffer_;
};

}  // namespace base

#endif  // BASE_BIG_ENDIAN_H_
