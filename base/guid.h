// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GUID_H_
#define BASE_GUID_H_

#include <stdint.h>

#include <iosfwd>
#include <string>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/strings/string_piece.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"

namespace content {
class FileSystemAccessManagerImpl;
}

namespace base {

// DEPRECATED, use GUID::GenerateRandomV4() instead.
BASE_EXPORT std::string GenerateGUID();

// DEPRECATED, use GUID::ParseCaseInsensitive() and GUID::is_valid() instead.
BASE_EXPORT bool IsValidGUID(StringPiece input);
BASE_EXPORT bool IsValidGUID(StringPiece16 input);

// DEPRECATED, use GUID::ParseLowercase() and GUID::is_valid() instead.
BASE_EXPORT bool IsValidGUIDOutputString(StringPiece input);

// For unit testing purposes only.  Do not use outside of tests.
BASE_EXPORT std::string RandomDataToGUIDString(const uint64_t bytes[2]);

class BASE_EXPORT GUID {
 public:
  // Length in bytes of the input required to format the input as a GUID in the
  // form of version 4.
  static constexpr size_t kGuidV4InputLength = 16;

  // Generate a 128-bit random GUID in the form of version 4. see RFC 4122,
  // section 4.4. The format of GUID version 4 must be
  // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx, where y is one of [8, 9, a, b]. The
  // hexadecimal values "a" through "f" are output as lower case characters.
  // A cryptographically secure random source will be used, but consider using
  // UnguessableToken for greater type-safety if GUID format is unnecessary.
  static GUID GenerateRandomV4();

  // Formats a sequence of 16 random bytes as a GUID in the form of version 4.
  // `input` must:
  // - have been randomly generated (e.g. created from an UnguessableToken), and
  // - be of length 16 (this is checked at compile-time).
  // Despite taking 128 bits of randomness, certain bits will always be
  // masked over to adhere to the V4 GUID format.
  // Useful in cases where an opaque identifier that is generated from stable
  // inputs needs to be formatted as a V4 GUID. Currently only exposed to the
  // File System Access API to return a V4 GUID for the getUniqueId() method.
  static GUID FormatRandomDataAsV4(
      base::span<const uint8_t, kGuidV4InputLength> input,
      base::PassKey<content::FileSystemAccessManagerImpl> pass_key);
  static GUID FormatRandomDataAsV4ForTesting(
      base::span<const uint8_t, kGuidV4InputLength> input);

  // Returns a valid GUID if the input string conforms to the GUID format, and
  // an invalid GUID otherwise. Note that this does NOT check if the hexadecimal
  // values "a" through "f" are in lower case characters.
  static GUID ParseCaseInsensitive(StringPiece input);
  static GUID ParseCaseInsensitive(StringPiece16 input);

  // Similar to ParseCaseInsensitive(), but all hexadecimal values "a" through
  // "f" must be lower case characters.
  static GUID ParseLowercase(StringPiece input);
  static GUID ParseLowercase(StringPiece16 input);

  // Constructs an invalid GUID.
  GUID();

  GUID(const GUID& other);
  GUID& operator=(const GUID& other);
  GUID(GUID&& other);
  GUID& operator=(GUID&& other);

  bool is_valid() const { return !lowercase_.empty(); }

  // Returns the GUID in a lowercase string format if it is valid, and an empty
  // string otherwise. The returned value is guaranteed to be parsed by
  // ParseLowercase().
  //
  // NOTE: While AsLowercaseString() is currently a trivial getter, callers
  // should not treat it as such. When the internal type of base::GUID changes,
  // this will be a non-trivial converter. See the TODO above `lowercase_` for
  // more context.
  const std::string& AsLowercaseString() const;

  // Invalid GUIDs are equal.
  bool operator==(const GUID& other) const;
  bool operator!=(const GUID& other) const;
  bool operator<(const GUID& other) const;
  bool operator<=(const GUID& other) const;
  bool operator>(const GUID& other) const;
  bool operator>=(const GUID& other) const;

 private:
  static GUID FormatRandomDataAsV4Impl(
      base::span<const uint8_t, kGuidV4InputLength> input);

  // TODO(crbug.com/1026195): Consider using a different internal type.
  // Most existing representations of GUIDs in the codebase use std::string,
  // so matching the internal type will avoid inefficient string conversions
  // during the migration to base::GUID.
  //
  // The lowercase form of the GUID. Empty for invalid GUIDs.
  std::string lowercase_;
};

// For runtime usage only. Do not store the result of this hash, as it may
// change in future Chromium revisions.
struct BASE_EXPORT GUIDHash {
  size_t operator()(const GUID& guid) const {
    // TODO(crbug.com/1026195): Avoid converting to string to take the hash when
    // the internal type is migrated to a non-string type.
    return FastHash(guid.AsLowercaseString());
  }
};

// Stream operator so GUID objects can be used in logging statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const GUID& guid);

}  // namespace base

#endif  // BASE_GUID_H_
