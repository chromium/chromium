// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/browser_app_instance_map.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"

namespace apps {

namespace {

Browser* GetBrowserWithTabStripModel(TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model)
      return browser;
  }
  return nullptr;
}

Browser* GetBrowserWithAuraWindow(aura::Window* aura_window) {
  for (auto* browser : *BrowserList::GetInstance()) {
    BrowserWindow* window = browser->window();
    if (window && window->GetNativeWindow() == aura_window) {
      return browser;
    }
  }
  return nullptr;
}

aura::Window* AuraWindowForBrowser(Browser* browser) {
  BrowserWindow* window = browser->window();
  DCHECK(window && window->GetNativeWindow());
  aura::Window* aura_window = window->GetNativeWindow();
  DCHECK(aura_window);
  return aura_window;
}

wm::ActivationClient* ActivationClientForBrowser(Browser* browser) {
  aura::Window* window = AuraWindowForBrowser(browser)->GetRootWindow();
  wm::ActivationClient* client = wm::GetActivationClient(window);
  DCHECK(client);
  return client;
}

std::string GetAppId(content::WebContents* contents) {
  return GetInstanceAppIdForWebContents(contents).value_or("");
}

bool IsBrowserActive(Browser* browser) {
  auto* aura_window = AuraWindowForBrowser(browser);
  auto* activation_client = ActivationClientForBrowser(browser);
  return activation_client->GetActiveWindow() == aura_window;
}

bool IsWebContentsActive(Browser* browser, content::WebContents* contents) {
  return browser->tab_strip_model()->GetActiveWebContents() == contents;
}

}  // namespace

// Helper class to notify BrowserAppInstanceTracker when WebContents navigation
// finishes.
class BrowserAppInstanceTracker::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* contents,
                               BrowserAppInstanceTracker* owner)
      : content::WebContentsObserver(contents), owner_(owner) {}
  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;
  ~WebContentsObserver() override = default;

  // content::WebContentsObserver
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // TODO(crbug.com/1229189): Replace this callback with
    // WebContentObserver::PrimaryPageChanged() when fixed.
    if (handle->IsInPrimaryMainFrame() && handle->HasCommitted()) {
      owner_->OnWebContentsUpdated(web_contents());
    }
  }

  void TitleWasSet(content::NavigationEntry* entry) override {
    if (entry) {
      owner_->OnWebContentsUpdated(web_contents());
    }
  }

 private:
  BrowserAppInstanceTracker* const owner_;
};

BrowserAppInstanceTracker::BrowserAppInstanceTracker(
    Profile* profile,
    AppRegistryCache& app_registry_cache)
    : AppRegistryCache::Observer(&app_registry_cache),
      profile_(profile),
      browser_tab_strip_tracker_(this, this) {
  BrowserList::GetInstance()->AddObserver(this);
  browser_tab_strip_tracker_.Init();
}

BrowserAppInstanceTracker::~BrowserAppInstanceTracker() {
  BrowserList::GetInstance()->RemoveObserver(this);
  DCHECK_EQ(activation_client_observations_.GetSourcesCount(), 0u);
  DCHECK(tracked_browsers_.empty());
  DCHECK(observers_.empty());
}

const BrowserAppInstance* BrowserAppInstanceTracker::GetAppInstance(
    content::WebContents* contents) const {
  return GetInstance(app_instances_, contents);
}

const BrowserWindowInstance* BrowserAppInstanceTracker::GetWindowInstance(
    Browser* browser) const {
  return GetInstance(window_instances_, browser);
}

void BrowserAppInstanceTracker::ActivateTabInstance(base::UnguessableToken id) {
  for (const auto& pair : app_instances_) {
    const BrowserAppInstance& instance = *pair.second;
    if (instance.id == id) {
      Browser* browser = chrome::FindBrowserWithWebContents(pair.first);
      TabStripModel* tab_strip = browser->tab_strip_model();
      int index = tab_strip->GetIndexOfWebContents(pair.first);
      DCHECK_NE(TabStripModel::kNoTab, index);
      tab_strip->ActivateTabAt(index);
      break;
    }
  }
}

void BrowserAppInstanceTracker::StopInstancesOfApp(const std::string& app_id) {
  std::vector<content::WebContents*> web_contents_to_close;
  for (const auto& pair : app_instances_) {
    if (pair.second->app_id == app_id) {
      web_contents_to_close.push_back(pair.first);
    }
  }

  for (content::WebContents* web_contents : web_contents_to_close) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (!browser)
      continue;
    int index = browser->tab_strip_model()->GetIndexOfWebContents(web_contents);
    DCHECK(index != TabStripModel::kNoTab);
    browser->tab_strip_model()->CloseWebContentsAt(index,
                                                   TabStripModel::CLOSE_NONE);
  }
}

void BrowserAppInstanceTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Browser* browser = GetBrowserWithTabStripModel(tab_strip_model);
  DCHECK(browser);

  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeInsert(browser, *change.GetInsert(), selection);
      break;
    case TabStripModelChange::kRemoved:
      OnTabStripModelChangeRemove(browser, *change.GetRemove(), selection);
      break;
    case TabStripModelChange::kReplaced:
      OnTabStripModelChangeReplace(browser, *change.GetReplace());
      break;
    case TabStripModelChange::kMoved:
      // Ignored.
      break;
    case TabStripModelChange::kSelectionOnly:
      OnTabStripModelChangeSelection(browser, selection);
      break;
  }
}

bool BrowserAppInstanceTracker::ShouldTrackBrowser(Browser* browser) {
  return profile_->IsSameOrParent(browser->profile());
}

void BrowserAppInstanceTracker::OnWindowActivated(ActivationReason reason,
                                                  aura::Window* gained_active,
                                                  aura::Window* lost_active) {
  if (Browser* browser = GetBrowserWithAuraWindow(lost_active)) {
    OnBrowserWindowUpdated(browser);
  }
  if (Browser* browser = GetBrowserWithAuraWindow(gained_active)) {
    OnBrowserWindowUpdated(browser);
  }
}

void BrowserAppInstanceTracker::OnBrowserAdded(Browser* browser) {
  DCHECK(!base::Contains(tracked_browsers_, browser));
}

void BrowserAppInstanceTracker::OnBrowserRemoved(Browser* browser) {
  DCHECK(!base::Contains(tracked_browsers_, browser));
}

void BrowserAppInstanceTracker::OnAppUpdate(const AppUpdate& update) {
  if (!apps_util::AppTypeUsesWebContents(update.AppType())) {
    return;
  }
  // Sync app instances for existing tabs.
  // Iterate over the full list of browsers instead of tracked_browsers_ in case
  // tracked_browsers_ is out of date with global state.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (!IsBrowserTracked(browser)) {
      continue;
    }
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
      OnTabUpdated(browser, contents);
    }
  }
}

void BrowserAppInstanceTracker::OnAppRegistryCacheWillBeDestroyed(
    AppRegistryCache* cache) {
  Observe(nullptr);
}

void BrowserAppInstanceTracker::OnTabStripModelChangeInsert(
    Browser* browser,
    const TabStripModelChange::Insert& insert,
    const TabStripSelectionChange& selection) {
  if (selection.old_contents) {
    // A tab got deactivated on insertion.
    OnTabUpdated(browser, selection.old_contents);
  }
  if (insert.contents.size() == 0) {
    return;
  }
  // First tab attached.
  if (browser->tab_strip_model()->count() ==
      static_cast<int>(insert.contents.size())) {
    OnBrowserFirstTabAttached(browser);
  }
  for (const auto& inserted_tab : insert.contents) {
    content::WebContents* contents = inserted_tab.contents;
    bool tab_is_new = !base::Contains(webcontents_to_observer_map_, contents);
#if DCHECK_IS_ON()
    if (tab_is_new) {
      DCHECK(!base::Contains(tabs_in_transit_, contents));
    } else {
      // The tab must be in the set of tabs in transit.
      DCHECK(tabs_in_transit_.erase(contents) == 1);
    }
#endif
    if (tab_is_new) {
      OnTabCreated(browser, contents);
    }
    OnTabAttached(browser, contents);
  }
}

void BrowserAppInstanceTracker::OnTabStripModelChangeRemove(
    Browser* browser,
    const TabStripModelChange::Remove& remove,
    const TabStripSelectionChange& selection) {
  for (const auto& removed_tab : remove.contents) {
    content::WebContents* contents = removed_tab.contents;
    bool tab_will_be_closed = false;
    switch (removed_tab.remove_reason) {
      case TabStripModelChange::RemoveReason::kDeleted:
      case TabStripModelChange::RemoveReason::kCached:
#if DCHECK_IS_ON()
        DCHECK(!base::Contains(tabs_in_transit_, contents));
#endif
        tab_will_be_closed = true;
        break;
      case TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip:
        // The tab will be reinserted immediately into another browser, so
        // this event is ignored.
        if (browser->is_type_devtools()) {
          // TODO(crbug.com/1221967): when a dev tools window is docked, and
          // its WebContents is removed, it will not be reinserted into
          // another tab strip, so it should be treated as closed.
          tab_will_be_closed = true;
        } else {
          // The tab must not be already in the set of tabs in transit.
#if DCHECK_IS_ON()
          DCHECK(tabs_in_transit_.insert(contents).second);
#endif
        }
        break;
    }
    if (tab_will_be_closed) {
      OnTabClosing(browser, contents);
    }
  }
  // Last tab detached.
  if (browser->tab_strip_model()->count() == 0) {
    OnBrowserLastTabDetached(browser);
  }
  if (selection.new_contents) {
    // A tab got activated on removal.
    OnTabUpdated(browser, selection.new_contents);
  }
}

void BrowserAppInstanceTracker::OnTabStripModelChangeReplace(
    Browser* browser,
    const TabStripModelChange::Replace& replace) {
  // Simulate closing the old tab and opening a new tab.
  OnTabClosing(browser, replace.old_contents);
  OnTabCreated(browser, replace.new_contents);
  OnTabAttached(browser, replace.new_contents);
}

void BrowserAppInstanceTracker::OnTabStripModelChangeSelection(
    Browser* browser,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  if (selection.new_contents) {
    OnTabUpdated(browser, selection.new_contents);
  }
  if (selection.old_contents) {
    OnTabUpdated(browser, selection.old_contents);
  }
}

void BrowserAppInstanceTracker::OnBrowserFirstTabAttached(Browser* browser) {
  // Observe the activation client of the root window of the browser's aura
  // window if this is the first browser matching it (there is no other tracked
  // browser matching it).
  wm::ActivationClient* activation_client = ActivationClientForBrowser(browser);
  if (!IsActivationClientTracked(activation_client)) {
    activation_client_observations_.AddObservation(activation_client);
  }

  tracked_browsers_.insert(browser);
  if (browser->is_type_normal() || browser->is_type_popup() ||
      browser->is_type_devtools()) {
    CreateWindowInstance(browser);
  }
}

void BrowserAppInstanceTracker::OnBrowserLastTabDetached(Browser* browser) {
  RemoveWindowInstanceIfExists(browser);
  tracked_browsers_.erase(browser);

  // Unobserve the activation client of the root window of the browser's aura
  // window if the last browser using it was just removed.
  wm::ActivationClient* activation_client = ActivationClientForBrowser(browser);
  if (!IsActivationClientTracked(activation_client)) {
    activation_client_observations_.RemoveObservation(activation_client);
  }
}

void BrowserAppInstanceTracker::OnTabCreated(Browser* browser,
                                             content::WebContents* contents) {
  webcontents_to_observer_map_[contents] =
      std::make_unique<BrowserAppInstanceTracker::WebContentsObserver>(contents,
                                                                       this);

  std::string app_id = GetAppId(contents);
  if (!app_id.empty()) {
    CreateAppInstance(std::move(app_id), browser, contents);
  }
}

void BrowserAppInstanceTracker::OnTabAttached(Browser* browser,
                                              content::WebContents* contents) {
  OnTabUpdated(browser, contents);
}

void BrowserAppInstanceTracker::OnTabUpdated(Browser* browser,
                                             content::WebContents* contents) {
  std::string new_app_id = GetAppId(contents);
  BrowserAppInstance* instance = GetInstance(app_instances_, contents);
  if (instance) {
    if (instance->app_id != new_app_id) {
      // If app ID changed on navigation, remove the old app.
      RemoveAppInstanceIfExists(contents);
      // Add the new app instance, if navigated to another app.
      if (!new_app_id.empty()) {
        CreateAppInstance(std::move(new_app_id), browser, contents);
      }
    } else {
      // App ID did not change, but other attributes may have.
      MaybeUpdateAppInstance(*instance, browser, contents);
    }
  } else if (!new_app_id.empty()) {
    // Tab previously had no app ID, but navigated to a URL that does.
    CreateAppInstance(std::move(new_app_id), browser, contents);
  } else {
    // Tab without an app has changed, we don't care about it.
  }
}

void BrowserAppInstanceTracker::OnTabClosing(Browser* browser,
                                             content::WebContents* contents) {
  RemoveAppInstanceIfExists(contents);
  DCHECK(base::Contains(webcontents_to_observer_map_, contents));
  webcontents_to_observer_map_.erase(contents);
}

void BrowserAppInstanceTracker::OnWebContentsUpdated(
    content::WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    OnTabUpdated(browser, contents);
  }
}

void BrowserAppInstanceTracker::OnBrowserWindowUpdated(Browser* browser) {
  // We only want to send window events for the browsers we track to avoid
  // sending window events before a "browser added" event.
  if (!IsBrowserTracked(browser)) {
    return;
  }
  BrowserWindowInstance* instance = GetInstance(window_instances_, browser);
  if (instance) {
    MaybeUpdateWindowInstance(*instance, browser);
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); i++) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    OnTabUpdated(browser, contents);
  }
}

void BrowserAppInstanceTracker::CreateAppInstance(
    std::string app_id,
    Browser* browser,
    content::WebContents* contents) {
  auto& instance = AddInstance(
      app_instances_, contents,
      std::make_unique<BrowserAppInstance>(
          GenerateId(),
          (browser->is_type_app() || browser->is_type_app_popup())
              ? BrowserAppInstance::Type::kAppWindow
              : BrowserAppInstance::Type::kAppTab,
          std::move(app_id), browser->window()->GetNativeWindow(),
          base::UTF16ToUTF8(contents->GetTitle()), IsBrowserActive(browser),
          IsWebContentsActive(browser, contents)));
  for (auto& observer : observers_) {
    observer.OnBrowserAppAdded(instance);
  }
}

void BrowserAppInstanceTracker::MaybeUpdateAppInstance(
    BrowserAppInstance& instance,
    Browser* browser,
    content::WebContents* contents) {
  if (instance.MaybeUpdate(browser->window()->GetNativeWindow(),
                           base::UTF16ToUTF8(contents->GetTitle()),
                           IsBrowserActive(browser),
                           IsWebContentsActive(browser, contents))) {
    for (auto& observer : observers_) {
      observer.OnBrowserAppUpdated(instance);
    }
  }
}

void BrowserAppInstanceTracker::RemoveAppInstanceIfExists(
    content::WebContents* contents) {
  auto instance = PopInstanceIfExists(app_instances_, contents);
  if (instance) {
    for (auto& observer : observers_) {
      observer.OnBrowserAppRemoved(*instance);
    }
  }
}

void BrowserAppInstanceTracker::CreateWindowInstance(Browser* browser) {
  auto& instance =
      AddInstance(window_instances_, browser,
                  std::make_unique<BrowserWindowInstance>(
                      GenerateId(), browser->window()->GetNativeWindow(),
                      IsBrowserActive(browser)));
  for (auto& observer : observers_) {
    observer.OnBrowserWindowAdded(instance);
  }
}

void BrowserAppInstanceTracker::MaybeUpdateWindowInstance(
    BrowserWindowInstance& instance,
    Browser* browser) {
  if (instance.MaybeUpdate(IsBrowserActive(browser))) {
    for (auto& observer : observers_) {
      observer.OnBrowserWindowUpdated(instance);
    }
  }
}

void BrowserAppInstanceTracker::RemoveWindowInstanceIfExists(Browser* browser) {
  auto instance = PopInstanceIfExists(window_instances_, browser);
  if (instance) {
    for (auto& observer : observers_) {
      observer.OnBrowserWindowRemoved(*instance);
    }
  }
}

base::UnguessableToken BrowserAppInstanceTracker::GenerateId() const {
  return base::UnguessableToken::Create();
}

bool BrowserAppInstanceTracker::IsBrowserTracked(Browser* browser) const {
  return base::Contains(tracked_browsers_, browser);
}

bool BrowserAppInstanceTracker::IsActivationClientTracked(
    wm::ActivationClient* client) const {
  // Iterate over the full list of browsers instead of tracked_browsers_ in case
  // tracked_browsers_ is out of date with global state
  // TODO(crbug.com/1236273): This can be changed to iterate tracked_browsers_
  // when confident it doesn't get out of sync.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (IsBrowserTracked(browser) &&
        ActivationClientForBrowser(browser) == client) {
      return true;
    }
  }
  return false;
}

}  // namespace apps
