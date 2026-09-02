// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/tag_converters.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/internal/immutable_string.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/Locale.hpp"
#include "third_party/rust/chromium_crates_io/vendor/icu_capi-v2/bindings/cpp/icu4x/LocaleCanonicalizer.hpp"

namespace base::i18n {
namespace {

using ::base::i18n_internal::ConvertLegacyCodeToBcp47IfNecessary;
using ::base::i18n_internal::ImmutableString;

bool ShouldSkipCanonicalization(std::string_view tag) {
  size_t dash_pos = tag.find('-');
  std::string_view lang = tag.substr(0, dash_pos);
  static constexpr auto kLanguagesToSkipCanonicalization =
      base::MakeFixedFlatSet<std::string_view>({"sh"});
  return kLanguagesToSkipCanonicalization.contains(base::ToLowerASCII(lang));
}

}  // namespace

class LanguageTagConverter::Impl {
 public:
  Impl() : canonicalizer_(icu4x::LocaleCanonicalizer::create_extended()) {}
  ~Impl() = default;

  std::optional<LanguageTag> FromString(std::string_view tag) const;
  LanguageTag FromIcu4xCapiLocale(const icu4x::Locale& locale) const;

 private:
  std::unique_ptr<icu4x::LocaleCanonicalizer> canonicalizer_;
};

LanguageTag LanguageTagConverter::Impl::FromIcu4xCapiLocale(
    const icu4x::Locale& locale) const {
  std::string tag_str = locale.to_string();
  std::array<std::string_view, 1> parts = {tag_str};
  return LanguageTag(ImmutableString(parts));
}

std::optional<LanguageTag> LanguageTagConverter::Impl::FromString(
    std::string_view tag) const {
  // TODO(crbug.com/537806159): Handle private use tags.
  if (base::StartsWith(tag, "x-", base::CompareCase::INSENSITIVE_ASCII)) {
    return std::nullopt;
  }

  auto result = icu4x::Locale::from_string(tag);
  if (!result.is_ok()) {
    return std::nullopt;
  }
  std::unique_ptr<icu4x::Locale> locale = std::move(result).ok().value();

  if (!ShouldSkipCanonicalization(tag)) {
    canonicalizer_->canonicalize(*locale);
  }

  LanguageTag language_tag = FromIcu4xCapiLocale(*locale);
  return language_tag;
}

LanguageTagConverter::~LanguageTagConverter() = default;
LanguageTagConverter::LanguageTagConverter()
    : impl_(std::make_unique<Impl>()) {}

const LanguageTagConverter& LanguageTagConverter::GetInstance() {
  static base::NoDestructor<LanguageTagConverter> instance;
  return *instance;
}

LanguageTag LanguageTagConverter::FromIcu4xCapiLocale(
    const icu4x::Locale& locale) const {
  return impl_->FromIcu4xCapiLocale(locale);
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

}  // namespace base::i18n
