// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_TAG_H_
#define BASE_I18N_LANGUAGE_TAG_H_

#include <compare>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/internal/bcp47_parser.h"
#include "base/i18n/internal/immutable_string.h"

namespace base::i18n {

class BASE_I18N_EXPORT LanguageTagConverter;

class LanguageTag;

constexpr std::optional<LanguageTag> ParseKnownLanguageTag(
    std::string_view tag);
consteval LanguageTag GetKnownLanguageTag(std::string_view tag);

namespace mojo {
template <typename DataView, typename T>
struct StructTraits;
}  // namespace mojo

namespace mojo_base::mojom {
class LanguageTagDataView;
}  // namespace mojo_base::mojom

// A type-safe wrapper for BCP47 language tags (locales).
//
// Supported Format Specification:
// - Core Standard: BCP47 language tags.
// - Structure: Supports subtags separated by hyphens ('-'):
//   - Language: Mandatory, >= 2 chars (e.g., "en", "zh") or 3 chars.
//   - Script: Optional, 4 chars (e.g., "Hant").
//   - Region: Optional, 2-3 chars (e.g., "US", "001").
//   - Variants: Optional (e.g., "oxendict").
//   - Extensions: Optional (e.g., "u-ca-gregory").
//   - Private use: Optional (e.g., "x-privatestuff")
class BASE_I18N_EXPORT LanguageTag {
 public:
  using ImmutableStringType = i18n_internal::ImmutableString;

  constexpr LanguageTag(const LanguageTag&) noexcept = default;
  constexpr LanguageTag(LanguageTag&& other) noexcept = default;
  constexpr LanguageTag& operator=(const LanguageTag&) noexcept = default;
  constexpr LanguageTag& operator=(LanguageTag&&) noexcept = default;

  constexpr ~LanguageTag() = default;

  constexpr friend bool operator==(const LanguageTag& lhs,
                                   const LanguageTag& rhs) {
    return lhs.tag_string() == rhs.tag_string();
  }
  constexpr friend std::strong_ordering operator<=>(const LanguageTag& lhs,
                                                    const LanguageTag& rhs) {
    return lhs.tag_string() <=> rhs.tag_string();
  }

  template <typename H>
  friend H AbslHashValue(H h, const LanguageTag& tag) {
    return H::combine(std::move(h), tag.tag_string());
  }

  // Returns the BCP47 language tag (e.g., "en-US", "zh-CN").
  constexpr std::string_view tag_string() const LIFETIME_BOUND {
    return tag_.AsString();
  }

  // Returns the language tag in legacy ICU format, replacing hyphens with
  // underscores (e.g., "en_US", "zh_CN").
  // Note: This does not work correctly when the tag has extensions.
  // TODO(crbug.com/517510055): Convert unicode extensions to the legacy format.
  std::string ToLegacyICUFormat() const;

  // Returns the language subtag in the language tag if present.
  // Examples:
  // - "en-US" -> "en"
  // - "zh-Hant-TW" -> "zh"
  // - "en" -> "en"
  // - "sr-Latn" -> "sr"
  // - "und-BR" -> "und"
  //
  // Notice that this does not necessarily represent the language itself as some
  // of them need their region, script and variant to be properly represented.
  constexpr std::string_view language_subtag() const LIFETIME_BOUND {
    return i18n_internal::ParseBcp47Tag(tag_string())
        .value_or(i18n_internal::ParsedBcp47Tag())
        .language;
  }
  // Creates a new `LanguageTag` containing only the language subtag.
  LanguageTag WithLanguageSubtagOnly() const;

  // Returns the region subtag in the language tag if present.
  // Examples:
  // - "en-US" -> "US"
  // - "zh-Hant-TW" -> "TW"
  // - "en" -> ""
  // - "sr-Latn" -> ""
  // Note that the region subtag is not always present, if it is not set, an
  // empty string is returned.
  constexpr std::string_view region_subtag() const LIFETIME_BOUND {
    return i18n_internal::ParseBcp47Tag(tag_string())
        .value_or(i18n_internal::ParsedBcp47Tag())
        .region;
  }

  // Returns the parent language tag of this language tag by stripping the most
  // specific subtag. The parent hierarchy traversal order is:
  //   1. Private-use subtags (e.g., "en-US-x-test" -> "en-US")
  //   2. Extensions (e.g., "en-US-u-ca-gregory" -> "en-US")
  //   3. Variants (e.g., "en-GB-oxendict" -> "en-GB")
  //   4. Region (e.g., "sr-Latn-RS" -> "sr-Latn")
  //   5. Script (e.g., "sr-Latn" -> "sr")
  //
  // If the language tag only consists of the base language subtag (e.g., "en"),
  // it has no parent and `std::nullopt` is returned.
  constexpr std::optional<LanguageTag> GetParentTag() const;

  // Retrieves the singleton and subtag(s) for an extension to a BCP47 language
  // tag.
  //
  // Use the helper functions in `bcp47_extensions` to specify which
  // extension or private use subtags to retrieve:
  // - `GetExtension(bcp47_extensions::unicode())` for "u-" extensions.
  // - `GetExtension(bcp47_extensions::priv())` for "x-" private use
  // subtags.
  // - `GetExtension(bcp47_extensions::ext('a'))` for any other
  // single-char extension.
  //
  // Example:
  //   auto locale =
  //   LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory");
  //   auto ext = locale->GetExtension(bcp47_extensions::unicode());
  //   if (ext) {
  //     CHECK_EQ(ext->SubtagsString(), "ca-gregory");
  //   }
  std::optional<UnicodeExtension> GetExtension(
      bcp47_extensions::Traits<'u'> traits) const;

  std::optional<PrivateUseSubtags> GetExtension(
      bcp47_extensions::Traits<'x'> traits) const;

  template <char extid>
    requires(extid != 'u' && extid != 'x')
  std::optional<Extension> GetExtension(
      bcp47_extensions::Traits<extid> traits) const {
    std::string_view extension = GetExtensionStringInternal(extid);
    if (extension.empty()) {
      return std::nullopt;
    }

    return traits.Factory(base::PassKey<LanguageTag>(), extension);
  }

  // Returns a new `LanguageTag` with the given `extension` set (language tags
  // are immutable). If an extension with the same singleton already exists, it
  // is replaced. If the extension is empty, the current `LanguageTag` is
  // returned unchanged.
  //
  // Example:
  //   std::optional<UnicodeExtension> u_ext =
  //     tag.GetExtension(bcp47_extensions::unicode());
  //   u_ext->SetKeyword("ca", "gregory");
  //   LanguageTag mutated = tag.WithExtension(*u_ext);
  LanguageTag WithExtension(const UnicodeExtension& extension) const;
  LanguageTag WithExtension(const PrivateUseSubtags& extension) const;
  LanguageTag WithExtension(const Extension& extension) const;

 private:
  friend class LanguageTagConverter;
  friend constexpr std::optional<LanguageTag> ParseKnownLanguageTag(
      std::string_view tag);
  friend consteval LanguageTag GetKnownLanguageTag(std::string_view tag);
  // Allow Mojo StructTraits to default-construct an instance during IPC
  // deserialization
  friend struct mojo::StructTraits<mojo_base::mojom::LanguageTagDataView,
                                   base::i18n::LanguageTag>;

  // Default constructor is intended for internal use by Mojo StructTraits to
  // allow for deserialization of the language tag from IPC.
  // `mojo::DefaultConstruct` cannot be used here because of layered
  // dependencies.
  LanguageTag();

  std::string_view GetExtensionStringInternal(char key) const;
  LanguageTag WithExtensionStringInternal(char key,
                                          std::string_view subtags) const;

  // This constructor is intended for internal use by `LanguageTagConverter`.
  // Do not call this directly.
  explicit LanguageTag(ImmutableStringType tag);
  // Constexpr Constructor that expects the span of string-views and constructs
  // tha ImmutableString on its own.
  constexpr explicit LanguageTag(base::span<const std::string_view> parts)
      : tag_(std::is_constant_evaluated()
                 ? i18n_internal::ImmutableString(
                       i18n_internal::ImmutableString::ForceStackString{},
                       parts)
                 : i18n_internal::ImmutableString(parts)) {}

  // The BCP47 language tag, e.g. "pt-BR".
  // Supports language, script, region, variants and extensions.
  ImmutableStringType tag_;
};

BASE_I18N_EXPORT std::ostream& operator<<(std::ostream& os,
                                          const LanguageTag& lt);

BASE_I18N_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const std::optional<LanguageTag>& opt);

// Parses a LanguageTag from a string_view.
// Returns std::nullopt if `tag` is not a valid BCP 47 language tag or has
// unknown subtags.
constexpr std::optional<LanguageTag> ParseKnownLanguageTag(
    std::string_view tag) {
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag);
  if (!parsed || !i18n_internal::AreSubtagsKnown(*parsed)) {
    return std::nullopt;
  }
  return LanguageTag(base::span<const std::string_view>({tag}));
}

// Returns a LanguageTag checked at compile time. does not compile if tag is
// not one of the predefined supported language tags.
consteval LanguageTag GetKnownLanguageTag(std::string_view tag) {
  // It is only possible to construct `LanguageTag`s at compile-time if they
  // are small.
  if (tag.size() > i18n_internal::ImmutableString::kSmallBufferSize) {
    void ERROR_TagIsTooLarge();
    ERROR_TagIsTooLarge();
  }

  std::optional<LanguageTag> language_tag = ParseKnownLanguageTag(tag);
  if (!language_tag) {
    void ERROR_TagIsUnknown();
    ERROR_TagIsUnknown();
  }

  return *language_tag;
}

constexpr std::optional<LanguageTag> LanguageTag::GetParentTag() const {
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag_string());
  if (!parsed) {
    return std::nullopt;
  }

  if (!parsed->private_use.empty()) {
    parsed->private_use.clear();
  } else if (!parsed->extensions.empty()) {
    parsed->extensions.clear();
  } else if (!parsed->variants.empty()) {
    parsed->variants.pop_back();
  } else if (!parsed->region.empty()) {
    parsed->region = std::string_view();
  } else if (!parsed->script.empty()) {
    parsed->script = std::string_view();
  } else {
    return std::nullopt;
  }

  return LanguageTag(i18n_internal::GetBcp47TagPieces(*parsed));
}

}  // namespace base::i18n

#endif  // BASE_I18N_LANGUAGE_TAG_H_
