// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_CHECKER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_CHECKER_H_

#include <string>

namespace ash {
namespace input_method {

// Checks if the given UTF-16 text contains some accented letters.
bool HasDiacritics(const std::u16string& text);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_DIACRITICS_CHECKER_H_
