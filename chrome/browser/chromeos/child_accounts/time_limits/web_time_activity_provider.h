// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_

#include <set>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_navigation_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class Time;
}  // namespace base

class Browser;

namespace chromeos {
namespace app_time {

class AppId;
class AppTimeController;
class AppServiceWrapper;
enum class ChromeAppActivityState;

class WebTimeActivityProvider : public WebTimeNavigationObserver::EventListener,
                                public BrowserListObserver,
                                public TabStripModelObserver,
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

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // AppServiceWrapper::EventListener:
  void OnAppActive(const AppId& app_id,
                   aura::Window* window,
                   base::Time timestamp) override;
  void OnAppInactive(const AppId& app_id,
                     aura::Window* window,
                     base::Time timestamp) override;

  ChromeAppActivityState chrome_app_activty_state() const {
    return chrome_app_activity_state_;
  }

 private:
  void TabsInserted(const TabStripModelChange::Insert* insert);

  // Notifies AppActivityRegistry if there is a change in ChromeAppState.
  void MaybeNotifyStateChange(base::Time timestamp);

  // Calculates whether the Chrome app is kActive, kActiveAllowlisted or
  // kInactive.
  ChromeAppActivityState CalculateChromeAppActivityState() const;

  // Reference to AppTimeController. Owned by ChildUserService.
  AppTimeController* const app_time_controller_;

  // Reference to AppServiceWrapper. Owned by AppTimeController.
  AppServiceWrapper* const app_service_wrapper_;

  // The set of navigation observers |this| instance is listening to.
  std::set<WebTimeNavigationObserver*> navigation_observers_;

  // A set of active browser instances.
  std::set<const Browser*> active_browsers_;

  // The default chrome app activity state.
  ChromeAppActivityState chrome_app_activity_state_ =
      ChromeAppActivityState::kInactive;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_WEB_TIME_ACTIVITY_PROVIDER_H_
