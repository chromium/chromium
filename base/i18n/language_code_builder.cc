// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code_builder.h"

#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/i18n/language_code.h"
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

class LanguageCodeBuilder::Impl {
 public:
  explicit Impl() : canonicalizer_(create_icu_canonicalizer()) {}
  ~Impl() = default;

  std::optional<LanguageCode> FromString(std::string_view code) const;
  LanguageCode FromIcu4xLocale(const Icu4xLocale& icu_locale) const;

 private:
  rust::Box<base::i18n::internal::IcuCanonicalizer> canonicalizer_;
};

LanguageCode LanguageCodeBuilder::Impl::FromIcu4xLocale(
    const Icu4xLocale& icu_locale) const {
  return LanguageCode(ImmutableStringFromIcu4xLocale(icu_locale));
}

std::optional<LanguageCode> LanguageCodeBuilder::Impl::FromString(
    std::string_view code) const {
  rust::Slice<const uint8_t> locale_bytes(
      reinterpret_cast<const uint8_t*>(code.data()), code.size());

  // Use the new OptionalIcu4xLocale return type.
  i18n::internal::OptionalIcu4xLocale opt_locale =
      canonicalizer_->canonicalize(locale_bytes);

  if (!opt_locale.has_value) {
    return std::nullopt;
  }

  return FromIcu4xLocale(*opt_locale.value);
}

LanguageCodeBuilder::~LanguageCodeBuilder() = default;
LanguageCodeBuilder::LanguageCodeBuilder() : impl_(std::make_unique<Impl>()) {}

const LanguageCodeBuilder& LanguageCodeBuilder::GetInstance() {
  static base::NoDestructor<LanguageCodeBuilder> instance;
  return *instance;
}

LanguageCode LanguageCodeBuilder::FromIcu4xLocale(
    const Icu4xLocale& icu_locale) const {
  return impl_->FromIcu4xLocale(icu_locale);
}

std::optional<LanguageCode> LanguageCodeBuilder::FromString(
    std::string_view code) const {
  // A valid BCP47 language code is at least 2 chars (e.g. "en")
  if (code.size() < 2) {
    return std::nullopt;
  }

  std::optional<std::string> bcp47_converted_code =
      ConvertLegacyCodeToBcp47IfNecessary(code);
  // If there is no value, the code is already bcp47-compatible and extra copies
  // can be avoided.
  if (!bcp47_converted_code.has_value()) {
    return impl_->FromString(code);
  }

  return impl_->FromString(*bcp47_converted_code);
}

}  // namespace base
