// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REGISTRY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REGISTRY_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_system_provider/registry_interface.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash::file_system_provider {

// Key names for preferences.
extern const char kPrefKeyFileSystemId[];
extern const char kPrefKeyDisplayName[];
extern const char kPrefKeyWritable[];
extern const char kPrefKeySupportsNotifyTag[];
extern const char kPrefKeyWatchers[];
extern const char kPrefKeyWatcherEntryPath[];
extern const char kPrefKeyWatcherRecursive[];
extern const char kPrefKeyWatcherPersistentOrigins[];
extern const char kPrefKeyWatcherLastTag[];
extern const char kPrefKeyOpenedFilesLimit[];

class ProvidedFileSystemInfo;

// Registers preferences to remember registered file systems between reboots.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Remembers and restores file systems in a persistent storage.
class Registry : public RegistryInterface {
 public:
  explicit Registry(Profile* profile);

  Registry(const Registry&) = delete;
  Registry& operator=(const Registry&) = delete;

  ~Registry() override;

  // RegistryInterface overrides.
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                          const Watchers& watchers) override;
  void ForgetFileSystem(const ProviderId& provider_id,
                        const std::string& file_system_id) override;
  std::unique_ptr<RestoredFileSystems> RestoreFileSystems(
      const ProviderId& provider_id) override;
  void UpdateWatcherTag(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher) override;

 private:
  raw_ptr<Profile> profile_;  // Not owned.
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_REGISTRY_H_
