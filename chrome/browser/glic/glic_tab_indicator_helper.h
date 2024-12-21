// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace glic {

// Tracks the glic focus and ensures that tab indicators are updated when focus
// changes.
class GlicTabIndicatorHelper {
 public:
  explicit GlicTabIndicatorHelper(BrowserWindowInterface* browser);
  ~GlicTabIndicatorHelper();

 private:
  // Sets the last focused tab to `contents`.
  void SetLastFocusedTab(const content::WebContents* contents);

  // Updates the given tab if it is in the current tabstrip.
  void MaybeUpdateTab(const content::WebContents* contents);

  // Called when the focused tab changes.
  void OnFocusedTabChanged(const content::WebContents* contents);

  const raw_ref<BrowserWindowInterface> browser_;
  base::WeakPtr<const content::WebContents> last_focused_tab_;
  base::CallbackListSubscription change_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_INDICATOR_HELPER_H_
