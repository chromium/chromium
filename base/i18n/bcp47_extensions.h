// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_BCP47_EXTENSIONS_H_
#define BASE_I18N_BCP47_EXTENSIONS_H_

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/i18n/base_i18n_export.h"
#include "base/types/pass_key.h"

namespace base {

class BASE_I18N_EXPORT LanguageTag;

namespace i18n_extensions {

// Represents a BCP47 extension subtag.
// BCP47 extensions consist of a single-character singleton followed by one or
// more subtags. For example, in "en-US-a-myext", "a-myext" is an extension
// where 'a' is the singleton and "myext" is the subtag string.
// https://www.rfc-editor.org/info/rfc5646/#section-2.2.6
class BASE_I18N_EXPORT Extension {
 public:
  // These objects are managed by LanguageTag and cannot be constructed
  // manually.
  // |extension| must be a valid BCP47 extension string (e.g., "a-myext").
  Extension(base::PassKey<base::LanguageTag>, std::string_view extension);
  ~Extension() = default;

  Extension(const Extension&) = default;
  Extension& operator=(const Extension&) = default;
  Extension(Extension&&) = default;
  Extension& operator=(Extension&&) = default;

  // Returns the single-character identifier (singleton) of this extension.
  // For example, returns 'a' for the extension "a-myext".
  char singleton() const { return extension_[0]; }

  // Returns the subtags associated with this extension as a single string.
  // This does NOT include the singleton and the leading separator.
  // For example, returns "myext" for "a-myext".
  std::string_view subtags_string() const {
    return std::string_view(extension_).substr(2);
  }

 private:
  std::string extension_;
};

// Represents the 'x-' as-in BCP47 "private use subtags"
// (https://www.rfc-editor.org/info/rfc5646/#section-2.2.7).
class BASE_I18N_EXPORT PrivateUseSubtags {
 public:
  PrivateUseSubtags(base::PassKey<base::LanguageTag>,
                    std::string_view private_use);
  ~PrivateUseSubtags() = default;

  PrivateUseSubtags(const PrivateUseSubtags&) = default;
  PrivateUseSubtags& operator=(const PrivateUseSubtags&) = default;
  PrivateUseSubtags(PrivateUseSubtags&&) = default;
  PrivateUseSubtags& operator=(PrivateUseSubtags&&) = default;

  // Returns just the private use subtags, i.e. skips the 'x-' prefix.
  std::string_view subtags_string() const { return subtags_; }

 private:
  std::string subtags_;
};

// Represents a Unicode BCP47 extension ('u-').
// Unicode extensions have a specific internal structure defined by UTS #35,
// containing keywords and attributes.
class BASE_I18N_EXPORT UnicodeExtension : public Extension {
 public:
  // These objects are managed by LanguageTag and cannot be constructed
  // manually.
  // |extension| must be a valid Unicode extension string (e.g.,
  // "u-ca-gregory").
  UnicodeExtension(base::PassKey<base::LanguageTag>,
                   std::string_view extension);
  ~UnicodeExtension() = default;

  UnicodeExtension(const UnicodeExtension&) = default;
  UnicodeExtension& operator=(const UnicodeExtension&) = default;
  UnicodeExtension(UnicodeExtension&&) = default;
  UnicodeExtension& operator=(UnicodeExtension&&) = default;
};

// A traits used to map an extension key (e.g., 'u') to its corresponding
// result type (e.g., UnicodeExtension).
template <char extid>
struct BASE_I18N_EXPORT Traits {
  using type = Extension;
  static constexpr char key = extid;
};

// Specialization for the Unicode extension ('u').
template <>
struct BASE_I18N_EXPORT Traits<'u'> {
  using type = UnicodeExtension;
  static constexpr char key = 'u';
};

// Specialization for private use subtags ('x').
template <>
struct BASE_I18N_EXPORT Traits<'x'> {
  using type = PrivateUseSubtags;
  static constexpr char key = 'x';
};

namespace internal {

template <typename T>
struct IsTraits : std::false_type {};

template <char extid>
struct IsTraits<Traits<extid>> : std::true_type {};

}  // namespace internal

// Concept to ensure T is an instance of Traits<extid>.
template <typename T>
concept ExtensionTrait = internal::IsTraits<std::remove_cvref_t<T>>::value;

// Helper functions to create traits for GetExtension().

// Compile-time extension retrieval.
// Returns specialized type for known extensions (e.g. UnicodeExtension for
// 'u').
template <char c>
consteval Traits<c> ext() {
  static_assert((c >= 'a' && c <= 'z'), "Invalid BCP47 extension identifier");
  return {};
}

// Convenience helper for the Unicode extension ('u').
inline constexpr auto unicode() {
  return ext<'u'>();
}

// Convenience helper for private use subtags ('x').
inline constexpr auto priv() {
  return ext<'x'>();
}

}  // namespace i18n_extensions
}  // namespace base

#endif  // BASE_I18N_BCP47_EXTENSIONS_H_
