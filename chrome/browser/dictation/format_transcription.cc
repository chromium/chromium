// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/format_transcription.h"

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"

namespace dictation {

namespace {

bool IsScriptWithoutSpaces(UChar32 c) {
  UErrorCode error = U_ZERO_ERROR;
  UScriptCode script = uscript_getScript(c, &error);
  if (U_FAILURE(error)) {
    return false;
  }
  switch (script) {
    case USCRIPT_HAN:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
    case USCRIPT_BOPOMOFO:
    case USCRIPT_THAI:
    case USCRIPT_LAO:
    case USCRIPT_KHMER:
    case USCRIPT_MYANMAR:
      return true;
    default:
      return false;
  }
}

bool IsFullWidthOrWidePunctuation(UChar32 c) {
  if (!u_ispunct(c)) {
    return false;
  }
  int width = u_getIntPropertyValue(c, UCHAR_EAST_ASIAN_WIDTH);
  return width == U_EA_FULLWIDTH || width == U_EA_WIDE;
}

bool IsPrefixCharacterOrOpenPunctuation(UChar32 c) {
  if (c == u'"' || c == u'\'' || c == u'`' || c == u'@' || c == u'#' ||
      c == u'-' || c == u'/' || c == u'\\' || c == 0x2014 /* em-dash */ ||
      c == 0x2013 /* en-dash */) {
    return true;
  }
  int type = u_charType(c);
  return type == U_START_PUNCTUATION || type == U_INITIAL_PUNCTUATION ||
         type == U_CURRENCY_SYMBOL;
}

bool IsClosingPunctuationOrAttachingPunctuation(UChar32 c) {
  if (c == u'.' || c == u',' || c == u'!' || c == u'?' || c == u':' ||
      c == u';' || c == u'"' || c == u'\'' || c == u')' || c == u']' ||
      c == u'}' || c == u'%') {
    return true;
  }
  int type = u_charType(c);
  return type == U_END_PUNCTUATION || type == U_FINAL_PUNCTUATION;
}

}  // namespace

bool WhitespaceNeeded(std::u16string_view preceding_text,
                      std::u16string_view new_text) {
  if (preceding_text.empty() || new_text.empty()) {
    return false;
  }

  base::i18n::UTF16CharIterator preceding_iter =
      base::i18n::UTF16CharIterator::LowerBound(preceding_text,
                                                preceding_text.length() - 1);
  UChar32 last_char = preceding_iter.get();

  if (base::IsUnicodeWhitespace(last_char)) {
    return false;
  }

  if (IsFullWidthOrWidePunctuation(last_char)) {
    return false;
  }

  if (IsScriptWithoutSpaces(last_char)) {
    return false;
  }

  if (IsPrefixCharacterOrOpenPunctuation(last_char)) {
    return false;
  }

  base::i18n::UTF16CharIterator new_iter(new_text);
  UChar32 first_char = new_iter.get();

  if (base::IsUnicodeWhitespace(first_char)) {
    return false;
  }

  if (IsClosingPunctuationOrAttachingPunctuation(first_char) ||
      IsFullWidthOrWidePunctuation(first_char)) {
    return false;
  }

  return true;
}

std::u16string FormatTranscription(
    const std::u16string& text,
    std::optional<std::u16string_view> preceding_text) {
  if (text.empty()) {
    return text;
  }

  bool prepend_whitespace =
      preceding_text.has_value() && WhitespaceNeeded(*preceding_text, text);

  return prepend_whitespace ? (u" " + text) : text;
}

}  // namespace dictation
