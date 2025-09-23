// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync/extension_local_data_batch_uploader.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/extensions/sync/extension_sync_data.h"
#include "chrome/browser/extensions/sync/extension_sync_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace extensions {

class ExtensionLocalDataBatchUploaderTest
    : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    service()->Init();
    ASSERT_TRUE(extension_system()->is_ready());

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

 protected:
  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(profile());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  scoped_refptr<const Extension> LoadExtension(
      const std::string& path,
      bool installed_by_default = false) {
    ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(true);
    if (installed_by_default) {
      extension_loader.add_creation_flag(Extension::WAS_INSTALLED_BY_DEFAULT);
    }
    return extension_loader.LoadExtension(data_dir().AppendASCII(path));
  }

  AccountExtensionTracker::AccountExtensionType GetAccountExtensionType(
      const ExtensionId& extension_id) {
    return AccountExtensionTracker::Get(profile())->GetAccountExtensionType(
        extension_id);
  }

  // Simulates an initial download of sync data (empty for this test) so the
  // extension sync service can start syncing.
  void SimulateInitialSync() {
    ExtensionSyncService::Get(profile())->MergeDataAndStartSyncing(
        syncer::EXTENSIONS, /*initial_sync_data=*/{},
        std::make_unique<syncer::FakeSyncChangeProcessor>());
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

TEST_F(ExtensionLocalDataBatchUploaderTest,
       LocalDataDescriptionEmptyIfNotSyncing) {
  ExtensionLocalDataBatchUploader uploader(profile());

  base::test::TestFuture<syncer::LocalDataDescription> description;
  uploader.GetLocalDataDescription(description.GetCallback());
  EXPECT_THAT(description.Get(), syncer::IsEmptyLocalDataDescription());
}

TEST_F(ExtensionLocalDataBatchUploaderTest,
       LocalDataDescriptionOnlyReturnsUploadableExtensions) {
  // Enable extension syncing in transport mode.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  ExtensionLocalDataBatchUploader uploader(profile());

  // Add an extension that is marked as installed by default. This extension
  // should not be unploadable.
  scoped_refptr<const Extension> installed_by_default_extension =
      LoadExtension("simple_with_file", /*installed_by_default=*/true);
  ASSERT_TRUE(installed_by_default_extension);

  // Add an extension before the user signs in. This extension should be
  // uploadable.
  scoped_refptr<const Extension> uploadable_extension =
      LoadExtension("simple_with_icon");
  ASSERT_TRUE(uploadable_extension);

  // Perform an explicit sign in.
  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());

  // Add an extension after sign in. This extension is associated with the
  // user's account and should not be uploadable.
  scoped_refptr<const Extension> account_extension =
      LoadExtension("simple_with_host");
  ASSERT_TRUE(account_extension);

  base::test::TestFuture<syncer::LocalDataDescription> description_future;
  uploader.GetLocalDataDescription(description_future.GetCallback());

  // Note: fields are checked manually here since the icon is a base 64 and we
  // only care about the icon URL not being enpty.
  const syncer::LocalDataDescription& description = description_future.Get();
  EXPECT_EQ(syncer::DataType::EXTENSIONS, description.type);

  ASSERT_EQ(1u, description.local_data_models.size());
  const syncer::LocalDataItemModel& model = description.local_data_models[0];

  // Check that the one local data model matches `uploadable_extension`.
  const std::string* model_id = std::get_if<std::string>(&model.id);
  ASSERT_TRUE(model_id);
  EXPECT_EQ(uploadable_extension->id(), *model_id);

  EXPECT_EQ(uploadable_extension->name(), model.title);

  const GURL* icon_url =
      std::get_if<syncer::LocalDataItemModel::PageUrlIcon>(&model.icon);
  ASSERT_TRUE(icon_url);
  EXPECT_TRUE(icon_url->SchemeIs(url::kDataScheme));
}

TEST_F(ExtensionLocalDataBatchUploaderTest, TriggerLocalDataMigration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  ExtensionLocalDataBatchUploader uploader(profile());

  // Add an uploadable extension.
  scoped_refptr<const Extension> extension = LoadExtension("simple_with_icon");
  ASSERT_TRUE(extension);

  // Perform an explicit sign in and spin up extensions sync.
  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());
  SimulateInitialSync();

  // Make sure the extension is a local extension that's not syncing.
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(extension->id()));
  {
    syncer::SyncDataList list =
        ExtensionSyncService::Get(profile())->GetAllSyncDataForTesting(
            syncer::EXTENSIONS);
    EXPECT_TRUE(list.empty());
  }

  // Upload the extension to the user's account.
  uploader.TriggerLocalDataMigration();

  // Now the extension should be a syncing account extension.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(extension->id()));
  {
    syncer::SyncDataList list =
        ExtensionSyncService::Get(profile())->GetAllSyncDataForTesting(
            syncer::EXTENSIONS);
    ASSERT_EQ(1u, list.size());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_EQ(extension->id(), data->id());
    EXPECT_TRUE(data->enabled());
  }
}

TEST_F(ExtensionLocalDataBatchUploaderTest, TriggerLocalDataMigrationForItems) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  ExtensionLocalDataBatchUploader uploader(profile());

  // Add two uploadable extensions, though only `extension_1` will be uploaded.
  scoped_refptr<const Extension> extension_1 =
      LoadExtension("simple_with_icon");
  ASSERT_TRUE(extension_1);

  scoped_refptr<const Extension> extension_2 =
      LoadExtension("simple_with_host");
  ASSERT_TRUE(extension_2);

  // Perform an explicit sign in and spin up extensions sync.
  signin_test_util::SimulateExplicitSignIn(profile(), identity_test_env());
  SimulateInitialSync();

  // Upload just `extension_1` to the user's account.
  uploader.TriggerLocalDataMigrationForItems({extension_1->id()});

  // Now only `extension_1` should be a syncing account extension.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(extension_1->id()));
  {
    syncer::SyncDataList list =
        ExtensionSyncService::Get(profile())->GetAllSyncDataForTesting(
            syncer::EXTENSIONS);
    ASSERT_EQ(1u, list.size());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_EQ(extension_1->id(), data->id());
    EXPECT_TRUE(data->enabled());
  }

  // `extension_2` should still be a local extension.
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(extension_2->id()));
}

}  // namespace extensions
