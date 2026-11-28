// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/unicode_string_util.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf16.h"

namespace base::i18n {

namespace {

bool IsUnicodeFormat(UChar32 c) {
  return u_charType(c) == U_FORMAT_CHAR;
}

bool IsUnicodeControlOrFormat(UChar32 c) {
  return base::IsUnicodeControl(c) || IsUnicodeFormat(c);
}

}  // namespace

bool ContainsUnicodeControlOrFormat(std::string_view str) {
  for (UTF8CharIterator iter(str); !iter.end(); iter.Advance()) {
    if (IsUnicodeControlOrFormat(iter.get())) {
      return true;
    }
  }
  return false;
}

bool ContainsUnicodeControlOrFormat(std::u16string_view str) {
  for (size_t i = 0; i < str.length();) {
    UChar32 c;
    U16_NEXT(str, i, str.length(), c);
    if (IsUnicodeControlOrFormat(c)) {
      return true;
    }
  }
  return false;
}

bool ContainsUnicodeControlOrFormatLatin1(base::span<const uint8_t> str) {
  return std::ranges::any_of(str, IsUnicodeControlOrFormat);
}

bool RemoveUnicodeControlAndFormat(std::u16string& str) {
  if (!ContainsUnicodeControlOrFormat(str)) {
    return false;
  }

  std::u16string sanitized;
  sanitized.reserve(str.size());
  for (size_t i = 0; i < str.length();) {
    UChar32 c;
    U16_NEXT_OR_FFFD(str, i, str.length(), c);
    if (!IsUnicodeControlOrFormat(c)) {
      WriteUnicodeCharacter(c, &sanitized);
    }
  }
  str = std::move(sanitized);
  return true;
}

}  // namespace base::i18n
