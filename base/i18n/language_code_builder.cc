// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code_builder.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/language_code.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace base {

namespace {

constexpr std::string_view kBcp47SubtagSeparator = "-";

// Reconstructs the BCP47 language tag from the locale components.
// This is the C++ implementation of the locale string reconstruction that
// avoids extra allocations by allowing ImmutableString to join the parts
// directly into its storage.
i18n::internal::ImmutableString Icu4xLocaleToImmutableString(
    const i18n::internal::Icu4xLocale& locale) {
  std::vector<std::string_view> parts;

  // We must keep the temporary strings alive until ImmutableString has copied
  // them.
  rust::Vec<rust::String> variants = locale.variants();
  rust::Vec<rust::String> extensions = locale.extensions_as_strings();
  rust::Str script = locale.script();
  rust::Str region = locale.region();

  parts.emplace_back(locale.language());

  if (!script.empty()) {
    parts.push_back(kBcp47SubtagSeparator);
    parts.push_back(std::string_view(script.data(), script.size()));
  }

  if (!region.empty()) {
    parts.push_back(kBcp47SubtagSeparator);
    parts.push_back(std::string_view(region.data(), region.size()));
  }

  for (const rust::String& variant : variants) {
    parts.push_back(kBcp47SubtagSeparator);
    parts.push_back(std::string_view(variant.data(), variant.size()));
  }

  for (const rust::String& ext : extensions) {
    parts.push_back(kBcp47SubtagSeparator);
    parts.push_back(std::string_view(ext.data(), ext.size()));
  }

  return i18n::internal::ImmutableString(parts);
}

}  // namespace

class LanguageCodeBuilder::Impl {
 public:
  explicit Impl()
      : canonicalizer_(base::i18n::internal::create_icu_canonicalizer()) {}
  ~Impl() = default;

  std::optional<LanguageCode> FromString(std::string_view code) const;

 private:
  rust::Box<base::i18n::internal::IcuCanonicalizer> canonicalizer_;
};

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

  return LanguageCode(Icu4xLocaleToImmutableString(*opt_locale.value));
}

LanguageCodeBuilder::~LanguageCodeBuilder() = default;
LanguageCodeBuilder::LanguageCodeBuilder() : impl_(std::make_unique<Impl>()) {}

const LanguageCodeBuilder& LanguageCodeBuilder::GetInstance() {
  static base::NoDestructor<LanguageCodeBuilder> instance;
  return *instance;
}

std::optional<LanguageCode> LanguageCodeBuilder::FromString(
    std::string_view code) const {
  // A valid BCP47 language code is at least 2 chars (e.g. "en")
  if (code.size() < 2) {
    return std::nullopt;
  }

  return impl_->FromString(code);
}

}  // namespace base
