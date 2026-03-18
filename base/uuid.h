// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UUID_H_
#define BASE_UUID_H_

#include <stdint.h>

#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "build/robolectric_buildflags.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_ROBOLECTRIC)
#include "base/android/jni_string.h"
#include "base/uuid_jni/UUID_jni.h"
#include "third_party/jni_zero/jni_zero.h"
#endif

namespace base {

class BASE_EXPORT Uuid {
 public:
  // Length in bytes of the input required to format the input as a Uuid in the
  // form of version 4.
  static constexpr size_t kGuidV4InputLength = 16;

  // Generate a 128-bit random Uuid in the form of version 4. see RFC 4122,
  // section 4.4. The format of Uuid version 4 must be
  // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx, where y is one of [8, 9, a, b]. The
  // hexadecimal values "a" through "f" are output as lower case characters.
  // A cryptographically secure random source will be used, but consider using
  // UnguessableToken for greater type-safety if Uuid format is unnecessary.
  static Uuid GenerateRandomV4();

  // Returns a valid Uuid if the input string conforms to the Uuid format, and
  // an invalid Uuid otherwise. Note that this does NOT check if the hexadecimal
  // values "a" through "f" are in lower case characters.
  static Uuid ParseCaseInsensitive(std::string_view input);
  static Uuid ParseCaseInsensitive(std::u16string_view input);

  // Similar to ParseCaseInsensitive(), but all hexadecimal values "a" through
  // "f" must be lower case characters.
  static Uuid ParseLowercase(std::string_view input);
  static Uuid ParseLowercase(std::u16string_view input);

  // Constructs an invalid Uuid.
  Uuid();

  Uuid(const Uuid& other);
  Uuid& operator=(const Uuid& other);
  Uuid(Uuid&& other);
  Uuid& operator=(Uuid&& other);

  bool is_valid() const { return !lowercase_.empty(); }

  // Returns the Uuid in a lowercase string format if it is valid, and an empty
  // string otherwise. The returned value is guaranteed to be parsed by
  // ParseLowercase().
  //
  // NOTE: While AsLowercaseString() is currently a trivial getter, callers
  // should not treat it as such. When the internal type of base::Uuid changes,
  // this will be a non-trivial converter. See the TODO above `lowercase_` for
  // more context.
  const std::string& AsLowercaseString() const LIFETIME_BOUND;

  // Returns the Uuid as a 128-bit integer, or 0 if the Uuid is invalid.
  // Note: The memory layout is platform-dependent. On little-endian systems, it
  // matches neither the RFC 4122 byte sequence nor the Microsoft GUID layout.
  // Do not interpret or store the returned integer as a byte array.
  absl::uint128 AsInteger() const;

  // Invalid Uuids are equal.
  friend bool operator==(const Uuid&, const Uuid&) = default;
  // Uuids are 128bit chunks of data so must be indistinguishable if equivalent.
  friend std::strong_ordering operator<=>(const Uuid&, const Uuid&) = default;

 private:
  static Uuid FormatRandomDataAsV4Impl(
      base::span<const uint8_t, kGuidV4InputLength> input);

  // TODO(crbug.com/40108138): Consider using a different internal type.
  // Most existing representations of Uuids in the codebase use std::string,
  // so matching the internal type will avoid inefficient string conversions
  // during the migration to base::Uuid.
  //
  // The lowercase form of the Uuid. Empty for invalid Uuids.
  std::string lowercase_;
};

// For runtime usage only. Do not store the result of this hash, as it may
// change in future Chromium revisions.
struct BASE_EXPORT UuidHash {
  size_t operator()(const Uuid& uuid) const;
};

// Stream operator so Uuid objects can be used in logging statements.
BASE_EXPORT std::ostream& operator<<(std::ostream& out, const Uuid& uuid);

}  // namespace base

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_ROBOLECTRIC)

namespace jni_zero {

template <>
inline base::Uuid FromJniType<base::Uuid>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj) {
  if (!obj) {
    return base::Uuid();
  }
  return base::Uuid::ParseLowercase(
      FromJniType<std::string>(env, JNI_UUID::Java_UUID_toString(env, obj)));
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<base::Uuid>(
    JNIEnv* env,
    const base::Uuid& uuid) {
  if (!uuid.is_valid()) {
    return nullptr;
  }
  absl::uint128 value = uuid.AsInteger();
  return JNI_UUID::Java_UUID_Constructor(
      env, static_cast<jlong>(absl::Uint128High64(value)),
      static_cast<jlong>(absl::Uint128Low64(value)));
}

}  // namespace jni_zero
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_ROBOLECTRIC)

#endif  // BASE_UUID_H_
