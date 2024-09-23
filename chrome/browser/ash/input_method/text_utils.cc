// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/text_utils.h"

// TODO(crbug/1223597) The rules to detect sentence end is not perfect, and we
// may want to use regex to improve readability.
namespace ash {
namespace input_method {
namespace {

const int kMaxSearchRange = 200;
const int kSpecialWordMaxLength = 6;
// The index difference between a sentence end and the next sentence start.
// Setting it to 1 is sufficient for grammar check model, but 2 is better since
// according to current rules, there is always a space or '\n' or '\r' after a
// sentence end.
const int kGapBetweenSentenceEndAndNextStart = 2;

bool IsSentenceEndCharacter(char16_t c) {
  return (c == u'.' || c == u'?' || c == u'!' || c == u'。' || c == u'｡' ||
          c == u'．' || c == u'.' || c == u'？' || c == u'?' || c == u'！' ||
          c == u'!' || c == u'…');
}

bool EndsInSpecialPeriodWord(const std::u16string& text, uint32_t pos) {
  uint32_t idx = pos;
  while (idx <= pos && pos - idx <= kSpecialWordMaxLength &&
         text[idx] != u' ' && text[idx] != u'(') {
    idx--;
  }
  if (idx > pos || pos - idx > kSpecialWordMaxLength) {
    return false;
  }
  std::u16string last_word = text.substr(idx + 1, pos - idx);
  return (last_word == u"c.f." || last_word == u"cf." || last_word == u"e.g." ||
          last_word == u"eg." || last_word == u"i.e." || last_word == u"ie." ||
          last_word == u"Mmes." || last_word == u"Mr." ||
          last_word == u"Mrs." || last_word == u"Ms." ||
          last_word == u"Mses." || last_word == u"Mssrs." ||
          last_word == u"Prof." || last_word == u"n.b." || last_word == u"nb.");
}

bool IsSentenceEndSectionCharacter(char16_t c) {
  return (c == u')' || c == u']' || c == u'}' || c == u'\'' || c == u'\"' ||
          c == u'ʺ' || c == u'˝' || c == u'ˮ' || c == u'＂' || c == u'″' ||
          c == u'”' || c == u'»');
}

bool IsEmoticonEyes(char16_t c) {
  return (c == ':' || c == ';');
}

bool IsEmoticonNose(char16_t c) {
  return (c == u'-' || c == u'^' || c == u'{' || c == u'*');
}

bool IsEmoticonMouth(char16_t c) {
  return (c == u')' || c == u'(' || c == u'\\' || c == u'|' || c == u'/');
}

bool EndsInEmoticon(const std::u16string& text, uint32_t pos) {
  return ((pos >= 1 && IsEmoticonEyes(text[pos - 1]) &&
           IsEmoticonMouth(text[pos])) ||
          (pos >= 2 && IsEmoticonEyes(text[pos - 2]) &&
           IsEmoticonNose(text[pos - 1]) && IsEmoticonMouth(text[pos])));
}

bool IsSentenceEnd(const std::u16string& text, uint32_t pos) {
  if (pos < text.size() - 1 &&
      (text[pos + 1] == '\n' || text[pos + 1] == '\r')) {
    return true;
  }

  // The character after the sentence end must be a space or the end of the
  // text.
  if (pos < 2 || (pos < text.size() - 1 && text[pos + 1] != u' ')) {
    return false;
  }

  if (IsSentenceEndCharacter(text[pos]) &&
      !EndsInSpecialPeriodWord(text, pos)) {
    return true;
  }

  if (IsSentenceEndCharacter(text[pos - 1]) &&
      IsSentenceEndSectionCharacter(text[pos])) {
    return true;
  }

  if (EndsInEmoticon(text, pos)) {
    return true;
  }

  return false;
}

}  // namespace

Sentence::Sentence() {}

Sentence::Sentence(const gfx::Range& original_range, const std::u16string& text)
    : original_range(original_range), text(text) {}

Sentence::Sentence(const Sentence& other) = default;

Sentence::~Sentence() = default;

bool Sentence::operator==(const Sentence& other) const {
  return original_range == other.original_range && text == other.text;
}

bool Sentence::operator!=(const Sentence& other) const {
  return !(*this == other);
}

uint32_t FindLastSentenceEnd(const std::u16string& text, uint32_t pos) {
  if (pos == 0 || pos > text.size()) {
    return kUndefined;
  }

  for (size_t i = pos - 1; i > 0 && pos - i <= kMaxSearchRange; i--) {
    if (IsSentenceEnd(text, i)) {
      return i;
    }
  }
  return kUndefined;
}

uint32_t FindNextSentenceEnd(const std::u16string& text, uint32_t pos) {
  if (pos >= text.size()) {
    return kUndefined;
  }

  for (size_t i = pos; i < text.size() && i - pos <= kMaxSearchRange; i++) {
    if (IsSentenceEnd(text, i)) {
      return i;
    }
  }
  return kUndefined;
}

Sentence FindLastSentence(const std::u16string& text, uint32_t pos) {
  if (pos > text.size()) {
    return Sentence();
  }
  if (pos > 0 &&
      (pos == text.size() || text[pos] == '\n' || text[pos] == '\r')) {
    pos--;
  }
  uint32_t end = FindLastSentenceEnd(text, pos);
  if (end == kUndefined) {
    return Sentence();
  }
  uint32_t start = FindLastSentenceEnd(text, end);
  if (start == kUndefined) {
    start = 0;
  } else {
    start = start + kGapBetweenSentenceEndAndNextStart;
  }
  if (start >= end || end - start > kMaxSearchRange) {
    return Sentence();
  }
  return Sentence(gfx::Range(start, end + 1),
                  text.substr(start, end - start + 1));
}

Sentence FindCurrentSentence(const std::u16string& text, uint32_t pos) {
  if (pos > text.size()) {
    return Sentence();
  }
  if (pos > 0 &&
      (pos == text.size() || text[pos] == '\n' || text[pos] == '\r')) {
    pos--;
  }
  uint32_t start = FindLastSentenceEnd(text, pos);
  if (start == kUndefined) {
    start = 0;
  } else {
    start = start + kGapBetweenSentenceEndAndNextStart;
  }

  uint32_t end = FindNextSentenceEnd(text, pos);
  if (end == kUndefined) {
    end = text.length() - 1;
  }

  if (start >= end || end - start > kMaxSearchRange) {
    return Sentence();
  }

  return Sentence(gfx::Range(start, end + 1),
                  text.substr(start, end - start + 1));
}

}  // namespace input_method
}  // namespace ash
