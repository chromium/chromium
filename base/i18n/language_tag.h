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

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/internal/bcp47_parser.h"
#include "base/i18n/internal/immutable_string.h"

namespace base {
class Value;
}  // namespace base

namespace base::i18n {

class LanguageTagConverter;
class LanguageTag;
consteval LanguageTag GetKnownLanguageTag(std::string_view);
COMPONENT_EXPORT(LANGUAGE_TAG)
std::optional<LanguageTag> ValueToLanguageTag(const base::Value&);

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
class COMPONENT_EXPORT(LANGUAGE_TAG) LanguageTag {
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
    std::string_view tag = tag_string();
    size_t hyphen_pos = tag.find('-');
    return hyphen_pos == std::string_view::npos ? tag
                                                : tag.substr(0, hyphen_pos);
  }
  // Creates a new `LanguageTag` containing only the language subtag.
  LanguageTag WithLanguageSubtagOnly() const;

  // Returns the script subtag in the language tag, if present.
  // Examples:
  // - "zh-Hant-TW" -> "Hant"
  // - "zh-TW" -> ""
  // - "sr-Latn" -> "Latn"
  // - "zh-Hans" -> "Hans"
  constexpr std::string_view script_subtag() const LIFETIME_BOUND {
    return i18n_internal::ParseBcp47Tag(tag_string())
        .value_or(i18n_internal::ParsedBcp47Tag())
        .script;
  }
  // Returns the region subtag in the language tag if present.
  // Examples:
  // - "en-US" -> "US"
  // - "zh-Hant-TW" -> "TW"
  // - "en" -> ""
  // - "sr-Latn" -> ""
  constexpr std::string_view region_subtag() const LIFETIME_BOUND {
    return i18n_internal::ParseBcp47Tag(tag_string())
        .value_or(i18n_internal::ParsedBcp47Tag())
        .region;
  }

  // Returns the variant subtags in the language tag if present.
  // Examples:
  // - "en-US" -> []
  // - "en-GB-oxendict" -> ["oxendict"]
  // - "sl-IT-rozaj-biske" -> ["biske", "rozaj"]
  constexpr std::vector<std::string_view> variant_subtags() const
      LIFETIME_BOUND {
    return i18n_internal::ParseBcp47Tag(tag_string())
        .value_or(i18n_internal::ParsedBcp47Tag())
        .variants;
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
  // Returns the lineage of this language tag, starting with the tag itself and
  // traversing up the parent hierarchy.
  // Example:
  //  "sr-Latn-RS" -> ["sr-Latn-RS", "sr-Latn", "sr"]
  constexpr std::vector<LanguageTag> GetLineage() const;

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
    std::vector<std::string_view> extension_subtags =
        GetExtensionSubtagsInternal(extid);
    if (extension_subtags.empty()) {
      return std::nullopt;
    }

    return traits.Factory(base::PassKey<LanguageTag>(), extension_subtags);
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

  // Removes the extension keyed by `key`.
  // Examples:
  // "en-u-ca-gregory".WithExtensionRemoved("u") -> "en"
  // "en-u-ca-gregory".WithExtensionRemoved("t") -> "en-u-ca-gregory"
  LanguageTag WithExtensionRemoved(char key) const;

 private:
  friend class LanguageTagConverter;
  friend consteval LanguageTag GetKnownLanguageTag(std::string_view);
  // Allow Mojo StructTraits to default-construct an instance during IPC
  // deserialization
  friend struct mojo::StructTraits<mojo_base::mojom::LanguageTagDataView,
                                   base::i18n::LanguageTag>;
  // Allow base::Value conversion from and to `LanguageTag` without having to
  // depend on ICU.
  friend COMPONENT_EXPORT(LANGUAGE_TAG)
      std::optional<LanguageTag> ValueToLanguageTag(const base::Value&);

  // Default constructor is intended for internal use by Mojo StructTraits to
  // allow for deserialization of the language tag from IPC.
  // `mojo::DefaultConstruct` cannot be used here because of layered
  // dependencies.
  LanguageTag();

  std::vector<std::string_view> GetExtensionSubtagsInternal(char key) const;
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

COMPONENT_EXPORT(LANGUAGE_TAG)
std::ostream& operator<<(std::ostream& os, const LanguageTag& lt);

COMPONENT_EXPORT(LANGUAGE_TAG)
std::ostream& operator<<(std::ostream& os,
                         const std::optional<LanguageTag>& opt);

// Returns a LanguageTag checked at compile time. does not compile if tag is
// not one of the predefined supported language tags.
// The function expects that the tags are well-formed and normalized, which
// means:
// - language subtag: must be all lowercase (en).
// - script subtag: must have only the first letter uppercase (Latn).
// - region subtag: must be all uppercase (US).
// - variant subtags: must be all lowercase (oxendict).
//
// The function currently does not support extensions or tags that have fewer
// than 14 characters; that is when LanguageTag fits in the stack.
//
// Usage examples:
//     // OK
//   - GetKnownLanguageTag("en-US");
//
//     // Failed compilation: the tag is not well formed as the country code
//     // subtag is expected to be all uppercase.
//   - GetKnownLanguageTag("en-us");
//
//     // Failed compilation: the tag is not known "xx" even though it is
//     // well-formed according to the BCP47 standard.
//   - GetKnownLanguageTag("xx");
//
consteval LanguageTag GetKnownLanguageTag(std::string_view tag) {
  // It is only possible to construct `LanguageTag`s at compile-time if they
  // are small.
  if (tag.size() > i18n_internal::ImmutableString::kSmallBufferSize) {
    void ERROR_TagIsTooLarge();
    ERROR_TagIsTooLarge();
  }

  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag);
  // Check if the input `tag` is a well-formed bcp47 tag
  if (!parsed) {
    void ERROR_TagIsMalformed();
    ERROR_TagIsMalformed();
  }
  // Check that the subtags are known.
  if (!i18n_internal::AreSubtagsKnown(*parsed)) {
    void ERROR_TagIsUnknown();
    ERROR_TagIsUnknown();
  }

  return LanguageTag(base::span<const std::string_view>({tag}));
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

constexpr std::vector<LanguageTag> LanguageTag::GetLineage() const {
  std::vector<LanguageTag> lineage;
  for (std::optional<LanguageTag> tag = *this; tag; tag = tag->GetParentTag()) {
    lineage.push_back(*tag);
  }
  return lineage;
}

}  // namespace base::i18n

#endif  // BASE_I18N_LANGUAGE_TAG_H_
