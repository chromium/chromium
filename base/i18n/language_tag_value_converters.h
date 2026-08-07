// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_TAG_VALUE_CONVERTERS_H_
#define BASE_I18N_LANGUAGE_TAG_VALUE_CONVERTERS_H_

#include <optional>

#include "base/component_export.h"
#include "base/i18n/language_tag.h"

namespace base {
class Value;
}

namespace base::i18n {

// Converts a LanguageTag to a string base::Value.
COMPONENT_EXPORT(LANGUAGE_TAG)
base::Value LanguageTagToValue(const LanguageTag& tag);

// Parses a LanguageTag from a base::Value.
// Returns std::nullopt if `value` is nullptr, not a string Value, or not a
// valid BCP 47 language tag.
COMPONENT_EXPORT(LANGUAGE_TAG)
std::optional<LanguageTag> ValueToLanguageTag(const base::Value* value);
COMPONENT_EXPORT(LANGUAGE_TAG)
std::optional<LanguageTag> ValueToLanguageTag(const base::Value& value);

}  // namespace base::i18n

#endif  // BASE_I18N_LANGUAGE_TAG_VALUE_CONVERTERS_H_
