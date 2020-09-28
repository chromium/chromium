// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LIFETIME_MONITOR_H_
#define APPS_APP_LIFETIME_MONITOR_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace content {
class BrowserContext;
}

namespace apps {

// Observes startup of apps and their windows and notifies observers of these
// events.
class AppLifetimeMonitor : public KeyedService,
                           public content::NotificationObserver,
                           public extensions::AppWindowRegistry::Observer {
 public:
  class Observer {
   public:
    // Called when the app starts running.
    virtual void OnAppStart(content::BrowserContext* context,
                            const std::string& app_id) {}
    // Called when the app becomes active to the user, i.e. the first window
    // becomes visible.
    virtual void OnAppActivated(content::BrowserContext* context,
                                const std::string& app_id) {}
    // Called when the app becomes inactive to the user, i.e. the last window is
    // hidden or closed.
    virtual void OnAppDeactivated(content::BrowserContext* context,
                                  const std::string& app_id) {}
    // Called when the app stops running.
    virtual void OnAppStop(content::BrowserContext* context,
                           const std::string& app_id) {}

   protected:
    virtual ~Observer() = default;
  };

  explicit AppLifetimeMonitor(content::BrowserContext* context);
  AppLifetimeMonitor(const AppLifetimeMonitor&) = delete;
  AppLifetimeMonitor& operator=(const AppLifetimeMonitor&) = delete;
  ~AppLifetimeMonitor() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::AppWindowRegistry::Observer overrides:
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;
  void OnAppWindowHidden(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;

  // KeyedService overrides:
  void Shutdown() override;

  bool HasOtherVisibleAppWindows(extensions::AppWindow* app_window) const;

  void NotifyAppStart(const std::string& app_id);
  void NotifyAppActivated(const std::string& app_id);
  void NotifyAppDeactivated(const std::string& app_id);
  void NotifyAppStop(const std::string& app_id);

  content::NotificationRegistrar registrar_;
  content::BrowserContext* context_;
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace apps

#endif  // APPS_APP_LIFETIME_MONITOR_H_
