// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_METADATA_TABLE_CHROMEOS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_METADATA_TABLE_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

// MetadataTable is a persistent data structure that tracks component usage
// across profiles.
// The instance of this Class lives on UI thread.
class MetadataTable {
 public:
  explicit MetadataTable(PrefService* pref_service);

  MetadataTable(const MetadataTable&) = delete;
  MetadataTable& operator=(const MetadataTable&) = delete;

  ~MetadataTable();

  // Create and return a MetadataTable instance for testing purpose.
  static std::unique_ptr<component_updater::MetadataTable> CreateForTest();

  // Register a Dictionary in PrefService.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Adds or updates a component usage item for current active user.
  bool AddComponentForCurrentUser(const std::string& component_name);

  // Deletes a component usage item for current active user.
  bool DeleteComponentForCurrentUser(const std::string& component_name);

  // Checks if component usage item exists for any user.
  bool HasComponentForAnyUser(const std::string& component_name) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerMetadataTest, Add);
  FRIEND_TEST_ALL_PREFIXES(CrOSComponentInstallerMetadataTest, Delete);

  // Constructor for testing purpose. Access via CreateForTest().
  MetadataTable();

  // Loads |installed_items_| from PrefService.
  void Load();

  // Stores |installed_items_| to PrefService.
  void Store();

  // Add an item to |installed_items_|.
  void AddItem(const std::string& hashed_user_id,
               const std::string& component_name);

  // Delete an item from |installed_items_].
  bool DeleteItem(const std::string& hashed_user_id,
                  const std::string& component_name);

  // Checks if component usage item exists for a user.
  bool HasComponentForUser(const std::string& hashed_user_id,
                           const std::string& component_name) const;

  // Returns the index of an installed item with the given |hashed_user_id| and
  // |component_name|. Returns `installed_items_.size()` if
  // no such item exists.
  size_t GetInstalledItemIndex(const std::string& hashed_user_id,
                               const std::string& component_name) const;

  // Information about installed items.
  base::Value::List installed_items_;

  // Local state PrefService.
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_METADATA_TABLE_CHROMEOS_H_
