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

bool IsSentenceEndingPeriod(char16_t c) {
  return c == u'.' || c == u'。';
}

bool IsEllipsisAt(std::u16string_view text, size_t period_index) {
  return period_index > 0 && IsSentenceEndingPeriod(text[period_index - 1]);
}

bool HasAtMostOneSentence(std::u16string_view text, size_t last_period_index) {
  // If there are interior sentence terminators or newlines before the final
  // period, text contains multiple sentences.
  for (size_t i = 0; i < last_period_index; ++i) {
    char16_t c = text[i];
    if (c == u'!' || c == u'?' || c == u'。' || c == u'\n' || c == u'\r') {
      return false;
    }
    if (c == u'.') {
      // Ignore decimal numbers (e.g. "3.14").
      if (i > 0 && i + 1 < last_period_index &&
          base::IsAsciiDigit(text[i - 1]) && base::IsAsciiDigit(text[i + 1])) {
        continue;
      }
      return false;
    }
  }
  return true;
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

// TODO(b/561503223): Review and re-evaluate sentence-ending punctuation
// handling when extending beyond en-US.
std::u16string TrimTrailingPeriodIfSingleSentence(std::u16string_view text) {
  std::u16string_view trimmed = base::TrimWhitespace(text, base::TRIM_TRAILING);
  if (trimmed.empty()) {
    return std::u16string(text);
  }

  size_t last_non_whitespace_index = trimmed.size() - 1;
  char16_t last_char = text[last_non_whitespace_index];
  if (!IsSentenceEndingPeriod(last_char) ||
      IsEllipsisAt(text, last_non_whitespace_index)) {
    return std::u16string(text);
  }

  if (HasAtMostOneSentence(text, last_non_whitespace_index)) {
    std::u16string result(text.substr(0, last_non_whitespace_index));
    result.append(text.substr(last_non_whitespace_index + 1));
    return result;
  }

  return std::u16string(text);
}

std::u16string FormatTranscription(const std::u16string& text,
                                   std::u16string_view preceding_text) {
  if (text.empty()) {
    return text;
  }

  std::u16string formatted_text = TrimTrailingPeriodIfSingleSentence(text);

  bool prepend_whitespace = WhitespaceNeeded(preceding_text, formatted_text);

  return prepend_whitespace ? (u" " + formatted_text) : formatted_text;
}

}  // namespace dictation
