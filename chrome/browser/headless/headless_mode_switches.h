// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_

namespace switches {

// Allows headless mode to access any URL whose scheme is chrome://.
inline constexpr char kAllowChromeSchemeUrl[] = "allow-chrome-scheme-url";

// Enable hardware GPU support.
// Headless uses swiftshader by default for consistency across headless
// environments. This flag just turns forcing of swiftshader off and lets
// us revert to regular driver selection logic. Alternatively, specific
// drivers may be forced with --use-gl or --use-angle. Nethier approach
// guarantees that hardware GPU support will be enabled, as this is still
// conditional on headless having access to X display etc.
inline constexpr char kEnableGPU[] = "enable-gpu";

}  // namespace switches

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_SWITCHES_H_
