// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_

#include <map>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/active_install_data.h"
#include "chrome/browser/extensions/install_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionPrefs;

class InstallTracker : public KeyedService,
                       public content::NotificationObserver,
                       public ExtensionRegistryObserver {
 public:
  InstallTracker(content::BrowserContext* browser_context,
                 extensions::ExtensionPrefs* prefs);
  ~InstallTracker() override;

  static InstallTracker* Get(content::BrowserContext* context);

  void AddObserver(InstallObserver* observer);
  void RemoveObserver(InstallObserver* observer);

  // If an install is currently in progress for |extension_id|, returns details
  // of the installation. This instance retains ownership of the returned
  // pointer. Returns NULL if the extension is not currently being installed.
  const ActiveInstallData* GetActiveInstall(
      const std::string& extension_id) const;

  // Registers an install initiated by the user to allow checking of duplicate
  // installs. Download of the extension has not necessarily started.
  // RemoveActiveInstall() must be called when install is complete regardless of
  // success or failure. Consider using ScopedActiveInstall rather than calling
  // this directly.
  void AddActiveInstall(const ActiveInstallData& install_data);

  // Deregisters an active install.
  void RemoveActiveInstall(const std::string& extension_id);

  void OnBeginExtensionInstall(
      const InstallObserver::ExtensionInstallParams& params);
  void OnBeginExtensionDownload(const std::string& extension_id);
  void OnDownloadProgress(const std::string& extension_id,
                          int percent_downloaded);
  void OnBeginCrxInstall(const std::string& extension_id);
  void OnFinishCrxInstall(const std::string& extension_id, bool success);
  void OnInstallFailure(const std::string& extension_id);

  // NOTE(limasdf): For extension [un]load and [un]installed, use
  //                ExtensionRegistryObserver.

  // Overriddes for KeyedService.
  void Shutdown() override;

 private:
  void OnAppsReordered();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;

  // Maps extension id to the details of an active install.
  typedef std::map<std::string, ActiveInstallData> ActiveInstallsMap;
  ActiveInstallsMap active_installs_;

  base::ObserverList<InstallObserver>::Unchecked observers_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(InstallTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_H_
