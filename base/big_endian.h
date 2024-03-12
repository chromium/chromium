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

  // Moves the internal state forward `len` bytes, or returns false if there is
  // not enough bytes left to read from.
  bool Skip(size_t len);

  // Reads an 8-bit integer and advances past it. Returns false if there is not
  // enough bytes to read from.
  bool ReadU8(uint8_t* value);
  // Reads a 16-bit integer and advances past it. Returns false if there is not
  // enough bytes to read from.
  bool ReadU16(uint16_t* value);
  // Reads a 32-bit integer and advances past it. Returns false if there is not
  // enough bytes to read from.
  bool ReadU32(uint32_t* value);
  // Reads a 64-bit integer and advances past it. Returns false if there is not
  // enough bytes to read from.
  bool ReadU64(uint64_t* value);

  // An alias for `ReadU8` that works with a `char` pointer instead of
  // `uint8_t`.
  bool ReadChar(char* value) {
    return ReadU8(reinterpret_cast<uint8_t*>(value));
  }

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
  std::optional<span<const uint8_t, N>> ReadSpan() {
    if (remaining() < N) {
      return std::nullopt;
    }
    auto [consume, remain] = buffer_.split_at<N>();
    buffer_ = remain;
    return {consume};
  }

  // Copies into a span (writing to the whole span) from the buffer and moves
  // the internal state past the copied bytes, or returns false and if there are
  // not enough bytes remaining in the buffer to fill the span and leaves the
  // internal state unchanged.
  bool ReadBytes(span<uint8_t> out);

  // Copies into a span of `N` bytes from the buffer and moves the internal
  // state past the copied bytes, or returns false and if there are not `N`
  // bytes remaining in the buffer and leaves the internal state unchanged.
  template <size_t N>
  bool ReadBytes(span<uint8_t, N> out) {
    std::optional<span<const uint8_t, N>> span = ReadSpan<N>();
    if (!span.has_value()) {
      return false;
    }
    out.copy_from(*span);
    return true;
  }

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
  raw_span<const uint8_t> buffer_;
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
  raw_span<uint8_t> buffer_;
};

}  // namespace base

#endif  // BASE_BIG_ENDIAN_H_
