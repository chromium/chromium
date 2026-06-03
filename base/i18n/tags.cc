// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/tags.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"

namespace base::language_tags {
namespace {

LanguageTag CreateChecked(std::string_view tag) {
  std::optional<LanguageTag> lang_tag =
      LanguageTagConverter::GetInstance().FromString(tag);
  CHECK(lang_tag.has_value()) << "Invalid language tag: " << tag;
  return std::move(lang_tag).value();
}

}  // namespace

#define IMPL_LANGUAGECODE_TAG_NAME(tag, name)                               \
  const base::LanguageTag& name() {                                         \
    static const base::NoDestructor<LanguageTag> kname(CreateChecked(tag)); \
    return *kname;                                                          \
  }

#include "base/i18n/internal/canonical_language_tags.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME

}  // namespace base::language_tags
