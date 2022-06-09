// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
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

void WebsiteMetrics::OnBrowserRemoved(Browser* browser) {
  auto* window = GetWindowWithBrowser(browser);
  if (window) {
    window_to_web_contents_.erase(window);
  }
}

void WebsiteMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeInsert(tab_strip_model, *change.GetInsert(),
                                  selection);
      break;
    case TabStripModelChange::kRemoved:
      OnTabStripModelChangeRemove(tab_strip_model, *change.GetRemove(),
                                  selection);
      break;
    case TabStripModelChange::kReplaced:
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
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
  // TODO(crbug.com/1334173): Remove local records for urls in `deletion_info`.
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
    TabStripModel* tab_strip_model,
    const TabStripModelChange::Remove& remove,
    const TabStripSelectionChange& selection) {
  // Last tab detached.
  if (tab_strip_model->count() == 0) {
    // Unobserve the activation client of the root window of the browser's aura
    // window if the last browser using it was just removed.
    auto* activation_client =
        GetActivationClientWithTabStripModel(tab_strip_model);
    if (activation_client_observations_.IsObservingSource(activation_client)) {
      activation_client_observations_.RemoveObservation(activation_client);
    }
  }
}

}  // namespace apps
