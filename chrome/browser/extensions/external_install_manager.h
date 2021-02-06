// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
}

namespace extensions {
class Extension;
class ExtensionPrefs;
class ExternalInstallError;

class ExternalInstallManager : public ExtensionRegistryObserver,
                               public content::NotificationObserver {
 public:
  ExternalInstallManager(content::BrowserContext* browser_context,
                         bool is_first_run);
  ~ExternalInstallManager() override;

  // Called when the associated profile will be destroyed.
  void Shutdown();

  // Returns true if prompting for external extensions is enabled.
  static bool IsPromptingEnabled();

  // Removes the error associated with a given extension.
  void RemoveExternalInstallError(const std::string& extension_id);

  // Checks if there are any new external extensions to notify the user about.
  void UpdateExternalExtensionAlert();

  // Given a (presumably just-installed) extension id, mark that extension as
  // acknowledged.
  void AcknowledgeExternalExtension(const std::string& extension_id);

  // Notifies the manager that |external_install_error| has changed its alert
  // visibility.
  void DidChangeInstallAlertVisibility(
      ExternalInstallError* external_install_error,
      bool visible);

  bool has_currently_visible_install_alert() {
    return currently_visible_install_alert_ != nullptr;
  }

  ExternalInstallError* currently_visible_install_alert_for_testing() const {
    return currently_visible_install_alert_;
  }

  // Returns a mutable copy of the list of global errors for testing purposes.
  std::vector<ExternalInstallError*> GetErrorsForTesting();

  // Clears the record of shown IDs for testing.
  void ClearShownIdsForTesting();

 private:
  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Adds a global error informing the user that an external extension was
  // installed. If |is_new_profile| is true, then this error is from the first
  // time our profile checked for new extensions.
  void AddExternalInstallError(const Extension* extension, bool is_new_profile);

  // Returns true if this extension is an external one that has yet to be
  // marked as acknowledged.
  bool IsUnacknowledgedExternalExtension(const Extension& extension) const;

  // The associated BrowserContext.
  content::BrowserContext* browser_context_;

  // Whether or not this is the first run for the profile.
  bool is_first_run_;

  // The associated ExtensionPrefs.
  ExtensionPrefs* extension_prefs_;

  // The collection of ExternalInstallErrors.
  std::map<std::string, std::unique_ptr<ExternalInstallError>> errors_;

  // The set of ids of unacknowledged external extensions. Populated at
  // initialization, and then updated as extensions are added, removed,
  // acknowledged, etc.
  std::set<ExtensionId> unacknowledged_ids_;

  // The set of ids of extensions that we have warned about in this session.
  std::set<ExtensionId> shown_ids_;

  // The error that is currently showing an alert dialog/bubble.
  ExternalInstallError* currently_visible_install_alert_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalInstallManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_H_
