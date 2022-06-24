// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/history/core/browser/history_types.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/aura/window.h"

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

WebsiteMetrics::ActiveTabWebContentsObserver::ActiveTabWebContentsObserver(
    content::WebContents* contents,
    WebsiteMetrics* owner)
    : content::WebContentsObserver(contents), owner_(owner) {}

void WebsiteMetrics::ActiveTabWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  owner_->OnWebContentsUpdated(web_contents());
}

WebsiteMetrics::WebsiteMetrics(Profile* profile)
    : browser_tab_strip_tracker_(this, nullptr) {
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
  if (!base::Contains(window_to_web_contents_, window)) {
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
  // TODO(crbug.com/1334173): Calculate the usage time for the activated tab
  // url.
}

void WebsiteMetrics::OnURLsDeleted(history::HistoryService* history_service,
                                   const history::DeletionInfo& deletion_info) {
  // To simplify the implementation, remove all recorded urls no matter whatever
  // `deletion_info`.
  webcontents_to_ukm_key_.clear();
}

void WebsiteMetrics::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_observation_.IsObservingSource(history_service));
  history_observation_.Reset();
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
    webcontents_to_ukm_key_.erase(removed_tab.contents);
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
      webcontents_to_observer_map_.erase(it->second);
      webcontents_to_ukm_key_.erase(it->second);
      window_to_web_contents_.erase(it);
    }
  }
}

void WebsiteMetrics::OnTabStripModelChangeReplace(
    const TabStripModelChange::Replace& replace) {
  webcontents_to_ukm_key_.erase(replace.old_contents);
}

void WebsiteMetrics::OnActiveTabChanged(aura::Window* window,
                                        content::WebContents* old_contents,
                                        content::WebContents* new_contents) {
  if (old_contents) {
    webcontents_to_observer_map_.erase(old_contents);

    // Clear `old_contents` from `window_to_web_contents_`.
    auto it = window_to_web_contents_.find(window);
    if (it != window_to_web_contents_.end())
      it->second = nullptr;
  }

  if (new_contents) {
    window_to_web_contents_[window] = new_contents;
    if (!base::Contains(webcontents_to_observer_map_, new_contents)) {
      webcontents_to_observer_map_[new_contents] =
          std::make_unique<WebsiteMetrics::ActiveTabWebContentsObserver>(
              new_contents, this);
    }
  }

  // TODO(crbug.com/1334173): Calculate the usage time for the activated tab
  // url.
}

void WebsiteMetrics::OnWebContentsUpdated(content::WebContents* web_contents) {
  // TODO(crbug.com/1334173): Calculate the usage time for the url.

  // If there is an app for the url, we don't need to record the url, because
  // the app metrics can record the usage time metrics.
  if (GetInstanceAppIdForWebContents(web_contents).has_value()) {
    webcontents_to_ukm_key_.erase(web_contents);
    return;
  }

  // When the primary page of `web_contents` is changed called by
  // contents::WebContentsObserver::PrimaryPageChanged(), set the visible url as
  // default value for the ukm key url.
  webcontents_to_ukm_key_[web_contents] = web_contents->GetVisibleURL();

  // WebContents in app windows are filtered out in OnBrowserAdded. installed
  // web apps opened in tabs are filtered out too. So every WebContents here
  // must be a website not installed. Check the manifest to get the scope or the
  // start url if there is a manifest.
  webapps::InstallableParams params;
  params.valid_manifest = true;
  webapps::InstallableManager* manager =
      webapps::InstallableManager::FromWebContents(web_contents);
  DCHECK(manager);
  manager->GetData(
      params,
      base::BindOnce(&WebsiteMetrics::OnDidPerformInstallableWebAppCheck,
                     weak_factory_.GetWeakPtr(), web_contents));
}

void WebsiteMetrics::OnDidPerformInstallableWebAppCheck(
    content::WebContents* web_contents,
    const webapps::InstallableData& data) {
  auto it = webcontents_to_ukm_key_.find(web_contents);
  if (it == webcontents_to_ukm_key_.end()) {
    // If the `web_contents` has been removed or replaced, we don't need to set
    // the url.
    return;
  }

  if (blink::IsEmptyManifest(data.manifest)) {
    return;
  }

  if (!data.manifest.scope.is_empty()) {
    it->second = data.manifest.scope;
  } else if (!data.manifest.start_url.is_empty()) {
    it->second = data.manifest.start_url;
  }
}

}  // namespace apps
