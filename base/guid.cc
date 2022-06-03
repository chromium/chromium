// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>

#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace base {

namespace {

template <typename Char>
constexpr bool IsLowerHexDigit(Char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

constexpr bool IsHyphenPosition(size_t i) {
  return i == 8 || i == 13 || i == 18 || i == 23;
}

// Returns a canonical GUID string given that `input` is validly formatted
// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, such that x is a hexadecimal digit.
// If `strict`, x must be a lower-case hexadecimal digit.
template <typename StringPieceType>
std::string GetCanonicalGUIDInternal(StringPieceType input, bool strict) {
  using CharType = typename StringPieceType::value_type;

  constexpr size_t kGUIDLength = 36;
  if (input.length() != kGUIDLength)
    return std::string();

  std::string lowercase_;
  lowercase_.resize(kGUIDLength);
  for (size_t i = 0; i < input.length(); ++i) {
    CharType current = input[i];
    if (IsHyphenPosition(i)) {
      if (current != '-')
        return std::string();
      lowercase_[i] = '-';
    } else {
      if (strict ? !IsLowerHexDigit(current) : !IsHexDigit(current))
        return std::string();
      lowercase_[i] = ToLowerASCII(current);
    }
  }

  return lowercase_;
}

}  // namespace

std::string GenerateGUID() {
  GUID guid = GUID::GenerateRandomV4();
  return guid.AsLowercaseString();
}

bool IsValidGUID(StringPiece input) {
  return !GetCanonicalGUIDInternal(input, /*strict=*/false).empty();
}

bool IsValidGUID(StringPiece16 input) {
  return !GetCanonicalGUIDInternal(input, /*strict=*/false).empty();
}

bool IsValidGUIDOutputString(StringPiece input) {
  return !GetCanonicalGUIDInternal(input, /*strict=*/true).empty();
}

std::string RandomDataToGUIDString(const uint64_t bytes[2]) {
  return StringPrintf(
      "%08x-%04x-%04x-%04x-%012llx", static_cast<uint32_t>(bytes[0] >> 32),
      static_cast<uint32_t>((bytes[0] >> 16) & 0x0000ffff),
      static_cast<uint32_t>(bytes[0] & 0x0000ffff),
      static_cast<uint32_t>(bytes[1] >> 48), bytes[1] & 0x0000ffff'ffffffffULL);
}

// static
GUID GUID::GenerateRandomV4() {
  uint64_t sixteen_bytes[2];
  // Use base::RandBytes instead of crypto::RandBytes, because crypto calls the
  // base version directly, and to prevent the dependency from base/ to crypto/.
  RandBytes(&sixteen_bytes, sizeof(sixteen_bytes));

  // Set the GUID to version 4 as described in RFC 4122, section 4.4.
  // The format of GUID version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, a, b].

  // Clear the version bits and set the version to 4:
  sixteen_bytes[0] &= 0xffffffff'ffff0fffULL;
  sixteen_bytes[0] |= 0x00000000'00004000ULL;

  // Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively:
  sixteen_bytes[1] &= 0x3fffffff'ffffffffULL;
  sixteen_bytes[1] |= 0x80000000'00000000ULL;

  GUID guid;
  guid.lowercase_ = RandomDataToGUIDString(sixteen_bytes);
  return guid;
}

// static
GUID GUID::ParseCaseInsensitive(StringPiece input) {
  GUID guid;
  guid.lowercase_ = GetCanonicalGUIDInternal(input, /*strict=*/false);
  return guid;
}

// static
GUID GUID::ParseCaseInsensitive(StringPiece16 input) {
  GUID guid;
  guid.lowercase_ = GetCanonicalGUIDInternal(input, /*strict=*/false);
  return guid;
}

// static
GUID GUID::ParseLowercase(StringPiece input) {
  GUID guid;
  guid.lowercase_ = GetCanonicalGUIDInternal(input, /*strict=*/true);
  return guid;
}

// static
GUID GUID::ParseLowercase(StringPiece16 input) {
  GUID guid;
  guid.lowercase_ = GetCanonicalGUIDInternal(input, /*strict=*/true);
  return guid;
}

GUID::GUID() = default;

GUID::GUID(const GUID& other) = default;

GUID& GUID::operator=(const GUID& other) = default;

const std::string& GUID::AsLowercaseString() const {
  return lowercase_;
}

bool GUID::operator==(const GUID& other) const {
  return AsLowercaseString() == other.AsLowercaseString();
}

bool GUID::operator!=(const GUID& other) const {
  return !(*this == other);
}

bool GUID::operator<(const GUID& other) const {
  return AsLowercaseString() < other.AsLowercaseString();
}

bool GUID::operator<=(const GUID& other) const {
  return *this < other || *this == other;
}

bool GUID::operator>(const GUID& other) const {
  return !(*this <= other);
}

bool GUID::operator>=(const GUID& other) const {
  return !(*this < other);
}

std::ostream& operator<<(std::ostream& out, const GUID& guid) {
  return out << guid.AsLowercaseString();
}

}  // namespace base
