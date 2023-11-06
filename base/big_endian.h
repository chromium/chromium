// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIG_ENDIAN_H_
#define BASE_BIG_ENDIAN_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
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
template <typename T>
inline void ReadBigEndian(const uint8_t buf[], T* out) {
  static_assert(std::is_integral_v<T>, "T has to be an integral type.");
  // Make an unsigned version of the output type to make shift possible
  // without UB.
  typename std::make_unsigned<T>::type raw;
  memcpy(&raw, buf, sizeof(T));
  *out = static_cast<T>(internal::ByteSwapIfLittleEndian(raw));
}

// Write an integer (signed or unsigned) |val| to |buf| in Big Endian order.
// Note: this loop is unrolled with -O1 and above.
template<typename T>
inline void WriteBigEndian(char buf[], T val) {
  static_assert(std::is_integral_v<T>, "T has to be an integral type.");
  const auto unsigned_val =
      static_cast<typename std::make_unsigned<T>::type>(val);
  const auto raw = internal::ByteSwapIfLittleEndian(unsigned_val);
  memcpy(buf, &raw, sizeof(T));
}

// Allows reading integers in network order (big endian) while iterating over
// an underlying buffer. All the reading functions advance the internal pointer.
class BASE_EXPORT BigEndianReader {
 public:
  static BigEndianReader FromStringPiece(base::StringPiece string_piece);

  BigEndianReader(const uint8_t* buf, size_t len);
  explicit BigEndianReader(base::span<const uint8_t> buf);

  const uint8_t* ptr() const { return ptr_; }
  size_t remaining() const { return static_cast<size_t>(end_ - ptr_); }

  bool Skip(size_t len);
  bool ReadBytes(void* out, size_t len);
  // Creates a StringPiece in |out| that points to the underlying buffer.
  bool ReadPiece(base::StringPiece* out, size_t len);
  bool ReadSpan(base::span<const uint8_t>* out, size_t len);

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
  // Hidden to promote type safety.
  template<typename T>
  bool Read(T* v);
  template <typename T>
  bool ReadLengthPrefixed(base::StringPiece* out);

  const uint8_t* ptr_;
  const uint8_t* end_;
};

// Allows writing integers in network order (big endian) while iterating over
// an underlying buffer. All the writing functions advance the internal pointer.
class BASE_EXPORT BigEndianWriter {
 public:
  BigEndianWriter(char* buf, size_t len);

  char* ptr() const { return ptr_; }
  size_t remaining() const { return static_cast<size_t>(end_ - ptr_); }

  bool Skip(size_t len);
  bool WriteBytes(const void* buf, size_t len);
  bool WriteU8(uint8_t value);
  bool WriteU16(uint16_t value);
  bool WriteU32(uint32_t value);
  bool WriteU64(uint64_t value);

 private:
  // Hidden to promote type safety.
  template<typename T>
  bool Write(T v);

  raw_ptr<char, DanglingUntriaged | AllowPtrArithmetic> ptr_;
  raw_ptr<char, DanglingUntriaged | AllowPtrArithmetic> end_;
};

}  // namespace base

#endif  // BASE_BIG_ENDIAN_H_
