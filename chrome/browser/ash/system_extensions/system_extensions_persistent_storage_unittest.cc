// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_persistent_storage.h"

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace ash {

namespace {

constexpr char kPersistedSystemExtensions[] = "system_extensions.persisted";

constexpr char kFirstTypeStr[] = "window-management";
constexpr SystemExtensionId kFirstId = {1, 2, 3, 4};
constexpr char kFirstIdStr[] = "01020304";
constexpr char kFirstName[] = "Sample System Web Extension";
constexpr char kFirstShortName[] = "Sample SWX";
constexpr char kFirstServiceWorkerURL[] = "/sw.js";

constexpr char kSecondTypeStr[] = "peripheral-prototype";
constexpr SystemExtensionId kSecondId = {5, 6, 7, 8};
constexpr char kSecondIdStr[] = "05060708";
constexpr char kSecondName[] = "Second System Extension";
constexpr char kSecondShortName[] = "Second SX";
constexpr char kSecondServiceWorkerURL[] = "/sw.js";

class SystemExtensionsPersistentStorageTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SystemExtensionsPersistentStorageTest() {
    first_test_system_extension_.id = kFirstId;
    first_test_system_extension_.manifest.Set("type", kFirstTypeStr);
    first_test_system_extension_.manifest.Set("id", kFirstIdStr);
    first_test_system_extension_.manifest.Set("name", kFirstName);
    first_test_system_extension_.manifest.Set("short_name", kFirstShortName);
    first_test_system_extension_.manifest.Set("service_worker_url",
                                              kFirstServiceWorkerURL);

    second_test_system_extension_.id = kSecondId;
    second_test_system_extension_.manifest.Set("type", kSecondTypeStr);
    second_test_system_extension_.manifest.Set("id", kSecondIdStr);
    second_test_system_extension_.manifest.Set("name", kSecondName);
    second_test_system_extension_.manifest.Set("short_name", kSecondShortName);
    second_test_system_extension_.manifest.Set("service_worker_url",
                                               kSecondServiceWorkerURL);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SystemExtensionsPersistentStorage::RegisterProfilePrefs(
        profile()->GetTestingPrefService()->registry());
  }

 protected:
  const SystemExtension& first_test_system_extension() {
    return first_test_system_extension_;
  }

  const SystemExtension& second_test_system_extension() {
    return second_test_system_extension_;
  }

  const base::Value::Dict& first_test_manifest() {
    return first_test_system_extension_.manifest;
  }

  const base::Value::Dict& second_test_manifest() {
    return second_test_system_extension_.manifest;
  }

 private:
  SystemExtension first_test_system_extension_;
  SystemExtension second_test_system_extension_;
};

}  // namespace

// Tests that we are writing and removing prefs as we persist and delete
// System Extensions.
TEST_F(SystemExtensionsPersistentStorageTest, WriteAndRemovePrefs) {
  SystemExtensionsPersistentStorage storage(profile());
  storage.Add(first_test_system_extension());

  // Test that the extension was saved into a pref.
  auto* prefs = profile()->GetPrefs();
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetDict(kPersistedSystemExtensions);
    auto* persisted_system_extension =
        persisted_system_extensions_map.FindDict(kFirstIdStr);
    EXPECT_EQ(*persisted_system_extension->FindDict("manifest"),
              first_test_manifest());
  }

  // Test the API returns the correct values.
  absl::optional<SystemExtensionPersistedInfo> persistence_info =
      storage.Get(kFirstId);
  EXPECT_EQ(persistence_info->manifest, first_test_manifest());
  std::vector<SystemExtensionPersistedInfo> persistence_infos =
      storage.GetAll();
  EXPECT_EQ(persistence_infos.size(), 1u);
  EXPECT_EQ(persistence_infos[0].manifest, first_test_manifest());

  storage.Remove(kFirstId);

  // Test that the System Extension was removed from prefs.
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetDict(kPersistedSystemExtensions);
    auto* persisted_system_extension =
        persisted_system_extensions_map.FindDict(kFirstIdStr);
    EXPECT_FALSE(persisted_system_extension);
  }

  // Test that the API no longer returns the System Extension.
  EXPECT_FALSE(storage.Get(kFirstId));
  EXPECT_TRUE(storage.GetAll().empty());
}

TEST_F(SystemExtensionsPersistentStorageTest, WriteAndRemovePrefs_Multiple) {
  SystemExtensionsPersistentStorage storage(profile());
  storage.Add(first_test_system_extension());
  storage.Add(second_test_system_extension());

  // Test that the extension was saved into a pref.
  auto* prefs = profile()->GetPrefs();
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetDict(kPersistedSystemExtensions);

    auto* first_persisted_system_extension =
        persisted_system_extensions_map.FindDict(kFirstIdStr);
    EXPECT_EQ(*first_persisted_system_extension->FindDict("manifest"),
              first_test_manifest());

    auto* second_persisted_system_extension =
        persisted_system_extensions_map.FindDict(kSecondIdStr);
    EXPECT_EQ(*second_persisted_system_extension->FindDict("manifest"),
              second_test_manifest());
  }

  // Test the API returns the correct values.
  {
    std::vector<SystemExtensionPersistedInfo> persistence_infos =
        storage.GetAll();
    EXPECT_EQ(persistence_infos.size(), 2u);

    const SystemExtensionPersistedInfo& first_persistence_info =
        persistence_infos[0].id == kFirstId ? persistence_infos[0]
                                            : persistence_infos[1];
    const SystemExtensionPersistedInfo& second_persistence_info =
        persistence_infos[0].id == kSecondId ? persistence_infos[0]
                                             : persistence_infos[1];

    EXPECT_EQ(first_persistence_info.manifest, first_test_manifest());
    EXPECT_EQ(second_persistence_info.manifest, second_test_manifest());

    EXPECT_EQ(storage.Get(kFirstId)->manifest, first_test_manifest());
    EXPECT_EQ(storage.Get(kSecondId)->manifest, second_test_manifest());
  }

  storage.Remove(kFirstId);

  // Test that the System Extension was removed from prefs.
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetDict(kPersistedSystemExtensions);

    auto* first_persisted_system_extension =
        persisted_system_extensions_map.FindDict(kFirstIdStr);
    EXPECT_FALSE(first_persisted_system_extension);

    // The second System Extension should still be in prefs.
    auto* second_persisted_system_extension =
        persisted_system_extensions_map.FindDict(kSecondIdStr);
    EXPECT_EQ(*second_persisted_system_extension->FindDict("manifest"),
              second_test_manifest());
  }

  // Test that the API no longer returns the first System Extension but still
  // returns the second System Extension.
  {
    std::vector<SystemExtensionPersistedInfo> persistence_infos =
        storage.GetAll();
    EXPECT_EQ(persistence_infos.size(), 1u);
    EXPECT_EQ(persistence_infos[0].manifest, second_test_manifest());

    EXPECT_FALSE(storage.Get(kFirstId));
    EXPECT_EQ(storage.Get(kSecondId)->manifest, second_test_manifest());
  }

  storage.Remove(kSecondId);

  // Test that the last System Extension was removed from prefs.
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetDict(kPersistedSystemExtensions);
    EXPECT_TRUE(persisted_system_extensions_map.empty());
  }

  // Test that the API no longer returns any System Extensions.
  {
    std::vector<SystemExtensionPersistedInfo> persistence_infos =
        storage.GetAll();
    EXPECT_TRUE(persistence_infos.empty());

    EXPECT_FALSE(storage.Get(kFirstId));
    EXPECT_FALSE(storage.Get(kSecondId));
  }
}

// Tests that persisting a System Extension with the same id twice
// overwrites the original System Extension.
TEST_F(SystemExtensionsPersistentStorageTest, PersistTwice) {
  SystemExtensionsPersistentStorage storage(profile());
  storage.Add(first_test_system_extension());

  // The second System Extension has the same id but a different name.
  SystemExtension second_system_extension;
  second_system_extension.id = kFirstId;
  second_system_extension.manifest = first_test_manifest().Clone();
  second_system_extension.manifest.Set("name", "Second System Extension");

  storage.Add(second_system_extension);

  // Test that the saved manifest is the correct one i.e. from the second
  // System Extension.
  absl::optional<SystemExtensionPersistedInfo> persistence_info =
      storage.Get(kFirstId);
  EXPECT_EQ(persistence_info->manifest, second_system_extension.manifest);
}

// Tests deleting a non-existent System Extension doesn't crash.
TEST_F(SystemExtensionsPersistentStorageTest, RemoveNonExistent) {
  SystemExtensionsPersistentStorage storage(profile());
  storage.Remove(kFirstId);
}

// Tests that corrupt ids are ignored when returning values.
TEST_F(SystemExtensionsPersistentStorageTest, CorruptId) {
  SystemExtensionsPersistentStorage storage(profile());

  // Add a System Extension with a corrupted id.
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                kPersistedSystemExtensions);
    base::Value::Dict& persisted_system_extensions_map = update.Get();

    base::Value::Dict corrupted_system_extension;
    corrupted_system_extension.Set("manifest", first_test_manifest().Clone());
    persisted_system_extensions_map.Set("corrupted_id",
                                        std::move(corrupted_system_extension));
  }

  // Check that API calls return no entries, since the only entry is corrupted,
  // and that they don't crash because of the corrupted entry.
  EXPECT_FALSE(storage.Get(kFirstId));
  EXPECT_FALSE(storage.Get(kSecondId));
  EXPECT_TRUE(storage.GetAll().empty());

  // Persist a non-corrupted System Extension. Only the non-corrupted System
  // Extension should be returned since the first one is corrupted.
  storage.Add(second_test_system_extension());

  EXPECT_EQ(storage.Get(kSecondId)->manifest, second_test_manifest());

  auto infos = storage.GetAll();
  EXPECT_EQ(infos.size(), 1u);
  EXPECT_EQ(infos[0].manifest, second_test_manifest());

  storage.Remove(kSecondId);
  EXPECT_FALSE(storage.Get(kSecondId));
  EXPECT_TRUE(storage.GetAll().empty());
}

// Tests that entries with no manifest are ignored when returning values.
TEST_F(SystemExtensionsPersistentStorageTest, NoManifest) {
  SystemExtensionsPersistentStorage storage(profile());

  // Add a System Extension with no manifest.
  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                kPersistedSystemExtensions);

    base::Value::Dict& persisted_system_extensions_map = update.Get();
    persisted_system_extensions_map.Set(kFirstIdStr, base::Value::Dict());
  }

  // Check that the System Extension with no manifest is not returned by
  // the API.
  EXPECT_FALSE(storage.Get(kFirstId));
  EXPECT_FALSE(storage.Get(kSecondId));
  EXPECT_TRUE(storage.GetAll().empty());

  // Persist a System Extension with a manifest. Only the System Extension
  // with a manifest should be returned.
  storage.Add(second_test_system_extension());

  EXPECT_EQ(storage.Get(kSecondId)->manifest, second_test_manifest());

  auto infos = storage.GetAll();
  EXPECT_EQ(infos.size(), 1u);
  EXPECT_EQ(infos[0].manifest, second_test_manifest());

  storage.Remove(kSecondId);
  EXPECT_FALSE(storage.Get(kSecondId));
  EXPECT_TRUE(storage.GetAll().empty());
}

}  // namespace ash
