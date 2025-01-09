// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_BORDER_VIEW_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_BORDER_VIEW_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace glic {
// This class is responsible for adding a glow around the WebContentsView which
// hosts the active tab's WebContents.
class GlicBorderViewManager {
 public:
  explicit GlicBorderViewManager(BrowserWindowInterface* browser);
  ~GlicBorderViewManager();

 private:
  // Called when the focused tab changes.
  void OnFocusedTabChanged(const content::WebContents* contents);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // Called when the active tab changes within this browser window.
  void OnActiveTabChanged(BrowserWindowInterface* browser);

  // Updates the BorderView UI effect given the current state of the focused tab
  // and context access indicator flag.
  void UpdateBorderView();

  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<GlicKeyedService> glic_service_;
  base::WeakPtr<const content::WebContents> focused_tab_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription focus_change_subscription_;
  base::CallbackListSubscription indicator_change_subscription_;
  base::CallbackListSubscription active_tab_change_subscription_;
};
}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_BORDER_VIEW_MANAGER_H_
