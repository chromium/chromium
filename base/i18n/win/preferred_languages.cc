// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/win/preferred_languages.h"

#include <windows.h>

#include <ostream>
#include <string_view>

#include "base/check_op.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace base::i18n {
namespace {

using GetPreferredUILanguages_Fn = decltype(::GetSystemPreferredUILanguages)*;

constexpr std::wstring_view kNullTerminator{L"\0", 1};

std::vector<LanguageTag> GetPreferredUILanguageList(
    GetPreferredUILanguages_Fn function,
    ULONG flags) {
  DCHECK_EQ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME)), 0U);
  const ULONG call_flags = flags | MUI_LANGUAGE_NAME;
  ULONG language_count = 0;
  ULONG buffer_length = 0;
  if (!function(call_flags, &language_count, nullptr, &buffer_length) ||
      !buffer_length) {
    DPCHECK(!buffer_length) << "Failed getting size of preferred UI languages.";
    return {};
  }

  std::wstring buffer(buffer_length, '\0');
  if (!function(call_flags, &language_count, std::data(buffer),
                &buffer_length) ||
      !language_count) {
    DPCHECK(!language_count) << "Failed getting preferred UI languages.";
    return {};
  }

  // The buffer has been populated with a series of strings separated by
  // terminators, which ends with a single empty string (two terminators in a
  // row). Chop off the last of those two terminators so that |buffer| is a
  // basic_string that contains the terminator ending the last string but not
  // the terminator denoting an empty string.
  buffer.resize(buffer_length - 1);
  std::vector<LanguageTag> languages;
  // Split string on NUL characters.
  ULONG languages_added = 0;
  for (auto token :
       base::SplitStringPiece(buffer, kNullTerminator, base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (std::optional<LanguageTag> language_tag =
            GetLanguageTagFromString(base::WideToASCII(token))) {
      languages.emplace_back(*language_tag);
      ++languages_added;
    }
  }
  DCHECK_EQ(languages_added, language_count);
  return languages;
}

}  // namespace

std::vector<LanguageTag> GetUserPreferredUILanguageList() {
  return GetPreferredUILanguageList(::GetUserPreferredUILanguages, 0);
}

std::vector<LanguageTag> GetThreadPreferredUILanguageList() {
  return GetPreferredUILanguageList(
      ::GetThreadPreferredUILanguages,
      MUI_MERGE_SYSTEM_FALLBACK | MUI_MERGE_USER_FALLBACK);
}

}  // namespace base::i18n
