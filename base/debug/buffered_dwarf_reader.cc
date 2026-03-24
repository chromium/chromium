// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/buffered_dwarf_reader.h"

#include "base/compiler_specific.h"

#ifdef USE_SYMBOLIZE

#include <algorithm>
#include <span>

#include "base/numerics/safe_conversions.h"
#include "base/third_party/symbolize/symbolize.h"

namespace base::debug {

BufferedDwarfReaderBase::BufferedDwarfReaderBase(int fd,
                                                 uint64_t position,
                                                 std::span<char> buf)
    : file_(fd, buf.data(), buf.size()), position_(position) {}

size_t BufferedDwarfReaderBase::ReadCString(uint64_t max_position,
                                            char* out,
                                            size_t out_size) {
  char character;
  size_t bytes_written = 0;
  do {
    if (!ReadChar(character)) {
      return 0;
    }

    if (out && bytes_written < out_size) {
      UNSAFE_TODO(out[bytes_written++]) = character;
    }
  } while (character != '\0' && position_ < max_position);

  if (out) {
    UNSAFE_TODO(out[std::min(bytes_written, out_size - 1)]) = '\0';
  }

  return bytes_written;
}

bool BufferedDwarfReaderBase::ReadLeb128(uint64_t& value) {
  value = 0;
  uint8_t byte;
  int shift = 0;
  do {
    if (!ReadInt8(byte)) {
      return false;
    }
    value |= static_cast<uint64_t>(byte & 0x7F) << shift;
    shift += 7;
  } while (byte & 0x80);
  return true;
}

bool BufferedDwarfReaderBase::ReadLeb128(int64_t& value) {
  value = 0;
  uint8_t byte;
  int shift = 0;
  bool sign_bit = false;
  do {
    if (!ReadInt8(byte)) {
      return false;
    }
    value |= static_cast<uint64_t>(byte & 0x7F) << shift;
    shift += 7;
    sign_bit = byte & 0x40;
  } while (byte & 0x80);
  constexpr int bits_in_output = sizeof(value) * 8;
  if ((shift < bits_in_output) && sign_bit) {
    value |= -(1 << shift);
  }
  return true;
}

bool BufferedDwarfReaderBase::ReadInitialLength(bool& is_64bit,
                                                uint64_t& length) {
  uint32_t token_32bit;

  if (!ReadInt32(token_32bit)) {
    return false;
  }

  // Dwarf 3 introduced an extended length field that both indicates this is
  // DWARF-64 and changes how the size is encoded. 0xfffffff0 and higher are
  // reserved with 0xffffffff meaning it's the extended field with the
  // following 64-bits being the full length.
  if (token_32bit < 0xfffffff0) {
    length = token_32bit;
    is_64bit = false;
    return true;
  }

  if (token_32bit != 0xffffffff) {
    return false;
  }

  if (!ReadInt64(length)) {
    return false;
  }

  is_64bit = true;
  return true;
}

bool BufferedDwarfReaderBase::ReadOffset(bool is_64bit, uint64_t& offset) {
  if (is_64bit) {
    if (!ReadInt64(offset)) {
      return false;
    }
  } else {
    uint32_t tmp;
    if (!ReadInt32(tmp)) {
      return false;
    }
    offset = tmp;
  }
  return true;
}

bool BufferedDwarfReaderBase::ReadAddress(uint8_t address_size,
                                          uint64_t& address) {
  // Note `address_size` indicates the numbrer of bytes in the address.
  switch (address_size) {
    case 2: {
      uint16_t tmp;
      if (!ReadInt16(tmp)) {
        return false;
      }
      address = tmp;
    } break;

    case 4: {
      uint32_t tmp;
      if (!ReadInt32(tmp)) {
        return false;
      }
      address = tmp;
    } break;

    case 8: {
      uint64_t tmp;
      if (!ReadInt64(tmp)) {
        return false;
      }
      address = tmp;
    } break;

    default:
      return false;
  }
  return true;
}

bool BufferedDwarfReaderBase::ReadCommonHeader(bool& is_64bit,
                                               uint64_t& length,
                                               uint16_t& version,
                                               uint64_t& offset,
                                               uint8_t& address_size,
                                               uint64_t& end_position) {
  if (!ReadInitialLength(is_64bit, length)) {
    return false;
  }
  end_position = position_ + length;

  if (!ReadInt16(version)) {
    return false;
  }

  if (!ReadOffset(is_64bit, offset)) {
    return false;
  }

  if (!ReadInt8(address_size)) {
    return false;
  }

  return true;
}

bool BufferedDwarfReaderBase::BufferedRead(void* out, const size_t bytes) {
  if (!base::IsValueInRangeForNumericType<size_t>(position_)) {
    return false;
  }
  if (!file_.ReadFromOffsetExact(out, bytes, position_)) {
    return false;
  }
  position_ += bytes;
  return true;
}

}  // namespace base::debug

#endif
