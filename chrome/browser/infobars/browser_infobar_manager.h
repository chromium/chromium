// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserProcess;
class BrowserWindowInterface;

namespace tabs {
class TabInterface;
}

namespace infobars {

class InfoBar;

// BrowserInfoBarManager handles the lifecycle and scope management of modern
// InfoBars. Registered as a GlobalFeature.
class BrowserInfoBarManager : public BrowserCollectionObserver,
                              public infobars::InfoBarManager::Observer {
 public:
  explicit BrowserInfoBarManager(BrowserProcess* browser_process);
  BrowserInfoBarManager(const BrowserInfoBarManager&) = delete;
  BrowserInfoBarManager& operator=(const BrowserInfoBarManager&) = delete;
  ~BrowserInfoBarManager() override;

  DECLARE_USER_DATA(BrowserInfoBarManager);

  static BrowserInfoBarManager* From(BrowserProcess* browser_process);

  // Registers a spec. Each identifier can only be registered once.
  void Register(InfoBarSpec spec);

  // Returns true if an InfoBarSpec with the given identifier is registered.
  bool IsRegistered(
      infobars::InfoBarDelegate::InfoBarIdentifier identifier) const;

  // Shows the infobar in `tab`. Returns the added infobar, or null if none
  // was added. The infobar can be closed (and destroyed) at any time; a
  // caller that retains the pointer must hold it via InfoBar::AsWeakPtr().
  infobars::InfoBar* Show(
      tabs::TabInterface* tab,
      infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // As above, with per-show overrides.
  infobars::InfoBar* Show(
      tabs::TabInterface* tab,
      infobars::InfoBarDelegate::InfoBarIdentifier identifier,
      InfoBarShowParams params);

  // Shows the infobar in every browser. Returns true if any instance was
  // added.
  bool ShowGlobally(infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // Hides the infobar in `web_contents`. Removes tab-scoped instances;
  // instances tracked by the global machinery are skipped (use the
  // identifier-only overload).
  void Hide(content::WebContents* web_contents,
            infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // Removes an infobar returned by Show() without reporting a result. No-op
  // if the infobar is already closing.
  void Hide(infobars::InfoBar* infobar);

  // Hides the infobar. For kTab scope this only reaches the last-active
  // browser's active tab; prefer the WebContents overload.
  void Hide(infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerWillBeDestroyed(infobars::InfoBarManager* manager) override;

 private:
  // Returns the approved priority for an InfoBar.
  InfoBarDelegate::InfobarPriority GetApprovedPriority(
      infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  void OnActiveTabChanged(BrowserWindowInterface* browser);
  bool IsGlobal(infobars::InfoBarDelegate::InfoBarIdentifier identifier);
  bool IsTrackedGlobalInstance(infobars::InfoBar* infobar) const;
  BrowserWindowInterface* FindBrowserWithWebContents(
      content::WebContents* web_contents);

  ui::ScopedUnownedUserData<BrowserInfoBarManager> scoped_unowned_user_data_;

  std::map<infobars::InfoBarDelegate::InfoBarIdentifier, InfoBarSpec>
      registered_specs_;

  struct GlobalInfoBarContext {
    InfoBarSpec spec;
    std::map<infobars::InfoBarManager*, infobars::InfoBar*> active_instances;
  };

  // Tracking for active global infobars and their instances.
  std::map<infobars::InfoBarDelegate::InfoBarIdentifier, GlobalInfoBarContext>
      active_global_infobars_;

  // Track the last active tab's InfoBarManager for each browser.
  std::map<BrowserWindowInterface*, infobars::InfoBarManager*>
      last_active_managers_;

  // Subscriptions for active tab changes in each browser.
  std::map<BrowserWindowInterface*, base::CallbackListSubscription>
      active_tab_subscriptions_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  base::ScopedMultiSourceObservation<infobars::InfoBarManager,
                                     infobars::InfoBarManager::Observer>
      infobar_manager_observations_{this};
};

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_
