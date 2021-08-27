// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/process/process.h"
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
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

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

std::string GetAppId(content::WebContents* contents) {
  return GetInstanceAppIdForWebContents(contents).value_or("");
}

bool IsBrowserVisible(Browser* browser) {
  aura::Window* window = browser->window()->GetNativeWindow();
  return window->IsVisible();
}

bool IsBrowserActive(Browser* browser) {
  return browser->window()->IsActive();
}

bool IsAppVisible(Browser* browser, content::WebContents* contents) {
  return IsBrowserVisible(browser);
}

bool IsAppActive(Browser* browser, content::WebContents* contents) {
  return IsBrowserActive(browser) &&
         browser->tab_strip_model()->GetActiveWebContents() == contents;
}

WebContentsId::Generator& GetWebContentsIdGenerator() {
  // ID generator shared between all |BrowserAppInstanceTracker| instances as
  // IDs need to be unique within the scope of the process.
  static WebContentsId::Generator instance;
  return instance;
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
      owner_->OnTabNavigationFinished(web_contents());
    }
  }

 private:
  BrowserAppInstanceTracker* owner_;
};

const base::Feature BrowserAppInstanceTracker::kEnabled{
    "EnableBrowserAppsTracker", base::FEATURE_DISABLED_BY_DEFAULT};

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
  if (browser_window_observations_.GetSourcesCount() > 0) {
    // TODO(crbug.com/1236273): Remove when confident it does not happen.
    base::debug::DumpWithoutCrashing();
  }
  if (!tracked_browsers_.empty()) {
    // TODO(crbug.com/1236273): Remove when confident it does not happen.
    base::debug::DumpWithoutCrashing();
  }
}

std::unique_ptr<BrowserAppInstanceTracker> BrowserAppInstanceTracker::Create(
    Profile* profile,
    AppRegistryCache& app_registry_cache) {
  if (!base::FeatureList::IsEnabled(kEnabled)) {
    return nullptr;
  }
  return std::make_unique<BrowserAppInstanceTracker>(profile,
                                                     app_registry_cache);
}

std::set<const BrowserAppInstance*>
BrowserAppInstanceTracker::GetAppInstancesByAppId(
    const std::string& app_id) const {
  std::set<const BrowserAppInstance*> result;
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      result.insert(app_instance.get());
    }
  }
  for (const auto& pair : chrome_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      result.insert(app_instance.get());
    }
  }
  return result;
}

bool BrowserAppInstanceTracker::IsAppRunning(const std::string& app_id) const {
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      return true;
    }
  }
  for (const auto& pair : chrome_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->app_id == app_id) {
      return true;
    }
  }
  return false;
}

const BrowserAppInstance* BrowserAppInstanceTracker::GetAppInstance(
    content::WebContents* contents) const {
  auto it = app_instances_.find(contents);
  return it == app_instances_.end() ? nullptr : it->second.get();
}

const BrowserAppInstance*
BrowserAppInstanceTracker::GetAppInstanceByWebContentsId(
    WebContentsId web_contents_id) const {
  for (const auto& pair : app_instances_) {
    const auto& app_instance = pair.second;
    if (app_instance->web_contents_id == web_contents_id) {
      return app_instance.get();
    }
  }
  return nullptr;
}

const BrowserAppInstance* BrowserAppInstanceTracker::GetChromeInstance(
    Browser* browser) const {
  auto it = chrome_instances_.find(browser);
  return it == chrome_instances_.end() ? nullptr : it->second.get();
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

void BrowserAppInstanceTracker::OnWindowVisibilityChanged(aura::Window* window,
                                                          bool visible) {
  DCHECK(window);
  if (!IsWindowTracked(window)) {
    return;
  }
  if (Browser* browser = GetBrowserWithAuraWindow(window)) {
    OnBrowserWindowUpdated(browser);
  }
}

void BrowserAppInstanceTracker::OnWindowDestroying(aura::Window* window) {
  // TODO(crbug.com/1236273): Remove when confident it does not happen.
  base::debug::DumpWithoutCrashing();
}

void BrowserAppInstanceTracker::OnBrowserAdded(Browser* browser) {
  // TODO(crbug.com/1236273): Remove when confident it does not happen.
  if (base::Contains(tracked_browsers_, browser)) {
    base::debug::DumpWithoutCrashing();
  }
}

void BrowserAppInstanceTracker::OnBrowserSetLastActive(Browser* browser) {
  OnBrowserWindowUpdated(browser);
}

void BrowserAppInstanceTracker::OnBrowserNoLongerActive(Browser* browser) {
  OnBrowserWindowUpdated(browser);
}

void BrowserAppInstanceTracker::OnBrowserRemoved(Browser* browser) {
  // TODO(crbug.com/1236273): Remove when confident it does not happen.
  if (base::Contains(tracked_browsers_, browser)) {
    base::debug::DumpWithoutCrashing();
  }
}

void BrowserAppInstanceTracker::OnAppUpdate(const AppUpdate& update) {
  if (!apps_util::AppTypeUsesWebContents(update.AppType())) {
    return;
  }
  // Sync app instances for existing tabs.
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
      webcontents_to_observer_map_[contents] =
          std::make_unique<BrowserAppInstanceTracker::WebContentsObserver>(
              contents, this);
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
    if (tab_will_be_closed) {
      DCHECK(base::Contains(webcontents_to_observer_map_, contents));
      webcontents_to_observer_map_.erase(contents);
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
  tracked_browsers_.insert(browser);
  BrowserWindow* window = browser->window();
  DCHECK(window && window->GetNativeWindow());
  browser_window_observations_.AddObservation(window->GetNativeWindow());
  if (browser->is_type_normal()) {
    CreateChromeInstance(browser);
  }
}

void BrowserAppInstanceTracker::OnBrowserLastTabDetached(Browser* browser) {
  BrowserWindow* window = browser->window();
  DCHECK(window && window->GetNativeWindow());
  browser_window_observations_.RemoveObservation(window->GetNativeWindow());
  RemoveChromeInstanceIfExists(browser);
  tracked_browsers_.erase(browser);
}

void BrowserAppInstanceTracker::OnTabCreated(Browser* browser,
                                             content::WebContents* contents) {
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
  auto it = app_instances_.find(contents);
  if (it != app_instances_.end()) {
    auto& app_instance = it->second;
    if (app_instance->app_id != new_app_id) {
      // If app ID changed on navigation, remove the old app.
      RemoveAppInstanceIfExists(contents);
      // Add the new app instance, if navigated to another app.
      if (!new_app_id.empty()) {
        CreateAppInstance(std::move(new_app_id), browser, contents);
      }
    } else {
      // App ID did not change, but other attributes may have.
      MaybeUpdateAppInstance(*app_instance, browser, contents);
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
}

void BrowserAppInstanceTracker::OnTabNavigationFinished(
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
  auto it = chrome_instances_.find(browser);
  if (it != chrome_instances_.end()) {
    MaybeUpdateChromeInstance(*it->second, browser);
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
  CreateInstance(app_instances_, contents,
                 base::WrapUnique(new BrowserAppInstance{
                     std::move(app_id),
                     (browser->is_type_app() || browser->is_type_app_popup())
                         ? BrowserAppInstance::Type::kAppWindow
                         : BrowserAppInstance::Type::kAppTab,
                     base::Process::Current().Pid(),
                     browser->window()->GetNativeWindow(),
                     GetWebContentsIdGenerator().GenerateNextId(),
                     IsAppVisible(browser, contents),
                     IsAppActive(browser, contents),
                 }));
}

void BrowserAppInstanceTracker::MaybeUpdateAppInstance(
    BrowserAppInstance& instance,
    Browser* browser,
    content::WebContents* contents) {
  MaybeUpdateInstance(instance, browser, contents);
}

void BrowserAppInstanceTracker::RemoveAppInstanceIfExists(
    content::WebContents* contents) {
  RemoveInstanceIfExists(app_instances_, contents);
}

void BrowserAppInstanceTracker::CreateChromeInstance(Browser* browser) {
  CreateInstance(chrome_instances_, browser,
                 base::WrapUnique(new BrowserAppInstance{
                     extension_misc::kChromeAppId,
                     BrowserAppInstance::Type::kChromeWindow,
                     base::Process::Current().Pid(),
                     browser->window()->GetNativeWindow(),
                     WebContentsId(0),
                     IsBrowserVisible(browser),
                     IsBrowserActive(browser),
                 }));
}

void BrowserAppInstanceTracker::MaybeUpdateChromeInstance(
    BrowserAppInstance& instance,
    Browser* browser) {
  // Browser/WebContents itself does not change for Chrome instances, but other
  // attributes may change.
  MaybeUpdateInstance(instance, browser, nullptr /* contents */);
}

void BrowserAppInstanceTracker::RemoveChromeInstanceIfExists(Browser* browser) {
  RemoveInstanceIfExists(chrome_instances_, browser);
}

template <typename KeyT>
void BrowserAppInstanceTracker::CreateInstance(
    std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
    const KeyT& key,
    std::unique_ptr<BrowserAppInstance> instance_ptr) {
  DCHECK(!base::Contains(instances, key));
  auto it = instances.insert(std::make_pair(key, std::move(instance_ptr)));
  auto& inserted_instance_ptr = it.first->second;
  for (auto& observer : observers_) {
    observer.OnBrowserAppAdded(*inserted_instance_ptr);
  }
}

void BrowserAppInstanceTracker::MaybeUpdateInstance(
    BrowserAppInstance& instance,
    Browser* browser,
    content::WebContents* contents) {
  DCHECK(browser);
  bool visible;
  bool active;
  if (contents) {
    visible = IsAppVisible(browser, contents);
    active = IsAppActive(browser, contents);
  } else {
    visible = IsBrowserVisible(browser);
    active = IsBrowserActive(browser);
  }
  aura::Window* window = browser->window()->GetNativeWindow();
  if (instance.window == window && instance.visible == visible &&
      instance.active == active) {
    return;
  }
  instance.window = window;
  instance.visible = visible;
  instance.active = active;

  for (auto& observer : observers_) {
    observer.OnBrowserAppUpdated(instance);
  }
}

bool BrowserAppInstanceTracker::IsBrowserTracked(Browser* browser) const {
  return base::Contains(tracked_browsers_, browser);
}

bool BrowserAppInstanceTracker::IsWindowTracked(aura::Window* window) const {
  return browser_window_observations_.IsObservingSource(window);
}

template <typename KeyT>
void BrowserAppInstanceTracker::RemoveInstanceIfExists(
    std::map<KeyT, std::unique_ptr<BrowserAppInstance>>& instances,
    const KeyT& key) {
  auto it = instances.find(key);
  if (it == instances.end()) {
    return;
  }
  auto app_instance = std::move(it->second);
  instances.erase(it);
  for (auto& observer : observers_) {
    observer.OnBrowserAppRemoved(*app_instance);
  }
}

}  // namespace apps
