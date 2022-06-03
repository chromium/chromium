// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File utilities that use the ICU library go in this file.

#include "base/i18n/file_util_icu.h"

#include <stdint.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/string_compare.h"
#include "base/memory/singleton.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/icu/source/common/unicode/uniset.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace base {
namespace i18n {

namespace {

class IllegalCharacters {
 public:
  IllegalCharacters(const IllegalCharacters&) = delete;
  IllegalCharacters& operator=(const IllegalCharacters&) = delete;

  static IllegalCharacters* GetInstance() {
    return Singleton<IllegalCharacters>::get();
  }

  bool IsDisallowedEverywhere(UChar32 ucs4) const {
    return !!illegal_anywhere_.contains(ucs4);
  }

  bool IsDisallowedLeadingOrTrailing(UChar32 ucs4) const {
    return !!illegal_at_ends_.contains(ucs4);
  }

  bool IsAllowedName(const std::u16string& s) const {
    return s.empty() || (!!illegal_anywhere_.containsNone(
                             icu::UnicodeString(s.c_str(), s.size())) &&
                         !illegal_at_ends_.contains(*s.begin()) &&
                         !illegal_at_ends_.contains(*s.rbegin()));
  }

 private:
  friend struct DefaultSingletonTraits<IllegalCharacters>;

  IllegalCharacters();
  ~IllegalCharacters() = default;

  // Set of characters considered invalid anywhere inside a filename.
  icu::UnicodeSet illegal_anywhere_;

  // Set of characters considered invalid at either end of a filename.
  icu::UnicodeSet illegal_at_ends_;
};

IllegalCharacters::IllegalCharacters() {
  UErrorCode everywhere_status = U_ZERO_ERROR;
  UErrorCode ends_status = U_ZERO_ERROR;
  // Control characters, formatting characters, non-characters, path separators,
  // and some printable ASCII characters regarded as dangerous ('"*/:<>?\\').
  // See http://blogs.msdn.com/michkap/archive/2006/11/03/941420.aspx
  // and http://msdn2.microsoft.com/en-us/library/Aa365247.aspx
  // Note that code points in the "Other, Format" (Cf) category are ignored on
  // HFS+ despite the ZERO_WIDTH_JOINER and ZERO_WIDTH_NON-JOINER being
  // legitimate in Arabic and some S/SE Asian scripts. In addition tilde (~) is
  // also excluded due to the possibility of interacting poorly with short
  // filenames on VFAT. (Related to CVE-2014-9390)
  illegal_anywhere_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[\"~*/:<>?\\\\|][:Cc:][:Cf:]]"),
                      everywhere_status);
  illegal_at_ends_ =
      icu::UnicodeSet(UNICODE_STRING_SIMPLE("[[:WSpace:][.]]"), ends_status);
  DCHECK(U_SUCCESS(everywhere_status));
  DCHECK(U_SUCCESS(ends_status));

  // Add non-characters. If this becomes a performance bottleneck by
  // any chance, do not add these to |set| and change IsFilenameLegal()
  // to check |ucs4 & 0xFFFEu == 0xFFFEu|, in addition to calling
  // IsAllowedName().
  illegal_anywhere_.add(0xFDD0, 0xFDEF);
  for (int i = 0; i <= 0x10; ++i) {
    int plane_base = 0x10000 * i;
    illegal_anywhere_.add(plane_base + 0xFFFE, plane_base + 0xFFFF);
  }
  illegal_anywhere_.freeze();
  illegal_at_ends_.freeze();
}

// Returns the code point at position |cursor| in |file_name|, and increments
// |cursor| to the next position.
UChar32 GetNextCodePoint(const FilePath::StringType* const file_name,
                         int& cursor) {
  UChar32 code_point;
#if defined(OS_WIN)
  // Windows uses UTF-16 encoding for filenames.
  U16_NEXT(file_name->data(), cursor, static_cast<int>(file_name->length()),
           code_point);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Mac and Chrome OS use UTF-8 encoding for filenames.
  // Linux doesn't actually define file system encoding. Try to parse as
  // UTF-8.
  U8_NEXT(file_name->data(), cursor, static_cast<int>(file_name->length()),
          code_point);
#else
#error Unsupported platform
#endif
  return code_point;
}

}  // namespace

bool IsFilenameLegal(const std::u16string& file_name) {
  return IllegalCharacters::GetInstance()->IsAllowedName(file_name);
}

void ReplaceIllegalCharactersInPath(FilePath::StringType* file_name,
                                    char replace_char) {
  IllegalCharacters* illegal = IllegalCharacters::GetInstance();

  DCHECK(!(illegal->IsDisallowedEverywhere(replace_char)));
  const bool is_replace_char_illegal_at_ends =
      illegal->IsDisallowedLeadingOrTrailing(replace_char);
  // Keep track of the earliest and latest legal begin/end characters and file-
  // extension separator encountered, -1 if none yet.
  int unreplaced_legal_range_begin = -1;
  int unreplaced_legal_range_end = -1;
  int last_extension_separator = -1;
  static const UChar32 kExtensionSeparator =
      checked_cast<UChar32>(FilePath::kExtensionSeparator);

  int cursor = 0;  // The ICU macros expect an int.
  while (cursor < static_cast<int>(file_name->size())) {
    int char_begin = cursor;
    const UChar32 code_point = GetNextCodePoint(file_name, cursor);

    const bool is_illegal_at_ends =
        illegal->IsDisallowedLeadingOrTrailing(code_point);

    if (illegal->IsDisallowedEverywhere(code_point) ||
        ((char_begin == 0 || cursor == static_cast<int>(file_name->length())) &&
         is_illegal_at_ends && !is_replace_char_illegal_at_ends)) {
      file_name->replace(char_begin, cursor - char_begin, 1, replace_char);
      // We just made the potentially multi-byte/word char into one that only
      // takes one byte/word, so need to adjust the cursor to point to the next
      // character again.
      cursor = char_begin + 1;
    } else if (!is_illegal_at_ends) {
      if (unreplaced_legal_range_begin == -1)
        unreplaced_legal_range_begin = char_begin;
      unreplaced_legal_range_end = cursor;
    }

    if (code_point == kExtensionSeparator)
      last_extension_separator = char_begin;
  }

  // If |replace_char| is not a legal starting/ending character, ensure that
  // |replace_char| is not the first nor last character in |file_name|.
  if (is_replace_char_illegal_at_ends) {
    if (unreplaced_legal_range_begin == -1) {
      // |file_name| has no characters that are legal at ends; enclose in '_'s.
      file_name->insert(file_name->begin(), FILE_PATH_LITERAL('_'));
      file_name->append(FILE_PATH_LITERAL("_"));
    } else {
      // Trim trailing instances of |replace_char| and other characters that are
      // illegal at ends.
      file_name->erase(unreplaced_legal_range_end, FilePath::StringType::npos);

      // Trim leading instances of |replace_char| and other characters that are
      // illegal at ends, while ensuring that the file-extension separator is
      // not removed if present. The file-extension separator is considered the
      // last '.' in |file_name| followed by a legal character.
      if (last_extension_separator != -1 &&
          last_extension_separator == unreplaced_legal_range_begin - 1) {
        // If the file-extension separator is at the start of the resulting
        // |file_name|, prepend '_' instead of trimming it, e.g.,
        // "***.txt" -> "_.txt".
        file_name->erase(0, last_extension_separator);
        file_name->insert(file_name->begin(), FILE_PATH_LITERAL('_'));
      } else {
        file_name->erase(0, unreplaced_legal_range_begin);
      }
    }
    DCHECK(!file_name->empty());
  }
}

bool LocaleAwareCompareFilenames(const FilePath& a, const FilePath& b) {
  UErrorCode error_code = U_ZERO_ERROR;
  // Use the default collator. The default locale should have been properly
  // set by the time this constructor is called.
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));
  DCHECK(U_SUCCESS(error_code));
  // Make it case-sensitive.
  collator->setStrength(icu::Collator::TERTIARY);

#if defined(OS_WIN)
  return CompareString16WithCollator(*collator, AsStringPiece16(a.value()),
                                     AsStringPiece16(b.value())) == UCOL_LESS;

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // On linux, the file system encoding is not defined. We assume
  // SysNativeMBToWide takes care of it.
  return CompareString16WithCollator(
             *collator, WideToUTF16(SysNativeMBToWide(a.value())),
             WideToUTF16(SysNativeMBToWide(b.value()))) == UCOL_LESS;
#endif
}

void NormalizeFileNameEncoding(FilePath* file_name) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string normalized_str;
  if (ConvertToUtf8AndNormalize(file_name->BaseName().value(), kCodepageUTF8,
                                &normalized_str) &&
      !normalized_str.empty()) {
    *file_name = file_name->DirName().Append(FilePath(normalized_str));
  }
#endif
}

}  // namespace i18n
}  // namespace base
