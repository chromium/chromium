// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_RESTORE_SERVICE_H_
#define APPS_APP_RESTORE_SERVICE_H_

#include "apps/app_lifetime_monitor.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension_id.h"

namespace extensions {
class Extension;
}

namespace content {
class BrowserContext;
}

namespace apps {

// Tracks what apps need to be restarted when the browser restarts.
class AppRestoreService : public KeyedService,
                          public AppLifetimeMonitor::Observer {
 public:
  // Returns true if apps should be restored on the current platform, given
  // whether this new browser process launched due to a restart.
  static bool ShouldRestoreApps(bool is_browser_restart);

  explicit AppRestoreService(content::BrowserContext* context);
  AppRestoreService(const AppRestoreService&) = delete;
  AppRestoreService& operator=(const AppRestoreService&) = delete;

  // Restart apps that need to be restarted and clear the "running" preference
  // from apps to prevent them being restarted in subsequent restarts.
  void HandleStartup(bool should_restore_apps);

  // Returns whether this extension is running or, at startup, whether it was
  // running when Chrome was last terminated.
  bool IsAppRestorable(const extensions::ExtensionId& extension_id);

  // Called to notify that the application has begun to exit.
  void OnApplicationTerminating();

  static AppRestoreService* Get(content::BrowserContext* context);

 private:
  // AppLifetimeMonitor::Observer.
  void OnAppStart(content::BrowserContext* context,
                  const extensions::ExtensionId& extension_id) override;
  void OnAppActivated(content::BrowserContext* context,
                      const extensions::ExtensionId& extension_id) override;
  void OnAppDeactivated(content::BrowserContext* context,
                        const extensions::ExtensionId& extension_id) override;
  void OnAppStop(content::BrowserContext* context,
                 const extensions::ExtensionId& extension_id) override;

  // KeyedService.
  void Shutdown() override;

  void RecordAppStart(const extensions::ExtensionId& extension_id);
  void RecordAppStop(const extensions::ExtensionId& extension_id);
  void RecordAppActiveState(const extensions::ExtensionId& extension_id,
                            bool is_active);

  void RestoreApp(const extensions::Extension* extension);

  void StartObservingAppLifetime();
  void StopObservingAppLifetime();

  raw_ptr<content::BrowserContext> context_;
};

}  // namespace apps

#endif  // APPS_APP_RESTORE_SERVICE_H_
