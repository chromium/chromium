// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_

#include <map>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class RuntimeAPI;
class UpdateObserver;
}  // namespace extensions

class ChromeRuntimeAPIDelegate : public extensions::RuntimeAPIDelegate,
                                 public extensions::ExtensionRegistryObserver {
 public:
  explicit ChromeRuntimeAPIDelegate(content::BrowserContext* context);

  ChromeRuntimeAPIDelegate(const ChromeRuntimeAPIDelegate&) = delete;
  ChromeRuntimeAPIDelegate& operator=(const ChromeRuntimeAPIDelegate&) = delete;

  ~ChromeRuntimeAPIDelegate() override;

  // Sets a custom TickClock to use in tests.
  static void set_tick_clock_for_tests(const base::TickClock* clock);

 private:
  friend class extensions::RuntimeAPI;

  // extensions::RuntimeAPIDelegate implementation.
  void AddUpdateObserver(extensions::UpdateObserver* observer) override;
  void RemoveUpdateObserver(extensions::UpdateObserver* observer) override;
  void ReloadExtension(const extensions::ExtensionId& extension_id) override;
  bool CheckForUpdates(const extensions::ExtensionId& extension_id,
                       UpdateCheckCallback callback) override;
  void OpenURL(const GURL& uninstall_url) override;
  bool GetPlatformInfo(extensions::api::runtime::PlatformInfo* info) override;
  bool RestartDevice(std::string* error_message) override;
  bool OpenOptionsPage(const extensions::Extension* extension,
                       content::BrowserContext* browser_context) override;
  int GetDeveloperToolsWindowId(
      content::WebContents* developer_tools_web_contents) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  void OnExtensionUpdateFound(const extensions::ExtensionId& extension_id,
                              const base::Version& version);
  void UpdateCheckComplete(const extensions::ExtensionId& extension_id);
  void CallUpdateCallbacks(const extensions::ExtensionId& extension_id,
                           const UpdateCheckResult& result);

  raw_ptr<content::BrowserContext> browser_context_;

  // Whether the API registered with the ExtensionService to receive
  // update notifications.
  bool registered_for_updates_;

  // Map to prevent extensions from getting stuck in reload loops. Maps
  // extension id to the last time it was reloaded and the number of times
  // it was reloaded with not enough time in between reloads.
  std::map<std::string, std::pair<base::TimeTicks, int>> last_reload_time_;

  // Information about update checks, keyed by extension id.
  struct UpdateCheckInfo;
  std::map<std::string, UpdateCheckInfo> update_check_info_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_RUNTIME_CHROME_RUNTIME_API_DELEGATE_H_
