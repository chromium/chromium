// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag_value_converters.h"

#include <optional>
#include <string>

#include "base/i18n/internal/bcp47_parser.h"
#include "base/i18n/language_tag.h"
#include "base/values.h"

namespace base::i18n {

base::Value LanguageTagToValue(const LanguageTag& tag) {
  return base::Value(tag.tag_string());
}

std::optional<LanguageTag> ValueToLanguageTag(const base::Value* value) {
  return value ? ValueToLanguageTag(*value) : std::nullopt;
}

std::optional<LanguageTag> ValueToLanguageTag(const base::Value& value) {
  const std::string* str = value.GetIfString();
  if (!str) {
    return std::nullopt;
  }
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(*str);
  if (!parsed || !i18n_internal::AreSubtagsKnown(*parsed)) {
    return std::nullopt;
  }
  return LanguageTag(base::span<const std::string_view>({*str}));
}

}  // namespace base::i18n
