// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_
#define CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_

#include <string>

namespace chrome {

enum class CleanTextOptions {
  kEllipses = 1 << 0,
  kMnemonic = 1 << 1,
  kAll = kEllipses | kMnemonic,
};

// Strips accelerators (e.g., "&") and/or ellipsis (e.g., "..." or "\u2026")
// from a string based on `options`, suitable for text and tooltips in
// ActionItems.
std::u16string GetCleanText(std::u16string string,
                            CleanTextOptions options = CleanTextOptions::kAll);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_
