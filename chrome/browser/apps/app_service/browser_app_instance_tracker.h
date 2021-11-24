// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_TRACKER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_TRACKER_H_

#include <map>
#include <memory>
#include <set>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "content/public/browser/web_contents.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

class Browser;
class Profile;

namespace aura {
class Window;
}

namespace apps {

class BrowserAppInstanceObserver;

// BrowserAppInstanceTracker monitors changes to Browsers, TabStripModels and
// browsers' native window activation to maintain a list of running apps and
// notify its registered observers of any changes:
// - apps running in WebContents (web apps, hosted apps, V1 packaged apps)
// - browser instances (registered with app ID |extension_misc::kChromeAppId|).
class BrowserAppInstanceTracker : public TabStripModelObserver,
                                  public BrowserTabStripTrackerDelegate,
                                  public wm::ActivationChangeObserver,
                                  public AppRegistryCache::Observer,
                                  public BrowserListObserver {
 public:
  BrowserAppInstanceTracker(Profile* profile,
                            AppRegistryCache& app_registry_cache);
  ~BrowserAppInstanceTracker() override;
  BrowserAppInstanceTracker(const BrowserAppInstanceTracker&) = delete;
  BrowserAppInstanceTracker& operator=(const BrowserAppInstanceTracker&) =
      delete;

  // Get app instance running in a |contents|. Returns null if no app is found.
  const BrowserAppInstance* GetAppInstance(
      content::WebContents* contents) const;

  // Get Chrome instance running in |browser|. Returns null if not found.
  const BrowserWindowInstance* GetWindowInstance(Browser* browser) const;

  // Activate the given instance within its tabstrip. If the instance lives in
  // its own window, this will have no effect.
  void ActivateTabInstance(base::UnguessableToken id);

  // Stop all running instances of an app. The app's associated windows/tabs
  // will be closed.
  void StopInstancesOfApp(const std::string& app_id);

  void AddObserver(BrowserAppInstanceObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(BrowserAppInstanceObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserTabStripTrackerDelegate overrides:
  bool ShouldTrackBrowser(Browser* browser) override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override;

 private:
  class WebContentsObserver;
  friend class BrowserAppInstanceRegistry;

  // Called by TabStripModelChanged().
  void OnTabStripModelChangeInsert(Browser* browser,
                                   const TabStripModelChange::Insert& insert,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeRemove(Browser* browser,
                                   const TabStripModelChange::Remove& remove,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeReplace(
      Browser* browser,
      const TabStripModelChange::Replace& replace);
  void OnTabStripModelChangeSelection(Browser* browser,
                                      const TabStripSelectionChange& selection);

  // Called by OnTabStripModelChange* functions.
  void OnBrowserFirstTabAttached(Browser* browser);
  void OnBrowserLastTabDetached(Browser* browser);
  void OnTabCreated(Browser* browser, content::WebContents* contents);
  void OnTabAttached(Browser* browser, content::WebContents* contents);
  void OnTabUpdated(Browser* browser, content::WebContents* contents);
  void OnTabClosing(Browser* browser, content::WebContents* contents);

  // Called by |BrowserAppInstanceTracker::WebContentsObserver|.
  void OnWebContentsUpdated(content::WebContents* contents);

  // Called on browser window changes. Sends update events for all open tabs.
  void OnBrowserWindowUpdated(Browser* browser);

  // Creates an app instance for the app running in |WebContents|. Handles both
  // apps in tabs and windows.
  void CreateAppInstance(std::string app_id,
                         Browser* browser,
                         content::WebContents* contents);

  // Updates the app instance with the new attributes and notifies observers, if
  // it was updated.
  void MaybeUpdateAppInstance(BrowserAppInstance& instance,
                              Browser* browser,
                              content::WebContents* contents);

  // Removes the app instance, if it exists, and notifies observers.
  void RemoveAppInstanceIfExists(content::WebContents* contents);

  // Creates an app instance for a Chrome browser window.
  void CreateWindowInstance(Browser* browser);

  // Updates the browser instance with the new attributes and notifies
  // observers, if it was updated.
  void MaybeUpdateWindowInstance(BrowserWindowInstance& instance,
                                 Browser* browser);

  // Removes the browser instance, if it exists, and notifies observers.
  void RemoveWindowInstanceIfExists(Browser* browser);

  // Virtual to override in tests.
  virtual base::UnguessableToken GenerateId() const;

  bool IsBrowserTracked(Browser* browser) const;
  bool IsActivationClientTracked(wm::ActivationClient* client) const;

  Profile* const profile_;

  std::map<content::WebContents*, std::unique_ptr<WebContentsObserver>>
      webcontents_to_observer_map_;

  // A set of observed browsers: browsers where at least one tab has been added.
  // Events for all other browsers are filtered out.
  std::set<Browser*> tracked_browsers_;

  // A set of observed activation clients for all browser's windows.
  base::ScopedMultiSourceObservation<wm::ActivationClient,
                                     wm::ActivationChangeObserver>
      activation_client_observations_{this};

  BrowserTabStripTracker browser_tab_strip_tracker_;

#if DCHECK_IS_ON()
  // Tabs that are removed from one browser and are getting reinserted into
  // another.
  std::set<content::WebContents*> tabs_in_transit_;
#endif

  // A set of all apps running in either tabs or windows.
  BrowserAppInstanceMap<content::WebContents*, BrowserAppInstance>
      app_instances_;

  // Chrome browser windows.
  BrowserAppInstanceMap<Browser*, BrowserWindowInstance> window_instances_;

  base::ObserverList<BrowserAppInstanceObserver, true>::Unchecked observers_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_BROWSER_APP_INSTANCE_TRACKER_H_
