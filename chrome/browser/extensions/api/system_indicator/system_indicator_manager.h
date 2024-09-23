// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "ui/gfx/image/image.h"

class Profile;
class StatusTray;

namespace extensions {
FORWARD_DECLARE_TEST(SystemIndicatorApiTest, SystemIndicatorUnloaded);

class ExtensionIndicatorIcon;

// Keeps track of all the systemIndicator icons created for a given Profile
// that are currently visible in the UI.  Use SystemIndicatorManagerFactory to
// create a SystemIndicatorManager object.
class SystemIndicatorManager : public ExtensionRegistryObserver,
                               public KeyedService {
 public:
  SystemIndicatorManager(Profile* profile, StatusTray* status_tray);

  SystemIndicatorManager(const SystemIndicatorManager&) = delete;
  SystemIndicatorManager& operator=(const SystemIndicatorManager&) = delete;

  ~SystemIndicatorManager() override;

  // Sets the icon of the system indicator for the given |extension| to
  // |icon|.
  void SetSystemIndicatorDynamicIcon(const Extension& extension,
                                     gfx::Image icon);

  // Sets whether the system indicator for the given |extension| is enabled.
  void SetSystemIndicatorEnabled(const Extension& extension, bool is_enabled);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SystemIndicatorApiTest, SystemIndicatorUnloaded);

  // A structure representing the system indicator for an extension.
  struct SystemIndicator {
    SystemIndicator();

    SystemIndicator(const SystemIndicator&) = delete;
    SystemIndicator& operator=(const SystemIndicator&) = delete;

    ~SystemIndicator();

    // A dynamically-set icon (through systemIndicator.setIcon()). Takes
    // precedence over the |default_icon|.
    gfx::Image dynamic_icon;
    // The default system indicator icon specified in the manifest.
    ExtensionIconSet manifest_icon_set;
    // The system tray indicator. This is only non-null if the system indicator
    // is enabled.
    std::unique_ptr<ExtensionIndicatorIcon> system_tray_indicator;
  };

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // Causes a call to OnStatusIconClicked for the specified extension_id.
  // Returns false if no ExtensionIndicatorIcon is found for the extension.
  bool SendClickEventToExtensionForTest(const std::string& extension_id);

  using SystemIndicatorMap = std::map<ExtensionId, SystemIndicator>;

  raw_ptr<Profile> profile_;
  raw_ptr<StatusTray> status_tray_;
  SystemIndicatorMap system_indicators_;
  base::ThreadChecker thread_checker_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_H_
