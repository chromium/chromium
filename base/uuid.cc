// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/uuid.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string_view>

#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/pass_key.h"

namespace base {

namespace {

template <typename Char>
constexpr bool IsLowerHexDigit(Char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

constexpr bool IsHyphenPosition(size_t i) {
  return i == 8 || i == 13 || i == 18 || i == 23;
}

// Returns a canonical Uuid string given that `input` is validly formatted
// xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx, such that x is a hexadecimal digit.
// If `strict`, x must be a lower-case hexadecimal digit.
template <typename StringPieceType>
std::string GetCanonicalUuidInternal(StringPieceType input, bool strict) {
  using CharType = typename StringPieceType::value_type;

  constexpr size_t kUuidLength = 36;
  if (input.length() != kUuidLength) {
    return std::string();
  }

  std::string lowercase_;
  lowercase_.resize(kUuidLength);
  for (size_t i = 0; i < input.length(); ++i) {
    CharType current = input[i];
    if (IsHyphenPosition(i)) {
      if (current != '-') {
        return std::string();
      }
      lowercase_[i] = '-';
    } else {
      if (strict ? !IsLowerHexDigit(current) : !IsHexDigit(current)) {
        return std::string();
      }
      lowercase_[i] = static_cast<char>(ToLowerASCII(current));
    }
  }

  return lowercase_;
}

}  // namespace

// static
Uuid Uuid::GenerateRandomV4() {
  uint8_t sixteen_bytes[kGuidV4InputLength];
  // Use base::RandBytes instead of crypto::RandBytes, because crypto calls the
  // base version directly, and to prevent the dependency from base/ to crypto/.
  RandBytes(sixteen_bytes);
  return FormatRandomDataAsV4Impl(sixteen_bytes);
}

// static
Uuid Uuid::FormatRandomDataAsV4(
    base::span<const uint8_t, 16> input,
    base::PassKey<content::FileSystemAccessManagerImpl> /*pass_key*/) {
  return FormatRandomDataAsV4Impl(input);
}

// static
Uuid Uuid::FormatRandomDataAsV4ForTesting(base::span<const uint8_t, 16> input) {
  return FormatRandomDataAsV4Impl(input);
}

// static
Uuid Uuid::FormatRandomDataAsV4Impl(base::span<const uint8_t, 16> input) {
  DCHECK_EQ(input.size_bytes(), kGuidV4InputLength);

  uint64_t sixteen_bytes[2];
  memcpy(&sixteen_bytes, input.data(), sizeof(sixteen_bytes));

  // Set the Uuid to version 4 as described in RFC 4122, section 4.4.
  // The format of Uuid version 4 must be xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx,
  // where y is one of [8, 9, a, b].

  // Clear the version bits and set the version to 4:
  sixteen_bytes[0] &= 0xffffffff'ffff0fffULL;
  sixteen_bytes[0] |= 0x00000000'00004000ULL;

  // Set the two most significant bits (bits 6 and 7) of the
  // clock_seq_hi_and_reserved to zero and one, respectively:
  sixteen_bytes[1] &= 0x3fffffff'ffffffffULL;
  sixteen_bytes[1] |= 0x80000000'00000000ULL;

  Uuid uuid;
  uuid.lowercase_ =
      StringPrintf("%08x-%04x-%04x-%04x-%012llx",
                   static_cast<uint32_t>(sixteen_bytes[0] >> 32),
                   static_cast<uint32_t>((sixteen_bytes[0] >> 16) & 0x0000ffff),
                   static_cast<uint32_t>(sixteen_bytes[0] & 0x0000ffff),
                   static_cast<uint32_t>(sixteen_bytes[1] >> 48),
                   sixteen_bytes[1] & 0x0000ffff'ffffffffULL);
  return uuid;
}

// static
Uuid Uuid::ParseCaseInsensitive(std::string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/false);
  return uuid;
}

// static
Uuid Uuid::ParseCaseInsensitive(std::u16string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/false);
  return uuid;
}

// static
Uuid Uuid::ParseLowercase(std::string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/true);
  return uuid;
}

// static
Uuid Uuid::ParseLowercase(std::u16string_view input) {
  Uuid uuid;
  uuid.lowercase_ = GetCanonicalUuidInternal(input, /*strict=*/true);
  return uuid;
}

Uuid::Uuid() = default;

Uuid::Uuid(const Uuid& other) = default;

Uuid& Uuid::operator=(const Uuid& other) = default;

Uuid::Uuid(Uuid&& other) = default;

Uuid& Uuid::operator=(Uuid&& other) = default;

const std::string& Uuid::AsLowercaseString() const {
  return lowercase_;
}

std::ostream& operator<<(std::ostream& out, const Uuid& uuid) {
  return out << uuid.AsLowercaseString();
}

size_t UuidHash::operator()(const Uuid& uuid) const {
  // TODO(crbug.com/40108138): Avoid converting to string to take the hash when
  // the internal type is migrated to a non-string type.
  return FastHash(uuid.AsLowercaseString());
}

}  // namespace base
