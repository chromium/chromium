// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_

namespace headless {

// Returns positive if Chrome headless mode is in effect. In this mode Chrome is
// running without any visible UI.
bool IsHeadlessMode();

// Returns positive if headless mode can access any URL whose scheme is
// chrome://.
bool IsChromeSchemeUrlAllowed();

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_UTIL_H_
