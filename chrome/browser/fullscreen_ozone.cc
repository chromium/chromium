// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

// The Ozone implementation is limited to chrome only: it checks whether any
// existing browser object has a fullscreen window, but does not try to find
// if there are ones belonging to other applications.
bool IsFullScreenMode() {
  return base::ranges::any_of(
      *BrowserList::GetInstance(),
      [](const Browser* browser) { return browser->window()->IsFullscreen(); });
}
