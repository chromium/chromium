// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/tag_converters.h"

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace base {
namespace {

constexpr std::string_view kBcp47SubtagSeparator = "-";

using ::base::i18n::internal::ConvertLegacyCodeToBcp47IfNecessary;
using ::base::i18n::internal::create_icu_canonicalizer;
using ::base::i18n::internal::Icu4xLocale;

i18n::internal::ImmutableString ImmutableStringFromIcu4xLocale(
    const i18n::internal::Icu4xLocale& locale) {
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

  return i18n::internal::ImmutableString(parts);
}

}  // namespace

class LanguageTagConverter::Impl {
 public:
  explicit Impl() : canonicalizer_(create_icu_canonicalizer()) {}
  ~Impl() = default;

  std::optional<LanguageTag> FromString(std::string_view tag) const;
  LanguageTag FromIcu4xLocale(const Icu4xLocale& icu_locale) const;

 private:
  rust::Box<base::i18n::internal::IcuCanonicalizer> canonicalizer_;
};

LanguageTag LanguageTagConverter::Impl::FromIcu4xLocale(
    const Icu4xLocale& icu_locale) const {
  return LanguageTag(ImmutableStringFromIcu4xLocale(icu_locale));
}

std::optional<LanguageTag> LanguageTagConverter::Impl::FromString(
    std::string_view tag) const {
  rust::Slice<const uint8_t> locale_bytes(
      reinterpret_cast<const uint8_t*>(tag.data()), tag.size());

  // Use the new OptionalIcu4xLocale return type.
  i18n::internal::OptionalIcu4xLocale opt_locale =
      canonicalizer_->canonicalize(locale_bytes);

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

}  // namespace base
