// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_activity_provider.h"

#include <algorithm>
#include <optional>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace ash::app_time {

namespace {

const Browser* GetBrowserForInstance(
    const apps::InstanceRegistry& instance_registry,
    const base::UnguessableToken& instance_id) {
  aura::Window* window = nullptr;
  instance_registry.ForOneInstance(
      instance_id,
      [&](const apps::InstanceUpdate& update) { window = update.Window(); });

  if (!window) {
    return nullptr;
  }

  BrowserList* list = BrowserList::GetInstance();
  for (const Browser* browser : *list) {
    if (browser->window()->GetNativeWindow() == window) {
      return browser;
    }
  }
  return nullptr;
}

const Browser* GetBrowserForTabStripModel(const TabStripModel* model) {
  BrowserList* list = BrowserList::GetInstance();
  for (const Browser* browser : *list) {
    if (browser->tab_strip_model() == model) {
      return browser;
    }
  }

  LOG(WARNING) << "Could not find a browser for the given TabStripModel.";
  return nullptr;
}

const Browser* GetBrowserForWebContents(const content::WebContents* contents) {
  BrowserList* list = BrowserList::GetInstance();
  for (const Browser* browser : *list) {
    const auto* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      if (tab_strip_model->GetWebContentsAt(i) == contents) {
        return browser;
      }
    }
  }

  LOG(WARNING) << "Could not find a browser for the given WebContents.";
  return nullptr;
}

}  // namespace

WebTimeActivityProvider::WebTimeActivityProvider(
    AppTimeController* app_time_controller,
    AppServiceWrapper* app_service_wrapper)
    : app_time_controller_(app_time_controller) {
  DCHECK(app_time_controller_);
  DCHECK(app_service_wrapper);

  BrowserList::GetInstance()->AddObserver(this);
  app_service_wrapper_observation_.Observe(app_service_wrapper);
}

WebTimeActivityProvider::~WebTimeActivityProvider() {
  BrowserList::GetInstance()->RemoveObserver(this);
  TabStripModelObserver::StopObservingAll(this);
}

void WebTimeActivityProvider::OnWebActivityChanged(
    const WebTimeNavigationObserver::NavigationInfo& info) {
  if (info.web_contents == nullptr) {
    return;
  }

  WebTimeNavigationObserver* observer =
      WebTimeNavigationObserver::FromWebContents(info.web_contents);

  // Only cache info for observers that this provider is tracking.
  if (observer && web_time_navigation_observers_.IsObservingSource(observer)) {
    navigation_info_map_[observer] = info;
  }

  if (info.is_web_app) {
    return;
  }

  const Browser* browser = GetBrowserForWebContents(info.web_contents);

  // The browser window is not active. This may happen when a navigation
  // finishes in the background.
  if (!base::Contains(active_browsers_, browser)) {
    return;
  }

  // Navigation finished in a background tab. Return.
  if (browser->tab_strip_model()->GetActiveWebContents() != info.web_contents) {
    return;
  }

  MaybeNotifyStateChange(base::Time::Now());
}

void WebTimeActivityProvider::WebTimeNavigationObserverDestroyed(
    WebTimeNavigationObserver* navigation_observer) {
  web_time_navigation_observers_.RemoveObservation(navigation_observer);
  navigation_info_map_.erase(navigation_observer);
}

void WebTimeActivityProvider::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::Type::kInserted) {
    TabsInserted(change.GetInsert());
  }

  const Browser* browser = GetBrowserForTabStripModel(tab_strip_model);

  // If the Browser is not the active browser, simply return.
  if (!base::Contains(active_browsers_, browser)) {
    return;
  }

  // Check if the active tab changed, or the content::WebContents in the
  // active tab was replaced:
  bool active_tab_changed = selection.active_tab_changed();
  bool web_content_replaced =
      change.type() == TabStripModelChange::Type::kReplaced;

  if (!(active_tab_changed || web_content_replaced)) {
    return;
  }

  MaybeNotifyStateChange(base::Time::Now());
}

void WebTimeActivityProvider::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void WebTimeActivityProvider::OnBrowserRemoved(Browser* browser) {
  if (!base::Contains(active_browsers_, browser)) {
    return;
  }
  active_browsers_.erase(browser);
  MaybeNotifyStateChange(base::Time::Now());
}

void WebTimeActivityProvider::OnAppActive(
    const AppId& app_id,
    const base::UnguessableToken& instance_id,
    base::Time timestamp) {
  if (app_id != GetChromeAppId()) {
    return;
  }

  const Browser* browser = GetBrowserForInstance(
      app_service_wrapper_observation_.GetSource()->GetInstanceRegistry(),
      instance_id);
  if (!browser) {
    return;
  }

  active_browsers_.insert(browser);
  MaybeNotifyStateChange(timestamp);
}

void WebTimeActivityProvider::OnAppInactive(
    const AppId& app_id,
    const base::UnguessableToken& instance_id,
    base::Time timestamp) {
  if (app_id != GetChromeAppId()) {
    return;
  }

  const Browser* browser = GetBrowserForInstance(
      app_service_wrapper_observation_.GetSource()->GetInstanceRegistry(),
      instance_id);
  if (!browser) {
    return;
  }

  if (!base::Contains(active_browsers_, browser)) {
    return;
  }

  active_browsers_.erase(browser);
  MaybeNotifyStateChange(timestamp);
}

void WebTimeActivityProvider::TabsInserted(
    const TabStripModelChange::Insert* insert) {
  for (const TabStripModelChange::ContentsWithIndex& content_with_index :
       insert->contents) {
    WebTimeNavigationObserver* navigation_observer =
        WebTimeNavigationObserver::FromWebContents(content_with_index.contents);

    // Continue if the navigation observer is not created or if |this| already
    // observes it.
    if (!navigation_observer ||
        web_time_navigation_observers_.IsObservingSource(navigation_observer)) {
      continue;
    }

    web_time_navigation_observers_.AddObservation(navigation_observer);
  }
}

void WebTimeActivityProvider::MaybeNotifyStateChange(base::Time timestamp) {
  ChromeAppActivityState new_state = CalculateChromeAppActivityState();
  if (new_state == chrome_app_activity_state_) {
    return;
  }

  chrome_app_activity_state_ = new_state;
  app_time_controller_->app_registry()->OnChromeAppActivityChanged(new_state,
                                                                   timestamp);
}

ChromeAppActivityState
WebTimeActivityProvider::CalculateChromeAppActivityState() const {
  for (const Browser* browser : active_browsers_) {
    const content::WebContents* contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!contents) {
      continue;
    }

    const WebTimeNavigationObserver* observer =
        WebTimeNavigationObserver::FromWebContents(contents);
    CHECK(observer) << "This code should not run if ChromeActivityReporting "
                       "feature is disabled";

    const auto it = navigation_info_map_.find(observer);

    // The first navigation has not occurred yet.
    if (it == navigation_info_map_.end()) {
      continue;
    }

    const WebTimeNavigationObserver::NavigationInfo& info = it->second;

    // Web apps opened in the browser are reported separately from the browser
    // activity.
    if (info.is_web_app) {
      continue;
    }

    return ChromeAppActivityState::kActive;
  }
  return ChromeAppActivityState::kInactive;
}

}  // namespace ash::app_time
