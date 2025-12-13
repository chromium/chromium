// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_

#include "base/callback_list.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace glic {

// True if the immutable attributes of `browser` are valid for Glic focus.
// or pinning. Invalid browsers are never observed.
bool IsBrowserValidForSharingInProfile(
    BrowserWindowInterface* browser_interface,
    Profile* profile);

// Returns true if `web_contents` can be shared, given its current state.
// This becomes invalid when the committed URL changes.
// Sharing may still fail for other reasons.
bool IsTabValidForSharing(content::WebContents* web_contents);

// Returns an empty pin event.
GlicPinEvent GetEmptyPinEvent();

// Returns an empty pinned tab usage.
GlicPinnedTabUsage GetEmptyPinnedTabUsage();

// Returns an empty unpin event.
GlicUnpinEvent GetEmptyUnpinEvent();

// Shared util for monitoring changes to "active tab" for a given profile.
class GlicActiveTabForProfileTracker : public BrowserListObserver {
 public:
  explicit GlicActiveTabForProfileTracker(Profile* profile);
  ~GlicActiveTabForProfileTracker() override;
  GlicActiveTabForProfileTracker(const GlicActiveTabForProfileTracker&) =
      delete;
  GlicActiveTabForProfileTracker& operator=(
      const GlicActiveTabForProfileTracker&) = delete;

  // Subscribe to changes to active tab. Returns null when there is no active
  // browser or when the active browser is not for the same profile.
  base::CallbackListSubscription AddActiveTabChangedCallback(
      base::RepeatingCallback<void(tabs::TabInterface* tab)> callback);

  // Get the last notified active tab.
  tabs::TabInterface* GetActiveTab() const;

 private:
  // BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // Callback for changes to the active tab.
  void OnActiveTabChanged(BrowserWindowInterface* browser);

  // Pulls the active tab and notifies if changed.
  void UpdateActiveTab();

  // Notifies subscribers when active tab has changed.
  void NotifyActiveTabChanged(tabs::TabInterface* active_tab);

  // Updates the active tab subscription (if any) for the given browser.
  void UpdateActiveTabSubscription(BrowserWindowInterface* browser);

  // True if the browser is active and for the same profile.
  bool IsBrowserActiveForProfile(BrowserWindowInterface* browser);

  // The last tab we notified (used for de-duping).
  base::WeakPtr<tabs::TabInterface> last_notified_tab_;

  // Subscription list to notify of active tab changes.
  base::RepeatingCallbackList<void(tabs::TabInterface* tab)>
      active_tab_changed_callback_list_;

  // Subscription for listening to browser-specific active tab changes.
  base::CallbackListSubscription active_tab_subscription_;

  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_UTILS_H_
