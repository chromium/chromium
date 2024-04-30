// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_TRACKER_H_
#define CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_TRACKER_H_

#include <map>
#include <memory>
#include <set>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
// - browser instances (registered with app ID |app_constants::kChromeAppId|).
class BrowserAppInstanceTracker : public TabStripModelObserver,
                                  public BrowserTabStripTrackerDelegate,
                                  public AppRegistryCache::Observer,
                                  public BrowserListObserver {
 public:
  BrowserAppInstanceTracker(Profile* profile,
                            AppRegistryCache& app_registry_cache);
  ~BrowserAppInstanceTracker() override;
  BrowserAppInstanceTracker(const BrowserAppInstanceTracker&) = delete;
  BrowserAppInstanceTracker& operator=(const BrowserAppInstanceTracker&) =
      delete;

  // Get an app instance associated with |contents|. It will return either an
  // app instance associated with this tab, or an app instance associated with a
  // browser containing this tab (for app windows and tabs in tabbed app
  // windows). Returns null if no app is found.
  const BrowserAppInstance* GetAppInstance(
      content::WebContents* contents) const;

  // Get a window app instance associated with |browser|. Returns null if no app
  // is found.
  const BrowserAppInstance* GetAppInstance(Browser* browser) const;

  // Get Chrome instance running in |browser|. Returns null if not found.
  const BrowserWindowInstance* GetBrowserWindowInstance(Browser* browser) const;

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

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override;

  // Remove `browser` from internal tracking.
  void RemoveBrowserForTesting(Browser* browser);

 private:
  class WebContentsObserver;
  friend class BrowserAppInstanceRegistry;
  friend class BrowserAppInstanceTrackerLacros;

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
  virtual void OnBrowserFirstTabAttached(Browser* browser);
  virtual void OnBrowserLastTabDetached(Browser* browser);
  void OnTabCreated(Browser* browser, content::WebContents* contents);
  void OnTabAttached(Browser* browser, content::WebContents* contents);
  void OnTabUpdated(Browser* browser, content::WebContents* contents);
  void OnTabClosing(Browser* browser, content::WebContents* contents);

  // Called by |BrowserAppInstanceTracker::WebContentsObserver|.
  void OnWebContentsUpdated(content::WebContents* contents);

  // App tab instance lifecycle

  // Creates an app tab instance for the app running in |contents|.
  void CreateAppTabInstance(std::string app_id,
                            Browser* browser,
                            content::WebContents* contents);
  // Updates the app instance with the new attributes and notifies observers, if
  // it was updated.
  void MaybeUpdateAppTabInstance(BrowserAppInstance& instance,
                                 Browser* browser,
                                 content::WebContents* contents);
  // Removes the app tab instance, if it exists, and notifies observers.
  void RemoveAppTabInstanceIfExists(content::WebContents* contents);

  // App window instance lifecycle

  // Creates an app window instance for the app running in |browser|.
  void CreateAppWindowInstance(std::string app_id, Browser* browser);
  // Updates the app instance with the new attributes and notifies observers, if
  // it was updated.
  void MaybeUpdateAppWindowInstance(BrowserAppInstance& instance,
                                    Browser* browser);
  // Removes the app window instance, if it exists, and notifies observers.
  void RemoveAppWindowInstanceIfExists(Browser* browser);

  // Browser window instance lifecycle

  // Creates an app instance for a Chrome browser window.
  void CreateBrowserWindowInstance(Browser* browser);
  // Removes the browser instance, if it exists, and notifies observers.
  void RemoveBrowserWindowInstanceIfExists(Browser* browser);

  // Virtual to override in tests.
  virtual base::UnguessableToken GenerateId() const;

  bool IsBrowserTracked(Browser* browser) const;

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  std::map<content::WebContents*, std::unique_ptr<WebContentsObserver>>
      webcontents_to_observer_map_;

  // A set of observed browsers: browsers where at least one tab has been added.
  // Events for all other browsers are filtered out.
  std::set<raw_ptr<Browser, SetExperimental>> tracked_browsers_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

#if DCHECK_IS_ON()
  // Tabs that are removed from one browser and are getting reinserted into
  // another.
  std::set<raw_ptr<content::WebContents, SetExperimental>> tabs_in_transit_;
#endif
  // App instances running in tabs.
  BrowserAppInstanceMap<content::WebContents*, BrowserAppInstance>
      app_tab_instances_;

  // App instances running in windows.
  BrowserAppInstanceMap<Browser*, BrowserAppInstance> app_window_instances_;

  // Chrome browser windows.
  BrowserAppInstanceMap<Browser*, BrowserWindowInstance> window_instances_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ObserverList<BrowserAppInstanceObserver, true>::Unchecked observers_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(b/332628771): Remove this class 2 mile stones from this patch.
// Now that activation is observed by Ash even for Lacros windows,
// |BrowserAppInstanceTracker| no longer needs to observe activation changes.
// However to support older Ash, |BrowserAppInstanceTrackerLacros| adds
// |ActivationChangeObserver| functionality to |BrowserAppInstanceTracker| and
// notifies Ash.
class BrowserAppInstanceTrackerLacros : public BrowserAppInstanceTracker,
                                        public wm::ActivationChangeObserver {
 public:
  BrowserAppInstanceTrackerLacros(Profile* profile,
                                  AppRegistryCache& app_registry_cache);
  ~BrowserAppInstanceTrackerLacros() override;
  BrowserAppInstanceTrackerLacros(const BrowserAppInstanceTrackerLacros&) =
      delete;
  BrowserAppInstanceTrackerLacros& operator=(
      const BrowserAppInstanceTrackerLacros&) = delete;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 private:
  // Updates the browser instance with the new attributes and notifies
  // observers, if it was updated.
  void MaybeUpdateBrowserWindowInstance(BrowserWindowInstance& instance,
                                        Browser* browser);
  // Called on browser window changes. Sends update events for all open tabs.
  void OnBrowserWindowUpdated(Browser* browser);

  bool IsActivationClientTracked(wm::ActivationClient* client) const;

  // In addition to calling
  // |BrowserAppInstanceTracker::OnBrowserFirstTabAttached| starts observing
  // |ActivationClient| corresponding to |browser|.
  void OnBrowserFirstTabAttached(Browser* browser) override;
  // In addition to calling
  // |BrowserAppInstanceTracker::OnBrowserLastTabDetached| stops observing
  // |ActivationClient| corresponding to |browser|.
  void OnBrowserLastTabDetached(Browser* browser) override;

  // A set of observed activation clients for all browser's windows.
  base::ScopedMultiSourceObservation<wm::ActivationClient,
                                     wm::ActivationChangeObserver>
      activation_client_observations_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_BROWSER_INSTANCE_BROWSER_APP_INSTANCE_TRACKER_H_
