// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/extensions_helper.h"

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_installer.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace extensions_helper {

// Returns a unique extension name based in the integer |index|.
std::string CreateFakeExtensionName(int index) {
  return SyncExtensionHelper::GetInstance()->CreateFakeExtensionName(index);
}

bool HasSameExtensions(int index1, int index2) {
  return SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
      test()->GetProfile(index1), test()->GetProfile(index2));
}

bool HasSameExtensionsAsVerifier(int index) {
  return SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
      test()->GetProfile(index), test()->verifier());
}

bool AllProfilesHaveSameExtensionsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!HasSameExtensionsAsVerifier(i)) {
      LOG(ERROR) << "Profile " << i << " doesn't have the same extensions as"
                                       " the verifier profile.";
      return false;
    }
  }
  return true;
}

bool AllProfilesHaveSameExtensions() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(
        test()->GetProfile(0), test()->GetProfile(i))) {
      LOG(ERROR) << "Profile " << i << " doesnt have the same extensions as"
                                       " profile 0.";
      return false;
    }
  }
  return true;
}


std::string InstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->InstallExtension(
      profile,
      CreateFakeExtensionName(index),
      extensions::Manifest::TYPE_EXTENSION);
}

std::string InstallExtensionForAllProfiles(int index) {
  std::string extension_id;
  for (auto* profile : test()->GetAllProfiles()) {
    extension_id = InstallExtension(profile, index);
  }
  return extension_id;
}

void UninstallExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->UninstallExtension(
      profile, CreateFakeExtensionName(index));
}

std::vector<int> GetInstalledExtensions(Profile* profile) {
  std::vector<int> indices;
  std::vector<std::string> names =
      SyncExtensionHelper::GetInstance()->GetInstalledExtensionNames(profile);
  for (std::vector<std::string>::const_iterator it = names.begin();
       it != names.end(); ++it) {
    int index;
    if (SyncExtensionHelper::GetInstance()->ExtensionNameToIndex(*it, &index)) {
      indices.push_back(index);
    }
  }
  return indices;
}

void EnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->EnableExtension(
      profile, CreateFakeExtensionName(index));
}

void DisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->DisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsExtensionEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsExtensionEnabled(
      profile, CreateFakeExtensionName(index));
}

void IncognitoEnableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoEnableExtension(
      profile, CreateFakeExtensionName(index));
}

void IncognitoDisableExtension(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IncognitoDisableExtension(
      profile, CreateFakeExtensionName(index));
}

bool IsIncognitoEnabled(Profile* profile, int index) {
  return SyncExtensionHelper::GetInstance()->IsIncognitoEnabled(
      profile, CreateFakeExtensionName(index));
}

void InstallExtensionsPendingForSync(Profile* profile) {
  SyncExtensionHelper::GetInstance()->InstallExtensionsPendingForSync(profile);
}

}  // namespace extensions_helper

ExtensionsMatchChecker::ExtensionsMatchChecker()
    : profiles_(test()->GetAllProfiles()) {
  DCHECK_GE(profiles_.size(), 2U);
  for (Profile* profile : profiles_) {
    // Begin mocking the installation of synced extensions from the web store.
    synced_extension_installers_.push_back(
        std::make_unique<SyncedExtensionInstaller>(profile));

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    registry->AddObserver(this);
    registrar_.Add(this, extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
                   content::Source<Profile>(profile));
  }
}

ExtensionsMatchChecker::~ExtensionsMatchChecker() {
  for (Profile* profile : profiles_) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    registry->RemoveObserver(this);
  }
}

bool ExtensionsMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for extensions to match";

  auto it = profiles_.begin();
  Profile* profile0 = *it;
  ++it;
  for (; it != profiles_.end(); ++it) {
    if (!SyncExtensionHelper::GetInstance()->ExtensionStatesMatch(profile0,
                                                                  *it)) {
      return false;
    }
  }
  return true;
}

void ExtensionsMatchChecker::OnExtensionLoaded(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionUnloaded(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CheckExitCondition();
}

void ExtensionsMatchChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED, type);
  CheckExitCondition();
}
