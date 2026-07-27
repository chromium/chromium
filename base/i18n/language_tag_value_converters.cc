// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag_value_converters.h"

#include <optional>
#include <string>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
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
  return LanguageTagConverter::GetInstance().FromString(*str);
}

}  // namespace base::i18n
