// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/break_iterator.h"

#include <stdint.h>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "third_party/icu/source/common/unicode/ubrk.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/ustring.h"

namespace base {
namespace i18n {

const size_t npos = static_cast<size_t>(-1);

BreakIterator::BreakIterator(const StringPiece16& str, BreakType break_type)
    : iter_(nullptr),
      string_(str),
      break_type_(break_type),
      prev_(npos),
      pos_(0) {}

BreakIterator::BreakIterator(const StringPiece16& str,
                             const std::u16string& rules)
    : iter_(nullptr),
      string_(str),
      rules_(rules),
      break_type_(RULE_BASED),
      prev_(npos),
      pos_(0) {}

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
  DefaultLocaleBreakIteratorCache()
      : main_status_(U_ZERO_ERROR),
        main_(nullptr),
        main_could_be_leased_(true) {
    main_ = ubrk_open(break_type, nullptr, nullptr, 0, &main_status_);
    if (U_FAILURE(main_status_)) {
      NOTREACHED() << "ubrk_open failed for type " << break_type
                   << " with error " << main_status_;
    }
  }

  virtual ~DefaultLocaleBreakIteratorCache() { ubrk_close(main_); }

  UBreakIterator* Lease(UErrorCode& status) {
    if (U_FAILURE(status)) {
      return nullptr;
    }
    if (U_FAILURE(main_status_)) {
      status = main_status_;
      return nullptr;
    }
    {
      AutoLock scoped_lock(lock_);
      if (main_could_be_leased_) {
        // Just lease the main_ out.
        main_could_be_leased_ = false;
        return main_;
      }
    }
    // The main_ is already leased out to some other places, return a new
    // object instead.
    UBreakIterator* result =
        ubrk_open(break_type, nullptr, nullptr, 0, &status);
    if (U_FAILURE(status)) {
      NOTREACHED() << "ubrk_open failed for type " << break_type
                   << " with error " << status;
    }
    return result;
  }

  void Return(UBreakIterator* item) {
    // If the return item is the main_, just remember we can lease it out
    // next time.
    if (item == main_) {
      AutoLock scoped_lock(lock_);
      main_could_be_leased_ = true;
    } else {
      // Close the item if it is not main_.
      ubrk_close(item);
    }
  }

 private:
  UErrorCode main_status_;
  UBreakIterator* main_;
  bool main_could_be_leased_ GUARDED_BY(lock_);
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

BreakIterator::~BreakIterator() {
  if (iter_) {
    UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
    switch (break_type_) {
      // Free the iter if it is RULE_BASED
      case RULE_BASED:
        ubrk_close(iter);
        break;
      // Otherwise, return the iter to the cache it leased from.`
      case BREAK_CHARACTER:
        char_break_cache.Pointer()->Return(iter);
        break;
      case BREAK_WORD:
        word_break_cache.Pointer()->Return(iter);
        break;
      case BREAK_SENTENCE:
        sentence_break_cache.Pointer()->Return(iter);
        break;
      case BREAK_LINE:
      case BREAK_NEWLINE:
        line_break_cache.Pointer()->Return(iter);
        break;
      default:
        NOTREACHED() << "invalid break_type_";
        break;
    }
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
      iter_ =
          ubrk_openRules(rules_.c_str(), static_cast<int32_t>(rules_.length()),
                         nullptr, 0, &parse_error, &status);
      if (U_FAILURE(status)) {
        NOTREACHED() << "ubrk_openRules failed to parse rule string at line "
                     << parse_error.line << ", offset " << parse_error.offset;
      }
      break;
    default:
      NOTREACHED() << "invalid break_type_";
      return false;
  }

  if (U_FAILURE(status) || iter_ == nullptr) {
    return false;
  }

  if (string_.data() != nullptr) {
    ubrk_setText(static_cast<UBreakIterator*>(iter_), string_.data(),
                 static_cast<int32_t>(string_.size()), &status);
    if (U_FAILURE(status)) {
      return false;
    }
  }

  // Move the iterator to the beginning of the string.
  ubrk_first(static_cast<UBreakIterator*>(iter_));
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
      pos = ubrk_next(static_cast<UBreakIterator*>(iter_));
      if (pos == UBRK_DONE) {
        pos_ = npos;
        return false;
      }
      pos_ = static_cast<size_t>(pos);
      return true;
    case BREAK_NEWLINE:
      do {
        pos = ubrk_next(static_cast<UBreakIterator*>(iter_));
        if (pos == UBRK_DONE)
          break;
        pos_ = static_cast<size_t>(pos);
        status = ubrk_getRuleStatus(static_cast<UBreakIterator*>(iter_));
      } while (status >= UBRK_LINE_SOFT && status < UBRK_LINE_SOFT_LIMIT);
      if (pos == UBRK_DONE && prev_ == pos_) {
        pos_ = npos;
        return false;
      }
      return true;
    default:
      NOTREACHED() << "invalid break_type_";
      return false;
  }
}

bool BreakIterator::SetText(const char16_t* text, const size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  ubrk_setText(static_cast<UBreakIterator*>(iter_), text, length, &status);
  pos_ = 0;  // implicit when ubrk_setText is done
  prev_ = npos;
  if (U_FAILURE(status)) {
    NOTREACHED() << "ubrk_setText failed";
    return false;
  }
  string_ = StringPiece16(text, length);
  return true;
}

bool BreakIterator::IsWord() const {
  return GetWordBreakStatus() == IS_WORD_BREAK;
}

BreakIterator::WordBreakStatus BreakIterator::GetWordBreakStatus() const {
  int32_t status = ubrk_getRuleStatus(static_cast<UBreakIterator*>(iter_));
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

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  UBool boundary = ubrk_isBoundary(iter, static_cast<int32_t>(position));
  int32_t status = ubrk_getRuleStatus(iter);
  return (!!boundary && status != UBRK_WORD_NONE);
}

bool BreakIterator::IsStartOfWord(size_t position) const {
  if (break_type_ != BREAK_WORD && break_type_ != RULE_BASED)
    return false;

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  UBool boundary = ubrk_isBoundary(iter, static_cast<int32_t>(position));
  ubrk_next(iter);
  int32_t next_status = ubrk_getRuleStatus(iter);
  return (!!boundary && next_status != UBRK_WORD_NONE);
}

bool BreakIterator::IsSentenceBoundary(size_t position) const {
  if (break_type_ != BREAK_SENTENCE && break_type_ != RULE_BASED)
    return false;

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  return !!ubrk_isBoundary(iter, static_cast<int32_t>(position));
}

bool BreakIterator::IsGraphemeBoundary(size_t position) const {
  if (break_type_ != BREAK_CHARACTER)
    return false;

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  return !!ubrk_isBoundary(iter, static_cast<int32_t>(position));
}

std::u16string BreakIterator::GetString() const {
  return std::u16string(GetStringPiece());
}

StringPiece16 BreakIterator::GetStringPiece() const {
  DCHECK(prev_ != npos && pos_ != npos);
  return string_.substr(prev_, pos_ - prev_);
}

}  // namespace i18n
}  // namespace base
