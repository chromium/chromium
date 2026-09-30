// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_MANAGED_TAB_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_MANAGED_TAB_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"
#include "url/origin.h"

class BrowserWindowInterface;
class GURL;
class Profile;

namespace glic {

// Tracks a single browser tab that Chrome opens on behalf of the GEiC web
// client (e.g. for sign-in or a connector OAuth flow).
//
// Opening remembers which tab was active so that focus can be restored on
// close. Closing only ever affects the tab this object opened, so the web
// client is never given a capability to close arbitrary user tabs.
//
// The tab is considered to still belong to the flow only while its last
// committed URL is on an expected origin for its purpose (e.g. the redirector
// or guest origin for connector OAuth). If the user navigates it elsewhere, it
// is treated as theirs: it is no longer reused or closed.
//
// URL validation for `Open()` is the caller's responsibility.
class GeicManagedTab {
 public:
  // What to do when `Open()` is called while the managed tab is still open.
  enum class ReuseMode {
    // Activate the existing tab without navigating it.
    kActivate,
    // Navigate the existing tab to the new URL, then activate it.
    kNavigateAndActivate,
  };

  enum class CloseOutcome {
    // The active managed tab was closed and focus was restored.
    kClosedActive,
    // The managed tab was closed while inactive; focus was left undisturbed.
    kClosedInactive,
    // A tab was opened, but was already closed (e.g. by the user).
    kAlreadyClosed,
    // No tab was opened since construction or since the last `Close()`.
    kNotOpened,
    // A tab was opened, but the user navigated it away from the flow, so it
    // was left open and is no longer tracked.
    kNavigatedAway,
  };

  GeicManagedTab(Profile* profile,
                 mojom::AuthTabPurpose purpose,
                 url::Origin gaia_origin,
                 url::Origin guest_origin);
  GeicManagedTab(const GeicManagedTab&) = delete;
  GeicManagedTab& operator=(const GeicManagedTab&) = delete;
  ~GeicManagedTab();

  // Opens `url` in a new foreground tab in the profile's last active normal
  // browser window, or reuses the managed tab per `reuse_mode` if it still
  // belongs to the flow. Returns whether a tab is now open and active.
  bool Open(const GURL& url, ReuseMode reuse_mode);

  // Closes the managed tab if it is still open and still belongs to the flow.
  // Focus is restored to the tab that was active before it was opened only if
  // the managed tab was the active tab in its window when closed.
  CloseOutcome Close();

  tabs::TabInterface* GetTabForTesting() const { return tab_.Get(); }

  // Returns whether `url` is on an expected origin for this managed tab's
  // purpose.
  bool IsExpectedOrigin(const GURL& url) const;

 private:
  BrowserWindowInterface* GetLastActiveBrowserWindowForProfile() const;

  // Returns whether `tab` is still on a URL that belongs to the flow. A tab
  // whose initial navigation has not committed yet is considered owned.
  bool IsStillOwned(tabs::TabInterface* tab) const;

  raw_ptr<Profile> profile_ = nullptr;
  const mojom::AuthTabPurpose purpose_;
  const url::Origin gaia_origin_;
  const url::Origin guest_origin_;
  tabs::TabHandle tab_;
  tabs::TabHandle tab_active_before_open_;
  // Allows `Close()` to distinguish `kAlreadyClosed` (user closed the tab)
  // from `kNotOpened` when `tab_` is null.
  bool has_opened_tab_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_MANAGED_TAB_H_
