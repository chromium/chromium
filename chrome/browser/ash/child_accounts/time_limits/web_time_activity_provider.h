// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"

namespace ash {
class BrowserDelegate;
}

namespace base {
class Time;
class UnguessableToken;
}  // namespace base

namespace ash::app_time {

class AppId;
class AppTimeController;
enum class ChromeAppActivityState;

class WebTimeActivityProvider : public WebTimeNavigationObserver::EventListener,
                                public ash::BrowserController::Observer,
                                public ash::BrowserController::TabObserver,
                                public AppServiceWrapper::EventListener {
 public:
  WebTimeActivityProvider(AppTimeController* app_time_controller,
                          AppServiceWrapper* app_service_wrapper);
  WebTimeActivityProvider(const WebTimeActivityProvider&) = delete;
  WebTimeActivityProvider& operator=(const WebTimeActivityProvider&) = delete;

  ~WebTimeActivityProvider() override;

  // WebTimeNavigationObserver::EventListener:
  void OnWebActivityChanged(
      const WebTimeNavigationObserver::NavigationInfo& info) override;
  void WebTimeNavigationObserverDestroyed(
      WebTimeNavigationObserver* navigation_observer) override;

  // ash::BrowserController::TabObserver:
  void OnTabInserted(ash::BrowserDelegate* browser,
                     content::WebContents* contents) override;
  void OnTabReplaced(ash::BrowserDelegate* browser,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents) override;
  void OnActiveWebContentsChanged(ash::BrowserDelegate* browser,
                                  content::WebContents* old_contents,
                                  content::WebContents* new_contents) override;

  // ash::BrowserController::Observer:
  void OnBrowserClosed(ash::BrowserDelegate* browser) override;

  // AppServiceWrapper::EventListener:
  void OnAppActive(const AppId& app_id,
                   const base::UnguessableToken& instance_id,
                   base::Time timestamp) override;
  void OnAppInactive(const AppId& app_id,
                     const base::UnguessableToken& instance_id,
                     base::Time timestamp) override;

  ChromeAppActivityState chrome_app_activity_state() const {
    return chrome_app_activity_state_;
  }

 private:
  // Notifies AppActivityRegistry if there is a change in ChromeAppState.
  void MaybeNotifyStateChange(base::Time timestamp);

  // Calculates whether the Chrome app is kActive, kActiveAllowlisted or
  // kInactive.
  ChromeAppActivityState CalculateChromeAppActivityState() const;

  // Reference to AppTimeController. Owned by ChildUserService.
  const raw_ptr<AppTimeController> app_time_controller_;

  // A set of active browser instances.
  std::set<const ash::BrowserDelegate*> active_browsers_;

  // The default chrome app activity state.
  ChromeAppActivityState chrome_app_activity_state_ =
      ChromeAppActivityState::kInactive;

  // A map from a navigation observer to its most recently reported navigation
  // info.
  std::map<const WebTimeNavigationObserver*,
           WebTimeNavigationObserver::NavigationInfo>
      navigation_info_map_;

  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::Observer>
      browser_observation_{this};

  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::TabObserver>
      browser_tab_observation_{this};

  base::ScopedObservation<AppServiceWrapper, AppServiceWrapper::EventListener>
      app_service_wrapper_observation_{this};

  base::ScopedMultiSourceObservation<WebTimeNavigationObserver,
                                     WebTimeNavigationObserver::EventListener>
      web_time_navigation_observers_{this};
};

}  // namespace ash::app_time

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_
