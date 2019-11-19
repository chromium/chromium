// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_TERM_BREAK_ITERATOR_H_
#define ASH_PUBLIC_CPP_APP_LIST_TERM_BREAK_ITERATOR_H_

#include <stddef.h>

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace base {
namespace i18n {
class UTF16CharIterator;
}
}  // namespace base

namespace ash {

// TermBreakIterator breaks terms out of a word. Terms are broken on
// camel case boundaries and alpha/number boundaries. Numbers are defined
// as [0-9\.,]+.
//  e.g.
//   CamelCase -> Camel, Case
//   Python2.7 -> Python, 2.7
class ASH_PUBLIC_EXPORT TermBreakIterator {
 public:
  // Note that |word| must out live this iterator.
  explicit TermBreakIterator(const base::string16& word);
  ~TermBreakIterator();

  // Advance to the next term. Returns false if at the end of the word.
  bool Advance();

  // Returns the current term, which is the substr of |word_| in range
  // [prev_, pos_).
  const base::string16 GetCurrentTerm() const;

  size_t prev() const { return prev_; }
  size_t pos() const { return pos_; }

  static const size_t npos = static_cast<size_t>(-1);

 private:
  enum State {
    STATE_START,   // Initial state
    STATE_NUMBER,  // Current char is a number [0-9\.,].
    STATE_UPPER,   // Current char is upper case.
    STATE_LOWER,   // Current char is lower case.
    STATE_CHAR,    // Current char has no case, e.g. a cjk char.
    STATE_LAST,
  };

  // Returns new state for given |ch|.
  State GetNewState(base::char16 ch);

  const base::string16& word_;
  size_t prev_;
  size_t pos_;

  std::unique_ptr<base::i18n::UTF16CharIterator> iter_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(TermBreakIterator);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_TERM_BREAK_ITERATOR_H_
