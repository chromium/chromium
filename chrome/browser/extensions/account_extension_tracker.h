// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class BrowserContextKeyedServiceFactory;

namespace extensions {

// This service manages each extension's AccountExtensionType, which describes
// whether the extension is associated with a signed in user's account data.
class AccountExtensionTracker : public KeyedService,
                                public ExtensionRegistryObserver,
                                public signin::IdentityManager::Observer {
 public:
  enum AccountExtensionType {
    // The extension is only associated with the current device. This is used
    // for:
    // - all unsyncable extensions
    // - all extensions if there is no signed in user with sync enabled
    // - all extensions installed before the user signed in, that are not part
    //   the user's account's data.
    kLocal = 0,
    // The extension is part of the signed in user's account data but was
    // installed on this device before the user has signed in.
    kAccountInstalledLocally = 1,
    // The extension is part of the signed in user's account data and was
    // installed on this device after the user has signed in.
    kAccountInstalledSignedIn = 2,
    kLast = 2,
  };

  explicit AccountExtensionTracker(content::BrowserContext* context);

  AccountExtensionTracker(const AccountExtensionTracker&) = delete;
  AccountExtensionTracker& operator=(const AccountExtensionTracker&) = delete;

  ~AccountExtensionTracker() override;

  // Convenience method to get the AccountExtensionTracker for a profile.
  static AccountExtensionTracker* Get(content::BrowserContext* context);

  // Returns the singleton instance of the factory for this KeyedService.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // ExtensionRegistryObserver implementation.
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;

  // IdentityManagerObserver implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Called when sync data is applied for the given `extension_id`.
  void OnExtensionSyncDataApplied(const ExtensionId& extension_id);

  AccountExtensionType GetAccountExtensionTypeForTesting(
      const ExtensionId& extension_id) const;

 private:
  AccountExtensionType GetAccountExtensionType(
      const ExtensionId& extension_id) const;

  // Sets the extension's AccountExtensionType. Called when the extension is
  // installed (not updated) or when there is incoming sync data for the
  // extension, which implies that it's associated with a user's account data.
  void SetAccountExtensionType(const ExtensionId& extension_id,
                               AccountExtensionType type);

  const raw_ptr<content::BrowserContext> browser_context_;

  // ExtensionRegistry observer.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  // IdentityManager observer.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACCOUNT_EXTENSION_TRACKER_H_
