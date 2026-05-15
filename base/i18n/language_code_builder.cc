// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code_builder.h"

#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/language_code.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace base {

class LanguageCodeBuilder::Impl {
 public:
  explicit Impl()
      : canonicalizer_(base::i18n::internal::create_icu_canonicalizer()) {}
  ~Impl() = default;

  std::optional<std::string> Canonicalize(std::string_view code) const;

 private:
  rust::Box<base::i18n::internal::IcuCanonicalizer> canonicalizer_;
};

std::optional<std::string> LanguageCodeBuilder::Impl::Canonicalize(
    std::string_view code) const {
  std::string canonicalized;
  rust::Slice<const uint8_t> locale_bytes(
      reinterpret_cast<const uint8_t*>(code.data()), code.size());

  canonicalizer_->create_canonical(locale_bytes, canonicalized);
  if (canonicalized.empty()) {
    return std::nullopt;
  }

  return canonicalized;
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

  std::optional<std::string> canonicalized = impl_->Canonicalize(code);
  if (!canonicalized.has_value()) {
    return std::nullopt;
  }

  return LanguageCode(*canonicalized);
}

}  // namespace base
