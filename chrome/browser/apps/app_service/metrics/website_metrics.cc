// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"

namespace {

wm::ActivationClient* GetActivationClientWithTabStripModel(
    TabStripModel* tab_strip_model) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model) {
      BrowserWindow* browser_window = browser->window();
      DCHECK(browser_window);
      aura::Window* window = browser_window->GetNativeWindow();
      DCHECK(window);
      auto* root_window = window->GetRootWindow();
      DCHECK(root_window);
      return wm::GetActivationClient(root_window);
    }
  }
  return nullptr;
}

}  // namespace

namespace apps {

WebsiteMetrics::WebsiteMetrics() : browser_tab_strip_tracker_(this, nullptr) {
  BrowserList::GetInstance()->AddObserver(this);
  browser_tab_strip_tracker_.Init();
}

WebsiteMetrics::~WebsiteMetrics() {
  BrowserList::RemoveObserver(this);
}

void WebsiteMetrics::OnBrowserAdded(Browser* browser) {
  // TODO(crbug.com/1334173): Save the browser window.
}

void WebsiteMetrics::OnBrowserRemoved(Browser* browser) {
  // TODO(crbug.com/1334173): Remove the browser window.
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
    if (!activation_client_observations_.IsObservingSource(activation_client))
      activation_client_observations_.AddObservation(activation_client);
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
    if (activation_client_observations_.IsObservingSource(activation_client))
      activation_client_observations_.RemoveObservation(activation_client);
  }
}

}  // namespace apps
