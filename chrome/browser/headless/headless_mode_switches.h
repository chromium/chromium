// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_

namespace switches {

// Allows headless mode to access any URL whose scheme is chrome://.
inline constexpr char kAllowChromeSchemeUrl[] = "allow-chrome-scheme-url";

}  // namespace switches

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_
