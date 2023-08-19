// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_browser_helper_lacros.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window_tree_host.h"

namespace policy {

namespace dlp {

// Retrieves the aura::Window for the last active browser. Returns nullptr if no
// browser window is currently visible.
aura::Window* GetActiveAuraWindow() {
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  auto* browser = browser_list->GetLastActive();
  if (browser && browser->window()) {
    return browser->window()->GetNativeWindow();
  }

  return nullptr;
}

aura::WindowTreeHost* GetActiveWindowTreeHost() {
  aura::Window* active_window = GetActiveAuraWindow();

  if (active_window) {
    return active_window->GetRootWindow()->GetHost();
  }

  return nullptr;
}

}  // namespace dlp

}  // namespace policy
