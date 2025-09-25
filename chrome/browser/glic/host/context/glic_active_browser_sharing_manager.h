// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Profile;

namespace glic {

// Sharing manager that tracks with the active browser. When a Chrome window is
// active and its active tab is showing a GlicInstance, this sharing manager
// behaves like the sharing manager for that instance. Otherwise it behaves like
// an empty sharing manager (nothing is or can be shared).
class GlicActiveBrowserSharingManager : public GlicDelegatingSharingManager,
                                        public BrowserListObserver {
 public:
  explicit GlicActiveBrowserSharingManager(Profile* profile);
  ~GlicActiveBrowserSharingManager() override;

  GlicActiveBrowserSharingManager(const GlicActiveBrowserSharingManager&) =
      delete;
  GlicActiveBrowserSharingManager& operator=(
      const GlicActiveBrowserSharingManager&) = delete;

  // BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Callback for changes to the active tab.
  void OnActiveTabChanged(BrowserWindowInterface* browser);

 private:
  // Updates the delegate based on current active browser state.
  void UpdateDelegate();

  // Subscription for active tab changes.
  base::CallbackListSubscription active_tab_subscription_;

  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_ACTIVE_BROWSER_SHARING_MANAGER_H_
