// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "glic_window_controller.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

class BrowserWindowInterface;

namespace glic {
// Responsible for managing which tab is considered "focused" and for accessing
// its WebContents. This is an implementation detail of GlicKeyedService and
// others should rely on the interface that GlicKeyedService exposes for
// observing state changes.
class GlicFocusedTabManager : public BrowserListObserver,
                              public content::WebContentsObserver {
 public:
  explicit GlicFocusedTabManager(Profile* profile,
                                 GlicWindowController& window_controller);
  ~GlicFocusedTabManager() override;

  GlicFocusedTabManager(const GlicFocusedTabManager&) = delete;
  GlicFocusedTabManager& operator=(const GlicFocusedTabManager&) = delete;

  // Returns the currently focused tab or nullptr if nothing is focused.
  content::WebContents* GetWebContentsForFocusedTab();

  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // Callback for changes to focused tab. The web contents pointer can be null
  // if no tab is in focus.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(const content::WebContents*)>;
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

 private:
  // True if |browser| is valid for Glic focus.
  bool IsValidBrowser(BrowserWindowInterface* browser_interface);

  // True if |web_contents| is allowed to be focused.
  bool IsValidFocusable(content::WebContents* web_contents);

  // Updates focused tab if a new one is computed. Notifies if updated or if
  // |force_notify| is true.
  void MaybeUpdateFocusedTab(bool force_notify = false);

  // Computes the currently focused tab.
  content::WebContents* ComputeFocusedTab();

  // Computes the currently focusable tab for a given browser.
  content::WebContents* ComputeFocusableTabForBrowser(
      BrowserWindowInterface* browser_interface);

  // Calls all registered focused tab changed callbacks.
  void NotifyFocusedTabChanged();

  // List of callbacks to be notified when focused tab changed.
  base::RepeatingCallbackList<void(const content::WebContents*)>
      focused_callback_list_;

  // The profile for which to manage focused tabs.
  raw_ptr<Profile> profile_;

  // The Glic window controller.
  raw_ref<GlicWindowController> window_controller_;

  // The currently focused tab (or nullptr if no tab is focused).
  base::WeakPtr<content::WebContents> focused_web_contents_;

  // Callback subscription for listening to changes to active tab for a browser.
  base::CallbackListSubscription browser_subscription_;

  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Callback subscription for listening to changes to the Glic window
  // activation changes.
  base::CallbackListSubscription window_activation_subscription_;

  // Callback for Glic Window activation changes.
  void OnGlicWindowActivationChanged(bool active);
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_FOCUSED_TAB_MANAGER_H_
