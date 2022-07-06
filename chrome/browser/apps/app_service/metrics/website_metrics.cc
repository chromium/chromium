// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/history/core/browser/history_types.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace {

// Checks if a given browser is running a windowed app. It will return true for
// web apps, hosted apps, and packaged V1 apps.
bool IsAppBrowser(const Browser* browser) {
  return (browser->is_type_app() || browser->is_type_app_popup()) &&
         !web_app::GetAppIdFromApplicationName(browser->app_name()).empty();
}

aura::Window* GetWindowWithBrowser(Browser* browser) {
  if (!browser) {
    return nullptr;
  }
  BrowserWindow* browser_window = browser->window();
  // In some test cases, browser window might be skipped.
  return browser_window ? browser_window->GetNativeWindow() : nullptr;
}

aura::Window* GetWindowWithTabStripModel(TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model) {
      return GetWindowWithBrowser(browser);
    }
  }
  return nullptr;
}

wm::ActivationClient* GetActivationClientWithTabStripModel(
    TabStripModel* tab_strip_model) {
  auto* window = GetWindowWithTabStripModel(tab_strip_model);
  if (!window) {
    return nullptr;
  }

  auto* root_window = window->GetRootWindow();
  DCHECK(root_window);
  return wm::GetActivationClient(root_window);
}

}  // namespace

namespace apps {

constexpr char kWebsiteUsageTime[] = "app_platform_metrics.website_usage_time";
constexpr char kRunningTimeKey[] = "time";
constexpr char kUrlContentKey[] = "url_content";
constexpr char kPromotableKey[] = "promotable";

WebsiteMetrics::ActiveTabWebContentsObserver::ActiveTabWebContentsObserver(
    content::WebContents* contents,
    WebsiteMetrics* owner)
    : content::WebContentsObserver(contents), owner_(owner) {}

WebsiteMetrics::ActiveTabWebContentsObserver::~ActiveTabWebContentsObserver() =
    default;

void WebsiteMetrics::ActiveTabWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  owner_->OnWebContentsUpdated(web_contents());

  if (app_banner_manager_observer_.IsObserving()) {
    return;
  }

  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents());
  // In some test cases, AppBannerManager might be null.
  if (app_banner_manager) {
    app_banner_manager_observer_.Observe(app_banner_manager);
  }
}

void WebsiteMetrics::ActiveTabWebContentsObserver::WebContentsDestroyed() {
  app_banner_manager_observer_.Reset();
}

void WebsiteMetrics::ActiveTabWebContentsObserver::
    OnInstallableWebAppStatusUpdated() {
  owner_->OnInstallableWebAppStatusUpdated(web_contents());
}

base::Value WebsiteMetrics::UrlInfo::ConvertToValue() const {
  base::Value usage_time_dict(base::Value::Type::DICTIONARY);
  usage_time_dict.SetPath(kRunningTimeKey,
                          base::TimeDeltaToValue(running_time));
  usage_time_dict.SetIntKey(kUrlContentKey, static_cast<int>(url_content));
  usage_time_dict.SetBoolKey(kPromotableKey, promotable);
  return usage_time_dict;
}

WebsiteMetrics::WebsiteMetrics(Profile* profile)
    : profile_(profile), browser_tab_strip_tracker_(this, nullptr) {
  BrowserList::GetInstance()->AddObserver(this);
  browser_tab_strip_tracker_.Init();
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  if (history_service) {
    history_observation_.Observe(history_service);
  }
}

WebsiteMetrics::~WebsiteMetrics() {
  BrowserList::RemoveObserver(this);
}

void WebsiteMetrics::OnBrowserAdded(Browser* browser) {
  if (IsAppBrowser(browser)) {
    return;
  }

  auto* window = GetWindowWithBrowser(browser);
  if (window) {
    window_to_web_contents_[window] = nullptr;
  }
}

void WebsiteMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK(tab_strip_model);
  auto* window = GetWindowWithTabStripModel(tab_strip_model);
  if (!window || !base::Contains(window_to_web_contents_, window)) {
    // Skip the app browser window.
    return;
  }

  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeInsert(tab_strip_model, *change.GetInsert(),
                                  selection);
      break;
    case TabStripModelChange::kRemoved:
      OnTabStripModelChangeRemove(window, tab_strip_model, *change.GetRemove(),
                                  selection);
      break;
    case TabStripModelChange::kReplaced:
      OnTabStripModelChangeReplace(*change.GetReplace());
      break;
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (selection.active_tab_changed()) {
    OnActiveTabChanged(window, selection.old_contents, selection.new_contents);
  }
}

void WebsiteMetrics::OnWindowActivated(ActivationReason reason,
                                       aura::Window* gained_active,
                                       aura::Window* lost_active) {
  SetWindowInActivated(lost_active);
  SetWindowActivated(gained_active);
}

void WebsiteMetrics::OnURLsDeleted(history::HistoryService* history_service,
                                   const history::DeletionInfo& deletion_info) {
  // To simplify the implementation, remove all recorded urls no matter whatever
  // `deletion_info`.
  webcontents_to_ukm_key_.clear();
  url_infos_.clear();

  DictionaryPrefUpdate usage_time_update(profile_->GetPrefs(),
                                         kWebsiteUsageTime);
  auto& dict = usage_time_update->GetDict();
  dict.clear();
}

void WebsiteMetrics::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_observation_.IsObservingSource(history_service));
  history_observation_.Reset();
}

void WebsiteMetrics::OnFiveMinutes() {
  SaveUsageTime();
}

void WebsiteMetrics::OnTwoHours() {
  // TODO(crbug.com/1334173): Records the usage time UKM, and reset the local
  // variables after recording the UKM.

  std::map<GURL, UrlInfo> url_infos;
  for (const auto& it : webcontents_to_ukm_key_) {
    if (!base::Contains(url_infos, it.second)) {
      url_infos[it.second] = std::move(url_infos_[it.second]);
    }
  }
  url_infos.swap(url_infos_);
}

void WebsiteMetrics::OnTabStripModelChangeInsert(
    TabStripModel* tab_strip_model,
    const TabStripModelChange::Insert& insert,
    const TabStripSelectionChange& selection) {
  if (insert.contents.size() == 0) {
    return;
  }
  // First tab attached.
  if (tab_strip_model->count() == static_cast<int>(insert.contents.size())) {
    // Observe the activation client of the root window of the browser's aura
    // window if this is the first browser matching it (there is no other
    // tracked browser matching it).
    auto* activation_client =
        GetActivationClientWithTabStripModel(tab_strip_model);
    if (!activation_client_observations_.IsObservingSource(activation_client)) {
      activation_client_observations_.AddObservation(activation_client);
    }
  }
}

void WebsiteMetrics::OnTabStripModelChangeRemove(
    aura::Window* window,
    TabStripModel* tab_strip_model,
    const TabStripModelChange::Remove& remove,
    const TabStripSelectionChange& selection) {
  for (const auto& removed_tab : remove.contents) {
    OnTabClosed(removed_tab.contents);
  }

  // Last tab detached.
  if (tab_strip_model->count() == 0) {
    // Unobserve the activation client of the root window of the browser's aura
    // window if the last browser using it was just removed.
    auto* activation_client =
        GetActivationClientWithTabStripModel(tab_strip_model);
    if (activation_client_observations_.IsObservingSource(activation_client)) {
      activation_client_observations_.RemoveObservation(activation_client);
    }

    // The browser window will be closed, so remove the window and the web
    // contents.
    auto it = window_to_web_contents_.find(window);
    if (it != window_to_web_contents_.end()) {
      OnTabClosed(it->second);
      window_to_web_contents_.erase(it);
    }
  }
}

void WebsiteMetrics::OnTabStripModelChangeReplace(
    const TabStripModelChange::Replace& replace) {
  OnTabClosed(replace.old_contents);
}

void WebsiteMetrics::OnActiveTabChanged(aura::Window* window,
                                        content::WebContents* old_contents,
                                        content::WebContents* new_contents) {
  if (old_contents) {
    SetTabInActivated(old_contents);

    // Clear `old_contents` from `window_to_web_contents_`.
    auto it = window_to_web_contents_.find(window);
    if (it != window_to_web_contents_.end())
      it->second = nullptr;
  }

  if (new_contents) {
    SetTabActivated(new_contents);

    window_to_web_contents_[window] = new_contents;
    if (!base::Contains(webcontents_to_observer_map_, new_contents)) {
      webcontents_to_observer_map_[new_contents] =
          std::make_unique<WebsiteMetrics::ActiveTabWebContentsObserver>(
              new_contents, this);
    }
  }
}

void WebsiteMetrics::OnTabClosed(content::WebContents* web_contents) {
  SetTabInActivated(web_contents);
  webcontents_to_ukm_key_.erase(web_contents);
  webcontents_to_observer_map_.erase(web_contents);
}

void WebsiteMetrics::OnWebContentsUpdated(content::WebContents* web_contents) {
  // If there is an app for the url, we don't need to record the url, because
  // the app metrics can record the usage time metrics.
  if (GetInstanceAppIdForWebContents(web_contents).has_value()) {
    webcontents_to_ukm_key_.erase(web_contents);
    return;
  }

  auto* window =
      GetWindowWithBrowser(chrome::FindBrowserWithWebContents(web_contents));
  if (!window) {
    return;
  }

  // When the primary page of `web_contents` is changed, call SetTabInActivated
  // to calculate the usage time for the previous ukm key url.
  SetTabInActivated(web_contents);

  // When the primary page of `web_contents` is changed called by
  // contents::WebContentsObserver::PrimaryPageChanged(), set the visible url as
  // default value for the ukm key url.
  webcontents_to_ukm_key_[web_contents] = web_contents->GetVisibleURL();
  AddUrlInfo(web_contents->GetVisibleURL(), base::TimeTicks::Now(),
             UrlContent::kFullUrl, wm::IsActiveWindow(window),
             /*promotable=*/false);
}

void WebsiteMetrics::OnInstallableWebAppStatusUpdated(
    content::WebContents* web_contents) {
  auto it = webcontents_to_ukm_key_.find(web_contents);
  if (it == webcontents_to_ukm_key_.end()) {
    // If the `web_contents` has been removed or replaced, we don't need to set
    // the url.
    return;
  }

  // WebContents in app windows are filtered out in OnBrowserAdded. Installed
  // web apps opened in tabs are filtered out too. So every WebContents here
  // must be a website not installed. Check the manifest to get the scope or the
  // start url if there is a manifest.
  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents);
  DCHECK(app_banner_manager);

  if (blink::IsEmptyManifest(app_banner_manager->manifest())) {
    return;
  }

  auto* window =
      GetWindowWithBrowser(chrome::FindBrowserWithWebContents(web_contents));
  if (!window) {
    return;
  }

  DCHECK(!app_banner_manager->manifest().scope.is_empty());
  UpdateUrlInfo(it->second, app_banner_manager->manifest().scope,
                UrlContent::kScope, wm::IsActiveWindow(window),
                /*promotable=*/true);
  it->second = app_banner_manager->manifest().scope;
}

void WebsiteMetrics::AddUrlInfo(const GURL& url,
                                const base::TimeTicks& start_time,
                                UrlContent url_content,
                                bool is_activated,
                                bool promotable) {
  auto& url_info = url_infos_[url];
  url_info.start_time = start_time;
  url_info.url_content = url_content;
  url_info.is_activated = is_activated;
  url_info.promotable = promotable;
}

void WebsiteMetrics::UpdateUrlInfo(const GURL& old_url,
                                   const GURL& new_url,
                                   UrlContent url_content,
                                   bool is_activated,
                                   bool promotable) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  auto it = url_infos_.find(old_url);
  if (it != url_infos_.end()) {
    start_time = it->second.start_time;
    url_infos_.erase(old_url);
  }

  AddUrlInfo(new_url, start_time, url_content, is_activated, promotable);
}

void WebsiteMetrics::SetWindowActivated(aura::Window* window) {
  auto it = window_to_web_contents_.find(window);
  if (it != window_to_web_contents_.end()) {
    SetTabActivated(it->second);
  }
}

void WebsiteMetrics::SetWindowInActivated(aura::Window* window) {
  auto it = window_to_web_contents_.find(window);
  if (it != window_to_web_contents_.end()) {
    SetTabInActivated(it->second);
  }
}

void WebsiteMetrics::SetTabActivated(content::WebContents* web_contents) {
  auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
  if (web_contents_it == webcontents_to_ukm_key_.end()) {
    return;
  }
  auto url_it = url_infos_.find(web_contents_it->second);
  if (url_it == url_infos_.end()) {
    return;
  }
  url_it->second.start_time = base::TimeTicks::Now();
  url_it->second.is_activated = true;
}

void WebsiteMetrics::SetTabInActivated(content::WebContents* web_contents) {
  auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
  if (web_contents_it == webcontents_to_ukm_key_.end()) {
    return;
  }

  // Check whether `web_contents` is activated. If yes, calculate the running
  // time based on the start time set when `web_contents` is activated.
  auto it = url_infos_.find(web_contents_it->second);
  if (it == url_infos_.end() || !it->second.is_activated) {
    return;
  }

  DCHECK_GE(base::TimeTicks::Now(), it->second.start_time);
  it->second.running_time += base::TimeTicks::Now() - it->second.start_time;
  it->second.is_activated = false;
}

void WebsiteMetrics::SaveUsageTime() {
  DictionaryPrefUpdate usage_time_update(profile_->GetPrefs(),
                                         kWebsiteUsageTime);
  auto& dict = usage_time_update->GetDict();
  dict.clear();
  for (auto it : url_infos_) {
    if (it.second.is_activated) {
      it.second.running_time += base::TimeTicks::Now() - it.second.start_time;
      it.second.start_time = base::TimeTicks::Now();
    }
    if (!it.second.running_time.is_zero()) {
      dict.Set(it.first.spec(), it.second.ConvertToValue());
    }
  }
}

}  // namespace apps
