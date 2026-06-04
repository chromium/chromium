// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_
#define CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_

#include <string>

namespace chrome {

// Strips accelerators (e.g., "&") and ellipsis (e.g., "..." or "\u2026")
// from a string, suitable for text and tooltips in ActionItems.
std::u16string GetCleanTitleAndTooltipText(std::u16string string);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACTIONS_ACTIONS_UTIL_H_
