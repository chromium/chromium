// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_BROWSER_HELPER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_BROWSER_HELPER_LACROS_H_

namespace aura {
class Window;
class WindowTreeHost;
}

namespace policy {

namespace dlp {

// Retrieves the aura::Window for the last active browser.
aura::Window* GetActiveAuraWindow();

// Retrieves the aura::WindowTreeHost for the last active browser.
aura::WindowTreeHost* GetActiveWindowTreeHost();

}  // namespace dlp

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_BROWSER_HELPER_LACROS_H_
