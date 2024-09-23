// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

namespace extensions_helper {

// Returns true iff profiles with indices |index1| and |index2| have the same
// extensions.
[[nodiscard]] bool HasSameExtensions(int index1, int index2);

// Returns true iff the profile with index |index| has the same extensions
// as the verifier.
[[nodiscard]] bool HasSameExtensionsAsVerifier(int index);

// Returns true iff all existing profiles have the same extensions
// as the verifier.
[[nodiscard]] bool AllProfilesHaveSameExtensionsAsVerifier();

// Returns true iff all existing profiles have the same extensions.
[[nodiscard]] bool AllProfilesHaveSameExtensions();

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
// extensions lists.
class ExtensionsMatchChecker : public StatusChangeChecker,
                               public extensions::ExtensionRegistryObserver {
 public:
  ExtensionsMatchChecker();

  ExtensionsMatchChecker(const ExtensionsMatchChecker&) = delete;
  ExtensionsMatchChecker& operator=(const ExtensionsMatchChecker&) = delete;

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

 private:
  void OnExtensionUpdatingStarted(Profile* profile);

  std::vector<raw_ptr<Profile, VectorExperimental>> profiles_;

  base::WeakPtrFactory<ExtensionsMatchChecker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_EXTENSIONS_HELPER_H_
