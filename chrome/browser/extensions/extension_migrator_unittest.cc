// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_migrator.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/macros.h"
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

scoped_refptr<const Extension> CreateExtension(const std::string& id) {
  return ExtensionBuilder("test").SetID(id).Build();
}

}  // namespace

class ExtensionMigratorTest : public ExtensionServiceTestBase {
 public:
  ExtensionMigratorTest() {}
  ~ExtensionMigratorTest() override {}

 protected:
  void InitWithExistingProfile() {
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.is_first_run = false;
    // Create prefs file to make the profile not new.
    const char prefs[] = "{}";
    EXPECT_EQ(int(sizeof(prefs)),
              base::WriteFile(params.pref_file, prefs, sizeof(prefs)));
    InitializeExtensionService(params);
    service()->Init();
    AddMigratorProvider();
  }

  void AddMigratorProvider() {
    service()->AddProviderForTesting(std::make_unique<ExternalProviderImpl>(
        service(), new ExtensionMigrator(profile(), kOldId, kNewId), profile(),
        Manifest::EXTERNAL_PREF, Manifest::EXTERNAL_PREF_DOWNLOAD,
        Extension::FROM_WEBSTORE | Extension::WAS_INSTALLED_BY_DEFAULT));
  }

  void AddExtension(const std::string& id) {
    scoped_refptr<const Extension> fake_app = CreateExtension(id);
    service()->AddExtension(fake_app.get());
  }

  bool HasNewExtension() {
    return service()->pending_extension_manager()->IsIdPending(kNewId) ||
           !!registry()->GetInstalledExtension(kNewId);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionMigratorTest);
};

TEST_F(ExtensionMigratorTest, NoExistingOld) {
  InitWithExistingProfile();
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasNewExtension());
}

TEST_F(ExtensionMigratorTest, HasExistingOld) {
  InitWithExistingProfile();
  AddExtension(kOldId);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasNewExtension());
  EXPECT_TRUE(!!registry()->GetInstalledExtension(kOldId));
}

TEST_F(ExtensionMigratorTest, KeepExistingNew) {
  InitWithExistingProfile();
  AddExtension(kNewId);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(!!registry()->GetInstalledExtension(kNewId));
}

TEST_F(ExtensionMigratorTest, HasBothOldAndNew) {
  InitWithExistingProfile();
  AddExtension(kOldId);
  AddExtension(kNewId);
  service()->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(!!registry()->GetInstalledExtension(kOldId));
  EXPECT_TRUE(!!registry()->GetInstalledExtension(kNewId));
}

}  // namespace extensions
