// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_LACROS_WINDOW_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_LACROS_WINDOW_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"

namespace apps {
struct BrowserWindowInstance;
struct BrowserAppInstance;
}  // namespace apps

namespace ash {
namespace app_restore {

// Created in ash-chrome. Listens to |BrowserAppInstanceRegistry| events and
// calls the app restore interface to save and restore lacros windows.
class LacrosWindowHandler : public apps::BrowserAppInstanceObserver {
 public:
  LacrosWindowHandler(
      apps::BrowserAppInstanceRegistry& browser_app_instance_registry);
  LacrosWindowHandler(const LacrosWindowHandler&) = delete;
  LacrosWindowHandler& operator=(const LacrosWindowHandler&) = delete;
  ~LacrosWindowHandler() override;

  // BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override;
  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override {}
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override {}
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override;
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override {}
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override {}

 private:
  base::ScopedObservation<apps::BrowserAppInstanceRegistry,
                          apps::BrowserAppInstanceObserver>
      browser_app_instance_registry_observation_{this};
};

}  // namespace app_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_LACROS_WINDOW_HANDLER_H_
