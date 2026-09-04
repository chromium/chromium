// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_activity_provider.h"

#include <algorithm>
#include <optional>

#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace ash::app_time {

namespace {

ash::BrowserDelegate* GetBrowserForInstance(
    const apps::InstanceRegistry& instance_registry,
    const base::UnguessableToken& instance_id) {
  aura::Window* window = nullptr;
  instance_registry.ForOneInstance(
      instance_id,
      [&](const apps::InstanceUpdate& update) { window = update.Window(); });
  if (!window) {
    return nullptr;
  }

  ash::BrowserDelegate* found_browser = nullptr;
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [window, &found_browser](ash::BrowserDelegate& browser) {
        if (browser.GetNativeWindow() == window) {
          found_browser = &browser;
          return ash::BrowserController::kBreakIteration;
        }
        return ash::BrowserController::kContinueIteration;
      });
  return found_browser;
}

}  // namespace

WebTimeActivityProvider::WebTimeActivityProvider(
    AppTimeController* app_time_controller,
    AppServiceWrapper* app_service_wrapper)
    : app_time_controller_(app_time_controller) {
  DCHECK(app_time_controller_);
  DCHECK(app_service_wrapper);
  browser_observation_.Observe(ash::BrowserController::GetInstance());
  browser_tab_observation_.Observe(ash::BrowserController::GetInstance());
  app_service_wrapper_observation_.Observe(app_service_wrapper);
}

WebTimeActivityProvider::~WebTimeActivityProvider() = default;

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

  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForTab(
          info.web_contents);

  // The browser window is not active. This may happen when a navigation
  // finishes in the background.
  if (!browser || !active_browsers_.contains(browser)) {
    return;
  }

  // Navigation finished in a background tab. Return.
  if (browser->GetActiveWebContents() != info.web_contents) {
    return;
  }

  MaybeNotifyStateChange(base::Time::Now());
}

void WebTimeActivityProvider::WebTimeNavigationObserverDestroyed(
    WebTimeNavigationObserver* navigation_observer) {
  web_time_navigation_observers_.RemoveObservation(navigation_observer);
  navigation_info_map_.erase(navigation_observer);
}

void WebTimeActivityProvider::OnTabInserted(ash::BrowserDelegate* browser,
                                            content::WebContents* contents) {
  auto* navigation_observer =
      WebTimeNavigationObserver::FromWebContents(contents);
  if (navigation_observer &&
      !web_time_navigation_observers_.IsObservingSource(navigation_observer)) {
    web_time_navigation_observers_.AddObservation(navigation_observer);
  }
}

void WebTimeActivityProvider::OnTabReplaced(
    ash::BrowserDelegate* browser,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (active_browsers_.contains(browser)) {
    MaybeNotifyStateChange(base::Time::Now());
  }
}

void WebTimeActivityProvider::OnActiveWebContentsChanged(
    ash::BrowserDelegate* browser,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (active_browsers_.contains(browser)) {
    MaybeNotifyStateChange(base::Time::Now());
  }
}

void WebTimeActivityProvider::OnBrowserClosed(
    ash::BrowserDelegate* browser_delegate) {
  if (active_browsers_.contains(browser_delegate)) {
    active_browsers_.erase(browser_delegate);
    MaybeNotifyStateChange(base::Time::Now());
  }
}

void WebTimeActivityProvider::OnAppActive(
    const AppId& app_id,
    const base::UnguessableToken& instance_id,
    base::Time timestamp) {
  if (app_id != GetChromeAppId()) {
    return;
  }

  ash::BrowserDelegate* browser = GetBrowserForInstance(
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

  ash::BrowserDelegate* browser = GetBrowserForInstance(
      app_service_wrapper_observation_.GetSource()->GetInstanceRegistry(),
      instance_id);
  if (!browser) {
    return;
  }

  if (active_browsers_.erase(browser)) {
    MaybeNotifyStateChange(timestamp);
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
  for (const ash::BrowserDelegate* browser : active_browsers_) {
    const content::WebContents* contents = browser->GetActiveWebContents();
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
