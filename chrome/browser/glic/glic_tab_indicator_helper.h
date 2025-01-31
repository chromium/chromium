// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

namespace content {
class WebContents;
}

namespace glic {

// Tracks the glic focus and ensures that tab indicators are updated when focus
// changes.
class GlicTabIndicatorHelper {
 public:
  explicit GlicTabIndicatorHelper(tabs::TabInterface* tab);
  ~GlicTabIndicatorHelper();

 private:
  // Updates the given tab if it is in the current tabstrip.
  void MaybeUpdateTab(const content::WebContents* contents);

  // Called when the focused tab changes.
  void OnFocusedTabChanged(const content::WebContents* contents);

  // Called when the client changes the context access indicator status.
  void OnIndicatorStatusChanged(bool enabled);

  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  raw_ptr<tabs::TabInterface> tab_;
  bool context_access_indicator_enabled_ = false;
  bool is_detached_ = false;
  base::WeakPtr<const content::WebContents> last_focused_tab_;
  base::CallbackListSubscription focus_change_subscription_;
  base::CallbackListSubscription indicator_change_subscription_;
  base::CallbackListSubscription will_detach_subscription_;
  base::CallbackListSubscription did_insert_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_
