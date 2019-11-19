// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;
class SyncedExtensionInstaller;

namespace extensions_helper {

// Returns true iff profiles with indices |index1| and |index2| have the same
// extensions.
bool HasSameExtensions(int index1, int index2) WARN_UNUSED_RESULT;

// Returns true iff the profile with index |index| has the same extensions
// as the verifier.
bool HasSameExtensionsAsVerifier(int index) WARN_UNUSED_RESULT;

// Returns true iff all existing profiles have the same extensions
// as the verifier.
bool AllProfilesHaveSameExtensionsAsVerifier() WARN_UNUSED_RESULT;

// Returns true iff all existing profiles have the same extensions.
bool AllProfilesHaveSameExtensions() WARN_UNUSED_RESULT;

// Installs the extension for the given index to |profile|, and returns the
// extension ID of the new extension.
std::string InstallExtension(Profile* profile, int index);

// Installs the extension for the given index to all profiles (including the
// verifier), and returns the extension ID of the new extension.
std::string InstallExtensionForAllProfiles(int index);

// Uninstalls the extension for the given index from |profile|. Assumes that
// it was previously installed.
void UninstallExtension(Profile* profile, int index);

// Returns a vector containing the indices of all currently installed
// test extensions on |profile|.
std::vector<int> GetInstalledExtensions(Profile* profile);

// Installs all pending synced extensions for |profile|.
void InstallExtensionsPendingForSync(Profile* profile);

// Enables the extension for the given index on |profile|.
void EnableExtension(Profile* profile, int index);

// Disables the extension for the given index on |profile|.
void DisableExtension(Profile* profile, int index);

// Returns true if the extension with index |index| is enabled on |profile|.
bool IsExtensionEnabled(Profile* profile, int index);

// Enables the extension for the given index in incognito mode on |profile|.
void IncognitoEnableExtension(Profile* profile, int index);

// Disables the extension for the given index in incognito mode on |profile|.
void IncognitoDisableExtension(Profile* profile, int index);

// Returns true if the extension with index |index| is enabled in incognito
// mode on |profile|.
bool IsIncognitoEnabled(Profile* profile, int index);

// Runs the message loop until all profiles have same extensions. Returns false
// on timeout.
bool AwaitAllProfilesHaveSameExtensions();

}  // namespace extensions_helper

// A helper class to implement waiting for a set of profiles to have matching
// extensions lists. It waits for calls on both interfaces:
// ExtensionRegistryObserver and NotificationObserver. Observing
// NOTIFICATION_EXTENSION_UPDATING_STARTED notification is needed for tests
// against local server because in such tests extensions are not installed and
// ExtensionRegistryObserver methods are not called.
class ExtensionsMatchChecker : public StatusChangeChecker,
                               public extensions::ExtensionRegistryObserver,
                               public content::NotificationObserver {
 public:
  ExtensionsMatchChecker();
  ~ExtensionsMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  std::vector<Profile*> profiles_;
  std::vector<std::unique_ptr<SyncedExtensionInstaller>>
      synced_extension_installers_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMatchChecker);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
