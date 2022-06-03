// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_UTILS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_UTILS_H_

#include <string>

#include "ui/gfx/range/range.h"

// TODO(crbug/1223213): Move these to a sandbox environment.
namespace ash {
namespace input_method {

const int kUndefined = -1;

struct Sentence {
  Sentence();
  Sentence(const gfx::Range& range, const std::u16string& text);
  Sentence(const Sentence& other);
  ~Sentence();

  bool operator==(const Sentence& other) const;
  bool operator!=(const Sentence& other) const;

  // The range of the sentence in the original text.
  gfx::Range original_range;
  std::u16string text;
};

// Find the index of the last sentence end before |pos|, returns |kUndefined| if
// not found.
int FindLastSentenceEnd(const std::u16string& text, int pos);

// Find the index of the first sentence end equal or after |pos|, returns
// |kUndefined| if not found.
int FindNextSentenceEnd(const std::u16string& text, int pos);

// Find the last sentence before cursor position |pos|.
Sentence FindLastSentence(const std::u16string& text, int pos);

// Find the sentence containing the cursor position |pos|.
Sentence FindCurrentSentence(const std::u16string& text, int pos);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_TEXT_UTILS_H_
