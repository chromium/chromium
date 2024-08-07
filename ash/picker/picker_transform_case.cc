// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_transform_case.h"

#include <string>
#include <string_view>

#include "base/i18n/case_conversion.h"

namespace ash {
namespace {

bool u16_isalpha(char16_t ch) {
  return (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}

}  // namespace

std::u16string PickerTransformToLowerCase(std::u16string_view text) {
  return base::i18n::ToLower(text);
}

std::u16string PickerTransformToUpperCase(std::u16string_view text) {
  return base::i18n::ToUpper(text);
}

std::u16string PickerTransformToTitleCase(std::u16string_view text) {
  std::u16string result(text);
  std::u16string uppercase_text = base::i18n::ToUpper(text);
  for (size_t i = 0; i < result.length(); i++) {
    if (u16_isalpha(result[i]) && (i == 0 || result[i - 1] == u' ')) {
      result[i] = uppercase_text[i];
    }
  }
  return result;
}

}  // namespace ash
