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

aura::Window* GetActiveAuraWindow() {
  BrowserList* browser_list = BrowserList::GetInstance();
  DCHECK(browser_list);

  // Find the active tab in the visible focused or topmost browser.
  for (auto browser_iterator =
           browser_list->begin_browsers_ordered_by_activation();
       browser_iterator != browser_list->end_browsers_ordered_by_activation();
       ++browser_iterator) {
    const Browser* browser = *browser_iterator;
    DCHECK(browser);

    const BrowserWindow* window = browser->window();
    DCHECK(window);

    // We only need the visible focused or topmost browser.
    if (window->GetNativeWindow()->IsVisible())
      return window->GetNativeWindow();
  }

  return nullptr;
}

aura::WindowTreeHost* GetActiveWindowTreeHost() {
  aura::Window* active_window = GetActiveAuraWindow();

  if (active_window)
    return active_window->GetHost();

  return nullptr;
}

}  // namespace dlp

}  // namespace policy
