// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/break_iterator.h"

#include <stdint.h>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "third_party/icu/source/common/unicode/ubrk.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/ustring.h"

namespace base {
namespace i18n {

namespace {

// We found the usage pattern of break iterator is to create, use and destroy.
// The following cache support multiple break iterator in the same thread and
// also optimize to not create break iterator many time. For each kind of break
// iterator (character, word, line and sentence, but NOT rule), we keep one of
// them in the main_ and lease it out. If some other code request a lease
// before |main_| is returned, we create a new instance of the iterator.
// This will keep at most 4 break iterators (one for each kind) unreleased until
// the program destruction time.
template <UBreakIteratorType break_type>
class DefaultLocaleBreakIteratorCache {
 public:
  DefaultLocaleBreakIteratorCache() {
    main_ = UBreakIteratorPtr(
        ubrk_open(break_type, nullptr, nullptr, 0, &main_status_));
    if (U_FAILURE(main_status_)) {
      NOTREACHED() << "ubrk_open failed for type " << break_type
                   << " with error " << main_status_;
    }
  }
  UBreakIteratorPtr Lease(UErrorCode& status) {
    if (U_FAILURE(status)) {
      return nullptr;
    }
    if (U_FAILURE(main_status_)) {
      status = main_status_;
      return nullptr;
    }
    {
      AutoLock scoped_lock(lock_);
      if (main_) {
        return std::move(main_);
      }
    }

    // The main_ is already leased out to some other places, return a new
    // object instead.
    UBreakIteratorPtr result(
        ubrk_open(break_type, nullptr, nullptr, 0, &status));
    if (U_FAILURE(status)) {
      NOTREACHED() << "ubrk_open failed for type " << break_type
                   << " with error " << status;
    }
    return result;
  }

  void Return(UBreakIteratorPtr item) {
    AutoLock scoped_lock(lock_);
    if (!main_) {
      main_ = std::move(item);
    }
  }

 private:
  UErrorCode main_status_ = U_ZERO_ERROR;
  UBreakIteratorPtr main_ GUARDED_BY(lock_);
  Lock lock_;
};

static LazyInstance<DefaultLocaleBreakIteratorCache<UBRK_CHARACTER>>::Leaky
    char_break_cache = LAZY_INSTANCE_INITIALIZER;
static LazyInstance<DefaultLocaleBreakIteratorCache<UBRK_WORD>>::Leaky
    word_break_cache = LAZY_INSTANCE_INITIALIZER;
static LazyInstance<DefaultLocaleBreakIteratorCache<UBRK_SENTENCE>>::Leaky
    sentence_break_cache = LAZY_INSTANCE_INITIALIZER;
static LazyInstance<DefaultLocaleBreakIteratorCache<UBRK_LINE>>::Leaky
    line_break_cache = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void UBreakIteratorDeleter::operator()(UBreakIterator* ptr) {
  if (ptr) {
    ubrk_close(ptr);
  }
}

BreakIterator::BreakIterator(std::u16string_view str, BreakType break_type)
    : string_(str), break_type_(break_type) {}

BreakIterator::BreakIterator(std::u16string_view str,
                             const std::u16string& rules)
    : string_(str), rules_(rules), break_type_(RULE_BASED) {}

BreakIterator::~BreakIterator() {
  switch (break_type_) {
    case RULE_BASED:
      return;
    case BREAK_CHARACTER:
      char_break_cache.Pointer()->Return(std::move(iter_));
      return;
    case BREAK_WORD:
      word_break_cache.Pointer()->Return(std::move(iter_));
      return;
    case BREAK_SENTENCE:
      sentence_break_cache.Pointer()->Return(std::move(iter_));
      return;
    case BREAK_LINE:
    case BREAK_NEWLINE:
      line_break_cache.Pointer()->Return(std::move(iter_));
      return;
  }
}

bool BreakIterator::Init() {
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;
  switch (break_type_) {
    case BREAK_CHARACTER:
      iter_ = char_break_cache.Pointer()->Lease(status);
      break;
    case BREAK_WORD:
      iter_ = word_break_cache.Pointer()->Lease(status);
      break;
    case BREAK_SENTENCE:
      iter_ = sentence_break_cache.Pointer()->Lease(status);
      break;
    case BREAK_LINE:
    case BREAK_NEWLINE:
      iter_ = line_break_cache.Pointer()->Lease(status);
      break;
    case RULE_BASED:
      iter_ = UBreakIteratorPtr(
          ubrk_openRules(rules_.c_str(), static_cast<int32_t>(rules_.length()),
                         nullptr, 0, &parse_error, &status));
      if (U_FAILURE(status)) {
        NOTREACHED() << "ubrk_openRules failed to parse rule string at line "
                     << parse_error.line << ", offset " << parse_error.offset;
      }
      break;
  }

  if (U_FAILURE(status) || iter_ == nullptr) {
    return false;
  }

  if (string_.data() != nullptr) {
    ubrk_setText(iter_.get(), string_.data(),
                 static_cast<int32_t>(string_.size()), &status);
    if (U_FAILURE(status)) {
      return false;
    }
  }

  // Move the iterator to the beginning of the string.
  ubrk_first(iter_.get());
  return true;
}

bool BreakIterator::Advance() {
  int32_t pos;
  int32_t status;
  prev_ = pos_;
  switch (break_type_) {
    case BREAK_CHARACTER:
    case BREAK_WORD:
    case BREAK_LINE:
    case BREAK_SENTENCE:
    case RULE_BASED:
      pos = ubrk_next(iter_.get());
      if (pos == UBRK_DONE) {
        pos_ = npos;
        return false;
      }
      pos_ = static_cast<size_t>(pos);
      return true;
    case BREAK_NEWLINE:
      do {
        pos = ubrk_next(iter_.get());
        if (pos == UBRK_DONE)
          break;
        pos_ = static_cast<size_t>(pos);
        status = ubrk_getRuleStatus(iter_.get());
      } while (status >= UBRK_LINE_SOFT && status < UBRK_LINE_SOFT_LIMIT);
      if (pos == UBRK_DONE && prev_ == pos_) {
        pos_ = npos;
        return false;
      }
      return true;
  }
}

bool BreakIterator::SetText(std::u16string_view text) {
  UErrorCode status = U_ZERO_ERROR;
  ubrk_setText(iter_.get(), text.data(), text.length(), &status);
  pos_ = 0;  // implicit when ubrk_setText is done
  prev_ = npos;
  if (U_FAILURE(status)) {
    NOTREACHED() << "ubrk_setText failed";
  }
  string_ = text;
  return true;
}

bool BreakIterator::IsWord() const {
  return GetWordBreakStatus() == IS_WORD_BREAK;
}

BreakIterator::WordBreakStatus BreakIterator::GetWordBreakStatus() const {
  int32_t status = ubrk_getRuleStatus(iter_.get());
  if (break_type_ != BREAK_WORD && break_type_ != RULE_BASED)
    return IS_LINE_OR_CHAR_BREAK;
  // In ICU 60, trying to advance past the end of the text does not change
  // |status| so that |pos_| has to be checked as well as |status|.
  // See http://bugs.icu-project.org/trac/ticket/13447 .
  return (status == UBRK_WORD_NONE || pos_ == npos) ? IS_SKIPPABLE_WORD
                                                    : IS_WORD_BREAK;
}

bool BreakIterator::IsEndOfWord(size_t position) const {
  if (break_type_ != BREAK_WORD && break_type_ != RULE_BASED)
    return false;

  UBool boundary = ubrk_isBoundary(iter_.get(), static_cast<int32_t>(position));
  int32_t status = ubrk_getRuleStatus(iter_.get());
  return (!!boundary && status != UBRK_WORD_NONE);
}

bool BreakIterator::IsStartOfWord(size_t position) const {
  if (break_type_ != BREAK_WORD && break_type_ != RULE_BASED)
    return false;

  UBool boundary = ubrk_isBoundary(iter_.get(), static_cast<int32_t>(position));
  ubrk_next(iter_.get());
  int32_t next_status = ubrk_getRuleStatus(iter_.get());
  return (!!boundary && next_status != UBRK_WORD_NONE);
}

bool BreakIterator::IsSentenceBoundary(size_t position) const {
  if (break_type_ != BREAK_SENTENCE && break_type_ != RULE_BASED)
    return false;

  return !!ubrk_isBoundary(iter_.get(), static_cast<int32_t>(position));
}

bool BreakIterator::IsGraphemeBoundary(size_t position) const {
  if (break_type_ != BREAK_CHARACTER)
    return false;

  return !!ubrk_isBoundary(iter_.get(), static_cast<int32_t>(position));
}

std::u16string BreakIterator::GetString() const {
  return std::u16string(GetStringView());
}

std::u16string_view BreakIterator::GetStringView() const {
  DCHECK(prev_ != npos && pos_ != npos);
  return string_.substr(prev_, pos_ - prev_);
}

}  // namespace i18n
}  // namespace base
