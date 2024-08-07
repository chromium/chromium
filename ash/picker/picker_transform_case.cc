// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_transform_case.h"

#include <string>
#include <string_view>

#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace ash {

// TODO: b/333490858 - These functions should take in a locale as a parameter
// instead of using the default locale.

std::u16string PickerTransformToLowerCase(std::u16string_view text) {
  return base::i18n::ToLower(text);
}

std::u16string PickerTransformToUpperCase(std::u16string_view text) {
  return base::i18n::ToUpper(text);
}

std::u16string PickerTransformToTitleCase(std::u16string_view text) {
  icu::UnicodeString unicode_text(text.data(), text.length());
  return base::i18n::UnicodeStringToString16(
      unicode_text.toTitle(/*titleIter=*/nullptr));
}

}  // namespace ash
