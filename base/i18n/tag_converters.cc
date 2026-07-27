// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/tag_converters.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/internal/immutable_string.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {
namespace {

constexpr std::string_view kBcp47SubtagSeparator = "-";

using ::base::i18n_internal::ConvertLegacyCodeToBcp47IfNecessary;
using ::base::i18n_internal::create_icu_canonicalizer;
using ::base::i18n_internal::create_icu_locale;
using ::base::i18n_internal::Icu4xLocale;

bool ShouldSkipCanonicalization(std::string_view tag) {
  size_t dash_pos = tag.find('-');
  std::string_view lang = tag.substr(0, dash_pos);
  static constexpr auto kLanguagesToSkipCanonicalization =
      base::MakeFixedFlatSet<std::string_view>({"sh"});
  return kLanguagesToSkipCanonicalization.contains(base::ToLowerASCII(lang));
}

i18n_internal::ImmutableString ImmutableStringFromIcu4xLocale(
    const i18n_internal::Icu4xLocale& locale) {
  std::vector<std::string_view> parts;

  // We must keep the temporary strings alive until ImmutableString has
  // copied them.

  rust::Vec<rust::String> variants = locale.variants();
  rust::Vec<rust::String> extensions = locale.extensions_as_strings();
  rust::Str script = locale.script();
  rust::Str region = locale.region();

  parts.emplace_back(locale.language());

  if (!script.empty()) {
    parts.emplace_back(kBcp47SubtagSeparator);
    parts.emplace_back(script.data(), script.size());
  }

  if (!region.empty()) {
    parts.emplace_back(kBcp47SubtagSeparator);
    parts.emplace_back(region.data(), region.size());
  }

  for (const rust::String& variant : variants) {
    parts.emplace_back(kBcp47SubtagSeparator);
    parts.emplace_back(variant.data(), variant.size());
  }

  for (const rust::String& ext : extensions) {
    parts.emplace_back(kBcp47SubtagSeparator);
    parts.emplace_back(ext.data(), ext.size());
  }

  return i18n_internal::ImmutableString(parts);
}

}  // namespace

class LanguageTagConverter::Impl {
 public:
  Impl() : canonicalizer_(create_icu_canonicalizer()) {}
  ~Impl() = default;

  std::optional<LanguageTag> FromString(std::string_view tag) const;
  LanguageTag FromIcu4xLocale(const Icu4xLocale& icu_locale) const;

 private:
  rust::Box<i18n_internal::IcuCanonicalizer> canonicalizer_;
};

LanguageTag LanguageTagConverter::Impl::FromIcu4xLocale(
    const Icu4xLocale& icu_locale) const {
  return LanguageTag(ImmutableStringFromIcu4xLocale(icu_locale));
}

std::optional<LanguageTag> LanguageTagConverter::Impl::FromString(
    std::string_view tag) const {
  rust::Slice<const uint8_t> locale_bytes(
      reinterpret_cast<const uint8_t*>(tag.data()), tag.size());

  // Skip canonicalization for "tl" and "sh".
  i18n_internal::OptionalIcu4xLocale opt_locale =
      ShouldSkipCanonicalization(tag)
          ? create_icu_locale(locale_bytes)
          : canonicalizer_->canonicalize(locale_bytes);

  if (!opt_locale.has_value) {
    return std::nullopt;
  }

  return FromIcu4xLocale(*opt_locale.value);
}

LanguageTagConverter::~LanguageTagConverter() = default;
LanguageTagConverter::LanguageTagConverter()
    : impl_(std::make_unique<Impl>()) {}

const LanguageTagConverter& LanguageTagConverter::GetInstance() {
  static base::NoDestructor<LanguageTagConverter> instance;
  return *instance;
}

LanguageTag LanguageTagConverter::FromIcu4xLocale(
    const Icu4xLocale& icu_locale) const {
  return impl_->FromIcu4xLocale(icu_locale);
}

LanguageTag LanguageTagConverter::FromIcuLocale(
    const icu::Locale& icu_locale) const {
  UErrorCode status = U_ZERO_ERROR;
  std::string tag = icu_locale.toLanguageTag<std::string>(status);
  if (U_FAILURE(status)) {
    return GetKnownLanguageTag("und");
  }

  // The `tag` returned by ICU4C will certainly produce a valid LanguageTag,
  // that is why we always return a valid LanguageTag.
  // Note: we call FromString on the `tag` to make sure we apply the same
  // cannonicalizations.
  return FromString(tag).value_or(GetKnownLanguageTag("und"));
}

std::optional<LanguageTag> LanguageTagConverter::FromString(
    std::string_view tag) const {
  // A valid BCP47 language tag is at least 2 chars (e.g. "en")
  if (tag.size() < 2) {
    return std::nullopt;
  }

  std::optional<std::string> bcp47_converted_tag =
      ConvertLegacyCodeToBcp47IfNecessary(tag);
  // If there is no value, the code is already bcp47-compatible and extra copies
  // can be avoided.
  if (!bcp47_converted_tag.has_value()) {
    return impl_->FromString(tag);
  }

  return impl_->FromString(*bcp47_converted_tag);
}

std::optional<LanguageTag> GetLanguageTagFromString(std::string_view tag) {
  return LanguageTagConverter::GetInstance().FromString(tag);
}

IcuLocaleConverter::IcuLocaleConverter() {
  std::vector<std::pair<std::string, icu::Locale>> locales;

#define IMPL_LANGUAGECODE_TAG_NAME(tag, name)                            \
  {                                                                      \
    UErrorCode status = U_ZERO_ERROR;                                    \
    locales.emplace_back(tag, icu::Locale::forLanguageTag(tag, status)); \
  }
#include "base/i18n/internal/canonical_language_tags.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME

  cached_locales_ =
      base::flat_map<std::string, icu::Locale>(std::move(locales));
}

IcuLocaleConverter::~IcuLocaleConverter() = default;

// static
const IcuLocaleConverter& IcuLocaleConverter::GetInstance() {
  static base::NoDestructor<IcuLocaleConverter> instance;
  return *instance;
}

icu::Locale IcuLocaleConverter::FromLanguageTag(
    const LanguageTag& language_tag) const {
  auto it = cached_locales_.find(language_tag.tag_string());
  if (it != cached_locales_.end()) {
    return it->second;
  }
  UErrorCode status = U_ZERO_ERROR;
  return icu::Locale::forLanguageTag(language_tag.tag_string(), status);
}

}  // namespace base::i18n
