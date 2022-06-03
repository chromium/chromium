// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/i18n.h"

#include <windows.h>

#include <ostream>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

using GetPreferredUILanguages_Fn = decltype(::GetSystemPreferredUILanguages)*;

constexpr base::WStringPiece kNullTerminator{L"\0", 1};

bool GetPreferredUILanguageList(GetPreferredUILanguages_Fn function,
                                ULONG flags,
                                std::vector<std::wstring>* languages) {
  DCHECK_EQ((flags & (MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME)), 0U);
  const ULONG call_flags = flags | MUI_LANGUAGE_NAME;
  ULONG language_count = 0;
  ULONG buffer_length = 0;
  if (!function(call_flags, &language_count, nullptr, &buffer_length) ||
      !buffer_length) {
    DPCHECK(!buffer_length) << "Failed getting size of preferred UI languages.";
    return false;
  }

  std::wstring buffer(buffer_length, '\0');
  if (!function(call_flags, &language_count, base::data(buffer),
                &buffer_length) ||
      !language_count) {
    DPCHECK(!language_count) << "Failed getting preferred UI languages.";
    return false;
  }

  // The buffer has been populated with a series of strings separated by
  // terminators, which ends with a single empty string (two terminators in a
  // row). Chop off the last of those two terminators so that |buffer| is a
  // basic_string that contains the terminator ending the last string but not
  // the terminator denoting an empty string.
  buffer.resize(buffer_length - 1);

  // Split string on NUL characters.
  ULONG languages_added = 0;
  for (auto token :
       base::SplitStringPiece(buffer, kNullTerminator, base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    languages->emplace_back(token);
    ++languages_added;
  }
  DCHECK_EQ(languages_added, language_count);
  return true;
}

}  // namespace

namespace base {
namespace win {
namespace i18n {

bool GetUserPreferredUILanguageList(std::vector<std::wstring>* languages) {
  DCHECK(languages);
  return GetPreferredUILanguageList(::GetUserPreferredUILanguages, 0,
                                    languages);
}

bool GetThreadPreferredUILanguageList(std::vector<std::wstring>* languages) {
  DCHECK(languages);
  return GetPreferredUILanguageList(
      ::GetThreadPreferredUILanguages,
      MUI_MERGE_SYSTEM_FALLBACK | MUI_MERGE_USER_FALLBACK, languages);
}

}  // namespace i18n
}  // namespace win
}  // namespace base
