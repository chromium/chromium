// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_persistence_manager.h"

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace ash {

namespace {

constexpr char kTestTypeStr[] = "echo";
constexpr SystemExtensionId kTestId = {1, 2, 3, 4};
constexpr char kTestIdStr[] = "01020304";
constexpr char kTestName[] = "Sample System Web Extension";
constexpr char kTestShortName[] = "Sample SWX";
constexpr char kTestServiceWorkerURL[] = "/sw.js";

class SystemExtensionsPersistenceManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SystemExtensionsPersistenceManagerTest() {
    test_system_extension_.id = kTestId;
    test_system_extension_.manifest.Set("type", kTestTypeStr);
    test_system_extension_.manifest.Set("id", kTestIdStr);
    test_system_extension_.manifest.Set("name", kTestName);
    test_system_extension_.manifest.Set("short_name", kTestShortName);
    test_system_extension_.manifest.Set("service_worker_url",
                                        kTestServiceWorkerURL);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SystemExtensionsPersistenceManager::RegisterProfilePrefs(
        profile()->GetTestingPrefService()->registry());
  }

 protected:
  const SystemExtension& test_system_extension() {
    return test_system_extension_;
  }

  const base::Value::Dict& test_manifest() {
    return test_system_extension_.manifest;
  }

 private:
  SystemExtension test_system_extension_;
};

}  // namespace

// Tests that we are writing and removing prefs as we persist and delete
// System Extensions.
TEST_F(SystemExtensionsPersistenceManagerTest, WriteAndRemovePrefs) {
  SystemExtensionsPersistenceManager manager(profile());
  manager.Persist(test_system_extension());

  // Test that the extension was saved into a pref.
  auto* prefs = profile()->GetPrefs();
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetValueDict("system_extensions.persisted");
    auto* persisted_system_extension =
        persisted_system_extensions_map.FindDict(kTestIdStr);
    EXPECT_EQ(*persisted_system_extension->FindDict("manifest"),
              test_manifest());
  }

  // Test the API returns the correct value.
  absl::optional<SystemExtensionPersistenceInfo> persistence_info =
      manager.Get(kTestId);
  EXPECT_EQ(persistence_info->manifest, test_manifest());

  manager.Delete(kTestId);

  // Test that the System Extension was removed from prefs.
  {
    const base::Value::Dict& persisted_system_extensions_map =
        prefs->GetValueDict("system_extensions.persisted");
    auto* persisted_system_extension =
        persisted_system_extensions_map.FindDict(kTestIdStr);
    EXPECT_FALSE(persisted_system_extension);
  }

  // Test that the API no longer returns the System Extension.
  EXPECT_FALSE(manager.Get(kTestId));
}

// Tests that persisting a System Extension with the same id twice
// overwrites the original System Extension.
TEST_F(SystemExtensionsPersistenceManagerTest, PersistTwice) {
  SystemExtensionsPersistenceManager manager(profile());
  manager.Persist(test_system_extension());

  // The second System Extension has the same id but a different name.
  SystemExtension second_system_extension;
  second_system_extension.id = kTestId;
  second_system_extension.manifest = test_manifest().Clone();
  second_system_extension.manifest.Set("name", "Second System Extension");

  manager.Persist(second_system_extension);

  // Test that the saved manifest is the correct one i.e. from the second
  // System Extension.
  absl::optional<SystemExtensionPersistenceInfo> persistence_info =
      manager.Get(kTestId);
  EXPECT_EQ(persistence_info->manifest, second_system_extension.manifest);
}

// Tests deleting a non-existent System Extension doesn't crash.
TEST_F(SystemExtensionsPersistenceManagerTest, RemoveNonExistent) {
  SystemExtensionsPersistenceManager manager(profile());
  manager.Delete(kTestId);
}

}  // namespace ash
