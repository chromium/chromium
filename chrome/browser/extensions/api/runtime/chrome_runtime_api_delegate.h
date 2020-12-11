// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class TickClock;
class TimeTicks;
}

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
}

namespace extensions {
class RuntimeAPI;
class UpdateObserver;
}

class ChromeRuntimeAPIDelegate : public extensions::RuntimeAPIDelegate,
                                 public content::NotificationObserver,
                                 public extensions::ExtensionRegistryObserver {
 public:
  explicit ChromeRuntimeAPIDelegate(content::BrowserContext* context);
  ~ChromeRuntimeAPIDelegate() override;

  // Sets a custom TickClock to use in tests.
  static void set_tick_clock_for_tests(const base::TickClock* clock);

 private:
  friend class extensions::RuntimeAPI;

  // extensions::RuntimeAPIDelegate implementation.
  void AddUpdateObserver(extensions::UpdateObserver* observer) override;
  void RemoveUpdateObserver(extensions::UpdateObserver* observer) override;
  void ReloadExtension(const std::string& extension_id) override;
  bool CheckForUpdates(const std::string& extension_id,
                       const UpdateCheckCallback& callback) override;
  void OpenURL(const GURL& uninstall_url) override;
  bool GetPlatformInfo(extensions::api::runtime::PlatformInfo* info) override;
  bool RestartDevice(std::string* error_message) override;
  bool OpenOptionsPage(const extensions::Extension* extension,
                       content::BrowserContext* browser_context) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  void UpdateCheckComplete(const std::string& extension_id);
  void CallUpdateCallbacks(const std::string& extension_id,
                           const UpdateCheckResult& result);

  content::BrowserContext* browser_context_;

  content::NotificationRegistrar registrar_;

  // Whether the API registered with the ExtensionService to receive
  // update notifications.
  bool registered_for_updates_;

  // Map to prevent extensions from getting stuck in reload loops. Maps
  // extension id to the last time it was reloaded and the number of times
  // it was reloaded with not enough time in between reloads.
  std::map<std::string, std::pair<base::TimeTicks, int> > last_reload_time_;

  // Information about update checks, keyed by extension id.
  struct UpdateCheckInfo;
  std::map<std::string, UpdateCheckInfo> update_check_info_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeRuntimeAPIDelegate);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
