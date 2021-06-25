// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXT_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXT_UTILS_H_

#include <string>

// TODO(crbug/1223213): Move these to a sandbox environment.
namespace chromeos {
namespace text_utils {

const int kUndefined = -1;

// Find the index of the last sentence end before |pos|, returns |kUndefined| if
// not found.
int FindLastSentenceEnd(const std::u16string& text, int pos);

// Find the index of the first sentence end equal or after |pos|, returns
// |kUndefined| if not found.
int FindNextSentenceEnd(const std::u16string& text, int pos);

}  // namespace text_utils
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXT_UTILS_H_
