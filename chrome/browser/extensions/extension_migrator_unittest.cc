// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_migrator.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

const char kOldId[] = "oooooooooooooooooooooooooooooooo";
const char kNewId[] = "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn";

scoped_refptr<const Extension> CreateExtension(
    const std::string& id,
    mojom::ManifestLocation location) {
  return ExtensionBuilder("test").SetID(id).SetLocation(location).Build();
}

}  // namespace

class ExtensionMigratorTest : public ExtensionServiceTestBase {
 public:
  ExtensionMigratorTest() {}

  ExtensionMigratorTest(const ExtensionMigratorTest&) = delete;
  ExtensionMigratorTest& operator=(const ExtensionMigratorTest&) = delete;

  ~ExtensionMigratorTest() override {}

 protected:
  void InitWithExistingProfile() {
    ExtensionServiceInitParams params;
    // Create prefs file to make the profile not new.
    params.prefs_content = "{}";
    params.is_first_run = false;
    InitializeExtensionService(std::move(params));
    service()->Init();
    AddMigratorProvider();
  }

  void AddMigratorProvider() {
    service()->AddProviderForTesting(std::make_unique<ExternalProviderImpl>(
        service(), new ExtensionMigrator(profile(), kOldId, kNewId), profile(),
        mojom::ManifestLocation::kExternalPref,
        mojom::ManifestLocation::kExternalPrefDownload,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT));
  }

  scoped_refptr<const Extension> AddExtension(
      const std::string& id,
      mojom::ManifestLocation location) {
    scoped_refptr<const Extension> fake_app = CreateExtension(id, location);
    service()->AddExtension(fake_app.get());
    return fake_app;
  }

  bool HasNewExtension() {
    return service()->pending_extension_manager()->IsIdPending(kNewId) ||
           registry()->GetInstalledExtension(kNewId);
  }
};

TEST_F(ExtensionMigratorTest, NoExistingOld) {
  InitWithExistingProfile();
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasNewExtension());
}

TEST_F(ExtensionMigratorTest, HasExistingOld) {
  InitWithExistingProfile();
  AddExtension(kOldId, mojom::ManifestLocation::kExternalPrefDownload);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasNewExtension());
  EXPECT_TRUE(registry()->GetInstalledExtension(kOldId));
}

TEST_F(ExtensionMigratorTest, KeepExistingNew) {
  InitWithExistingProfile();
  AddExtension(kNewId, mojom::ManifestLocation::kExternalPrefDownload);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(registry()->GetInstalledExtension(kNewId));
}

TEST_F(ExtensionMigratorTest, HasBothOldAndNew) {
  InitWithExistingProfile();
  AddExtension(kOldId, mojom::ManifestLocation::kExternalPrefDownload);
  AddExtension(kNewId, mojom::ManifestLocation::kExternalPrefDownload);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(registry()->GetInstalledExtension(kOldId));
  EXPECT_TRUE(registry()->GetInstalledExtension(kNewId));
}

// Tests that a previously-force-installed extension can be uninstalled.
// crbug.com/1416682
TEST_F(ExtensionMigratorTest, HasPreviouslyForceInstalledNew) {
  InitWithExistingProfile();
  scoped_refptr<const Extension> extension =
      AddExtension(kNewId, mojom::ManifestLocation::kExternalPolicyDownload);
  service()->OnExtensionInstalled(extension.get(), syncer::StringOrdinal());
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  // A previously-force-installed-extension should not be persisted by the
  // ExtensionMigrator.
  EXPECT_FALSE(registry()->GetInstalledExtension(kNewId));
}

}  // namespace extensions
