// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_codes.h"

#include "base/i18n/language_code.h"
#include "base/i18n/language_code_builder.h"
#include "base/no_destructor.h"

namespace base::language_codes {
namespace {

LanguageCode CreateChecked(std::string_view code) {
  std::optional<LanguageCode> lang_code =
      LanguageCodeBuilder::GetInstance().FromString(code);
  CHECK(lang_code.has_value()) << "Invalid language code: " << code;
  return std::move(lang_code).value();
}

}  // namespace

#define IMPL_LANGUAGECODE_TAG_NAME(tag, name)                                \
  const base::LanguageCode& name() {                                         \
    static const base::NoDestructor<LanguageCode> kname(CreateChecked(tag)); \
    return *kname;                                                           \
  }

#include "base/i18n/internal/canonical_language_codes.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME

}  // namespace base::language_codes
