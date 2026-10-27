// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
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

  // Replaces `old_infobar` in place without reporting a result for it.
  infobars::InfoBar* Replace(
      infobars::InfoBar* old_infobar,
      infobars::InfoBarDelegate::InfoBarIdentifier identifier,
      InfoBarShowParams params = InfoBarShowParams());

  // Shows the infobar in every browser. Returns true if any instance was
  // added.
  bool ShowGlobally(infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // As above, with per-show overrides applied to every instance, including
  // ones mirrored into browsers created later. `params.scope` must be
  // unset; scope overrides are a Show() concept.
  bool ShowGlobally(infobars::InfoBarDelegate::InfoBarIdentifier identifier,
                    InfoBarShowParams params);

  // Hides the infobar in `web_contents`. Removes tab-scoped instances;
  // instances tracked by the global machinery are skipped (use the
  // identifier-only overload).
  void Hide(content::WebContents* web_contents,
            infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // Removes an infobar returned by Show() without reporting a result. No-op
  // if the infobar is already closing.
  void Hide(infobars::InfoBar* infobar);

  // Hides the infobar everywhere. For `InfoBarScope::kGlobal` scope, it is
  // every mirrored instance, and the infobar stops appearing in new windows
  // and tabs. For `InfoBarScope::kTab` scope, it is every instance that
  // `Show()` or `Replace()` added and that is still showing, in any tab of any
  // browser. The `browser_filter` only applies to `kTab` scope and limits the
  // hiding to the browsers it accepts.
  void Hide(infobars::InfoBarDelegate::InfoBarIdentifier identifier,
            const InfoBarSpec::BrowserFilter& browser_filter =
                InfoBarSpec::BrowserFilter());

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerWillBeDestroyed(infobars::InfoBarManager* manager) override;

 private:
  friend class RegistryInfoBarDelegate;

  void OnActiveTabChanged(BrowserWindowInterface* browser);
  bool IsTrackedGlobalInstance(infobars::InfoBar* infobar) const;

  // Records a tab-scoped `infobar` that was just added, so that
  // Hide(identifier) can reach it, and observes its InfoBarManager to known
  // when it goes away.
  void TrackTabInstance(infobars::InfoBar* infobar);

  ui::ScopedUnownedUserData<BrowserInfoBarManager> scoped_unowned_user_data_;

  std::map<infobars::InfoBarDelegate::InfoBarIdentifier, InfoBarSpec>
      registered_specs_;

  struct GlobalInfoBarContext {
    InfoBarSpec spec;
    InfoBarShowParams params;
    std::optional<InfoBarResult> pending_result = InfoBarResult::kIgnored;
    std::map<infobars::InfoBarManager*, infobars::InfoBar*> active_instances;
  };

  // Tracking for active global infobars and their instances.
  std::map<infobars::InfoBarDelegate::InfoBarIdentifier, GlobalInfoBarContext>
      active_global_infobars_;

  // Tab-scoped instances added by Show() or Replace() that are still in their
  // InfoBarManager. An instance leaves when it is removed or replaced, before
  // it can be destroyed.
  std::set<raw_ptr<infobars::InfoBar>> active_tab_infobars_;

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
