// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_BCP47_EXTENSIONS_H_
#define BASE_I18N_BCP47_EXTENSIONS_H_

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"

namespace base::i18n {

class BASE_I18N_EXPORT LanguageTag;

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
  Extension(base::PassKey<LanguageTag>, std::string_view extension);
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
  PrivateUseSubtags(base::PassKey<LanguageTag>, std::string_view private_use);
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
// containing keywords/types and attributes. Please see the specification for
// more details: https://www.rfc-editor.org/info/rfc6067
class BASE_I18N_EXPORT UnicodeExtension {
 public:
  UnicodeExtension(const UnicodeExtension&);
  UnicodeExtension& operator=(const UnicodeExtension&);
  UnicodeExtension(UnicodeExtension&&);
  UnicodeExtension& operator=(UnicodeExtension&&);
  ~UnicodeExtension();

  // Method that parses the extension string into `UnicodeExtension`. The
  // constructor itself is made private.
  //
  // If a keyword appears more than once in the input, only its first definition
  // is considered; subsequent occurrences are ignored as per RFC 6067.
  static std::optional<UnicodeExtension> FromString(std::string_view extension);

  // Returns the value (collection of zero or more types) for the given key, if
  // present. Keys are exactly 2 characters. Types are from 3-8 characters.
  // Reference:  https://www.rfc-editor.org/info/rfc6067/#section-2.1
  std::optional<std::string_view> GetKeywordValue(std::string_view key) const;

  // Returns all the keywords sorted by key and parsed into pairs of key subtag
  // string plus types subtag string, if present.
  base::span<const std::pair<std::string, std::string>> keywords() const {
    return base::span(keywords_);
  }

  // Removes the keyword if present.
  void remove_keyword(std::string_view key) {
    keywords_.erase(base::ToLowerASCII(key));
  }

  // Sets or updates the value for the given keyword.
  // `key` must be exactly 2 alphanumeric characters.
  // `value` must be a dash-separated string of types (3-8 alphanumeric chars).
  // Returns true if the keyword was updated, false otherwise.
  bool SetKeyword(std::string_view key, std::string_view type_subtags);

  // Returns true if the keyword is present.
  bool has_keyword(std::string_view key) const {
    return keywords_.contains(base::ToLowerASCII(key));
  }

  // Attributes come before any keyword/value and have length between 3 and 8.
  bool has_attribute(std::string_view attribute) const {
    return attributes_.contains(base::ToLowerASCII(attribute));
  }

  // Removes the attribute if present.
  void remove_attribute(std::string_view attribute) {
    attributes_.erase(base::ToLowerASCII(attribute));
  }

  // Adds the attribute if not present.
  // `attribute` must be between 3 and 8 alphanumeric characters.
  // Returns true if the attribute was added (or already present), false if
  // invalid.
  bool AddAttribute(std::string_view attribute);

  // Returns all attributes as a single, dash-separated string.
  // The attributes are sorted alphabetically.
  base::span<const std::string> attributes() const {
    return base::span(attributes_);
  }

  // Returns all keyword keys.
  // The keys are sorted alphabetically.
  std::vector<std::string> GetKeywordKeys() const;

  // Returns all attributes and keywords as a single, dash-separated string.
  // This does NOT include the singleton ('u') and the leading separator.
  // The output is canonical: attributes (sorted alphabetically) precede
  // keywords (sorted by key alphabetically).
  std::string ToString() const;

 private:
  // Must be constructed from `FromString`, thus the private constructor.
  UnicodeExtension();

  base::flat_set<std::string, std::less<>> attributes_;
  // The unicode extension keywords map.
  base::flat_map<std::string, std::string, std::less<>> keywords_;
};

namespace bcp47_extensions {

// A traits used to map an extension key (e.g., 'u') to its corresponding
// result type (e.g., UnicodeExtension).
template <char extid>
struct Traits {
  using type = Extension;
  static constexpr auto Factory = [](base::PassKey<LanguageTag> pass_key,
                                     std::string_view private_use) {
    return Extension(pass_key, private_use);
  };
  static constexpr char key = extid;
};

// Specialization for the Unicode extension ('u').
template <>
struct Traits<'u'> {
  using type = UnicodeExtension;
  static constexpr auto Factory = [](base::PassKey<LanguageTag>,
                                     std::string_view extension) {
    return UnicodeExtension::FromString(extension);
  };
  static constexpr char key = 'u';
};

// Specialization for private use subtags ('x').
template <>
struct Traits<'x'> {
  using type = PrivateUseSubtags;
  static constexpr auto Factory = [](base::PassKey<LanguageTag> pass_key,
                                     std::string_view private_use) {
    return PrivateUseSubtags(pass_key, private_use);
  };
  static constexpr char key = 'x';
};

}  // namespace bcp47_extensions
}  // namespace base::i18n

namespace base::i18n_internal {

template <typename T>
struct IsTraits : std::false_type {};

template <char extid>
struct IsTraits<base::i18n::bcp47_extensions::Traits<extid>> : std::true_type {
};

}  // namespace base::i18n_internal

namespace base::i18n::bcp47_extensions {

// Concept to ensure T is an instance of Traits<extid>.
template <typename T>
concept ExtensionTrait = i18n_internal::IsTraits<std::remove_cvref_t<T>>::value;

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

}  // namespace base::i18n::bcp47_extensions

#endif  // BASE_I18N_BCP47_EXTENSIONS_H_
