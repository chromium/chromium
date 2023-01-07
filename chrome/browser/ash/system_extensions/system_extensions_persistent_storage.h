// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PERSISTENT_STORAGE_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PERSISTENT_STORAGE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace ash {

struct SystemExtension;

// Holds persisted information for a System Extension.
struct SystemExtensionPersistedInfo {
  SystemExtensionId id;
  base::Value::Dict manifest;
};

// Manages persisting System Extensions to disk so that they can be loaded at
// startup. This only includes information about System Extension instances, not
// their resources or assets. See `SystemExtensionPersistedInfo`.
class SystemExtensionsPersistentStorage {
 public:
  // Registers prefs used for persisting System Extension information to disk.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SystemExtensionsPersistentStorage(Profile* profile);
  SystemExtensionsPersistentStorage(const SystemExtensionsPersistentStorage&) =
      delete;
  SystemExtensionsPersistentStorage& operator=(
      const SystemExtensionsPersistentStorage&) = delete;
  ~SystemExtensionsPersistentStorage();

  // Stores |system_extension| in persistent storage.
  void Add(const SystemExtension& system_extension);

  // Deletes |system_extension| from persistent storage.
  void Remove(const SystemExtensionId& system_extension_id);

  // Returns the System Extension with |system_extension_id| if it's in
  // persistent storage, or nullopt if it's not.
  absl::optional<SystemExtensionPersistedInfo> Get(
      const SystemExtensionId& system_extension_id);
  std::vector<SystemExtensionPersistedInfo> GetAll();

 private:
  // Safe to hold a pointer because the parent class is owned by Profile.
  raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PERSISTENCE_MANAGER_H_
