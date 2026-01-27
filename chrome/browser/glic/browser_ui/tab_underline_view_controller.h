// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace glic {
class TabUnderlineView;

class TabUnderlineViewController {
 public:
  virtual ~TabUnderlineViewController() = default;

  // Initialization. Starts observing the state of the browser.
  virtual void Initialize(TabUnderlineView* underline_view,
                          BrowserWindowInterface* browser_window_interface) = 0;

  // Called when the owning TabUnderlineView is added to its widget hierarchy.
  // Handles any initialization based on underline state changes that occurred
  // before construction, for example during tabstrip attachment.
  virtual void OnViewAddedToWidget() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_CONTROLLER_H_
