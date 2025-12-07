// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/sync/extension_sync_service.h"

#include <stddef.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/managed_installation_mode.h"
#include "chrome/browser/extensions/signin_test_util.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/extensions/sync/extension_sync_data.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_specifics.pb.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/browser/test_blocklist.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permission_set.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::AccountExtensionTracker;
using extensions::AppSorting;
using extensions::ComponentLoader;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSyncData;
using extensions::ExtensionSystem;
using extensions::Manifest;
using extensions::PermissionSet;
using extensions::mojom::ManifestLocation;
using syncer::SyncChange;
using syncer::SyncChangeList;
using testing::Mock;

namespace {

constexpr char kGood0[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr char kGood2[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
constexpr char kGoodCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
constexpr char kPageActionCrx[] = "obcimlgaoabeegjmmpldobjndiealpln";
constexpr char kTheme2Crx[] = "ibcijncamhmjjdodjamgiipcgnnaeagd";

ExtensionSyncData GetDisableSyncData(
    const Extension& extension,
    const base::flat_set<int>& disable_reasons) {
  bool enabled = false;
  bool incognito_enabled = false;
  bool remote_install = false;
  return ExtensionSyncData(extension, enabled, disable_reasons,
                           incognito_enabled, remote_install, GURL());
}

ExtensionSyncData GetEnableSyncData(const Extension& extension) {
  bool enabled = true;
  bool incognito_enabled = false;
  bool remote_install = false;
  return ExtensionSyncData(extension, enabled,
                           /*disable_reasons=*/{}, incognito_enabled,
                           remote_install, GURL());
}

SyncChangeList MakeSyncChangeList(const std::string& id,
                                  const sync_pb::EntitySpecifics& specifics,
                                  SyncChange::SyncChangeType change_type) {
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(id, "Name", specifics);
  return SyncChangeList(1, SyncChange(FROM_HERE, change_type, sync_data));
}



}  // namespace

class ExtensionSyncServiceTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  void MockSyncStartFlare(bool* was_called,
                          syncer::DataType* data_type_passed_in,
                          syncer::DataType data_type) {
    *was_called = true;
    *data_type_passed_in = data_type;
  }

  // Helper to call MergeDataAndStartSyncing with no server data and dummy
  // change processor / error factory.
  void StartSyncing(syncer::DataType type) {
    ASSERT_TRUE(type == syncer::EXTENSIONS || type == syncer::APPS);
    extension_sync_service()->MergeDataAndStartSyncing(
        type, syncer::SyncDataList(),
        std::make_unique<syncer::FakeSyncChangeProcessor>());
  }

  void DisableExtensionFromSync(const Extension& extension,
                                int disable_reasons) {
    ExtensionSyncData disable_extension = GetDisableSyncData(
        extension, {extensions::disable_reason::DISABLE_USER_ACTION});
    SyncChangeList list(
        1, disable_extension.GetSyncChange(SyncChange::ACTION_UPDATE));
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  }

  void EnableExtensionFromSync(const Extension& extension) {
    ExtensionSyncData enable_extension = GetEnableSyncData(extension);
    SyncChangeList list(
        1, enable_extension.GetSyncChange(SyncChange::ACTION_UPDATE));
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  }

 protected:
  // Paths to some of the fake extensions.
  base::FilePath good0_path() {
    return data_dir()
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII(kGood0)
        .AppendASCII("1.0.0.0");
  }

  ExtensionSyncService* extension_sync_service() {
    return ExtensionSyncService::Get(profile());
  }

  ExtensionSystem* extension_system() {
    return ExtensionSystem::Get(profile());
  }

  AccountExtensionTracker::AccountExtensionType GetAccountExtensionType(
      const extensions::ExtensionId& id) {
    return AccountExtensionTracker::Get(profile())->GetAccountExtensionType(id);
  }
};

TEST_F(ExtensionSyncServiceTest, DeferredSyncStartupPreInstalledComponent) {
  InitializeEmptyExtensionService();

  bool flare_was_called = false;
  syncer::DataType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionSyncServiceTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(base::BindRepeating(
      &ExtensionSyncServiceTest::MockSyncStartFlare, factory.GetWeakPtr(),
      &flare_was_called,  // Safe due to WeakPtrFactory scope.
      &triggered_type));  // Safe due to WeakPtrFactory scope.

  // Install a component extension.
  std::string manifest;
  ASSERT_TRUE(base::ReadFileToString(
      good0_path().Append(extensions::kManifestFilename), &manifest));
  ComponentLoader::Get(profile())->Add(manifest, good0_path());
  ASSERT_FALSE(extension_system()->is_ready());
  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  // Extensions added before service is_ready() don't trigger sync startup.
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionSyncServiceTest, DeferredSyncStartupPreInstalledNormal) {
  InitializeGoodInstalledExtensionService();

  bool flare_was_called = false;
  syncer::DataType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionSyncServiceTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(base::BindRepeating(
      &ExtensionSyncServiceTest::MockSyncStartFlare, factory.GetWeakPtr(),
      &flare_was_called,  // Safe due to WeakPtrFactory scope.
      &triggered_type));  // Safe due to WeakPtrFactory scope.

  ASSERT_FALSE(extension_system()->is_ready());
  service()->Init();
  ASSERT_EQ(3u, loaded_extensions().size());
  ASSERT_TRUE(extension_system()->is_ready());

  // Extensions added before service is_ready() don't trigger sync startup.
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionSyncServiceTest, DeferredSyncStartupOnInstall) {
  InitializeEmptyExtensionService();
  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  bool flare_was_called = false;
  syncer::DataType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionSyncServiceTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(base::BindRepeating(
      &ExtensionSyncServiceTest::MockSyncStartFlare, factory.GetWeakPtr(),
      &flare_was_called,  // Safe due to WeakPtrFactory scope.
      &triggered_type));  // Safe due to WeakPtrFactory scope.

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  EXPECT_TRUE(flare_was_called);
  EXPECT_EQ(syncer::EXTENSIONS, triggered_type);

  // Reset.
  flare_was_called = false;
  triggered_type = syncer::UNSPECIFIED;

  // Once sync starts, flare should no longer be invoked.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  path = data_dir().AppendASCII("page_action.crx");
  InstallCRX(path, INSTALL_NEW);
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionSyncServiceTest, DisableExtensionFromSync) {
  // Start the extensions service with one external extension already installed.
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
  InitializeExtensionService(std::move(params));

  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  ASSERT_EQ(3u, loaded_extensions().size());

  // We start enabled.
  const Extension* extension = registry()->enabled_extensions().GetByID(kGood0);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(kGood0));

  // Sync starts up.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Then sync data arrives telling us to disable `kGood0`.
  ExtensionSyncData disable_good_crx(
      *extension, false, {extensions::disable_reason::DISABLE_USER_ACTION},
      false, false, extension_urls::GetWebstoreUpdateUrl());
  SyncChangeList list(
      1, disable_good_crx.GetSyncChange(SyncChange::ACTION_UPDATE));
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  ASSERT_FALSE(registrar()->IsExtensionEnabled(kGood0));
}

// Test that sync can enable and disable installed extensions.
TEST_F(ExtensionSyncServiceTest, ReenableDisabledExtensionFromSync) {
  InitializeEmptyExtensionService();

  service()->Init();

  // Load up a simple extension.
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  scoped_refptr<const Extension> extension = extension_loader.LoadExtension(
      data_dir().AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);
  const std::string kExtensionId = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(kExtensionId));

  syncer::FakeSyncChangeProcessor* processor_raw = nullptr;
  {
    auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
    processor_raw = processor.get();
    extension_sync_service()->MergeDataAndStartSyncing(
        syncer::EXTENSIONS, syncer::SyncDataList(), std::move(processor));
  }
  processor_raw->changes().clear();

  DisableExtensionFromSync(*extension,
                           extensions::disable_reason::DISABLE_USER_ACTION);

  // The extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(kExtensionId));
  EXPECT_THAT(ExtensionPrefs::Get(profile())->GetDisableReasons(kExtensionId),
              testing::UnorderedElementsAre(
                  extensions::disable_reason::DISABLE_USER_ACTION));
  EXPECT_TRUE(processor_raw->changes().empty());

  // Enable the extension. Sync should push the new state.
  registrar()->EnableExtension(kExtensionId);
  {
    ASSERT_EQ(1u, processor_raw->changes().size());
    const SyncChange& change = processor_raw->changes()[0];
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(change.sync_data());
    EXPECT_EQ(kExtensionId, data->id());
    EXPECT_TRUE(data->disable_reasons().empty());
    EXPECT_TRUE(data->enabled());
  }

  // Disable the extension again. Sync should push the new state.
  processor_raw->changes().clear();
  registrar()->DisableExtension(
      kExtensionId, {extensions::disable_reason::DISABLE_USER_ACTION});
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(kExtensionId));
  {
    ASSERT_EQ(1u, processor_raw->changes().size());
    const SyncChange& change = processor_raw->changes()[0];
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(change.sync_data());
    EXPECT_EQ(kExtensionId, data->id());
    EXPECT_THAT(data->disable_reasons(),
                testing::UnorderedElementsAre(
                    extensions::disable_reason::DISABLE_USER_ACTION));
    EXPECT_FALSE(data->enabled());
  }
  processor_raw->changes().clear();

  // Enable the extension via sync.
  EnableExtensionFromSync(*extension);

  // The extension should be enabled.
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kExtensionId));
  EXPECT_TRUE(processor_raw->changes().empty());
}

// Tests that default-installed extensions won't be affected by incoming sync
// data. (It's feasible to have a sync entry for an extension that could be
// default installed, since one installation may be default-installed while
// another may not be).
TEST_F(ExtensionSyncServiceTest,
       DefaultInstalledExtensionsAreNotReenabledOrDisabledBySync) {
  InitializeEmptyExtensionService();

  service()->Init();

  // Load up an extension that's considered default installed.
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  extension_loader.add_creation_flag(Extension::WAS_INSTALLED_BY_DEFAULT);
  scoped_refptr<const Extension> extension = extension_loader.LoadExtension(
      data_dir().AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  // The extension shouldn't sync.
  EXPECT_FALSE(extensions::sync_util::ShouldSync(profile(), extension.get()));
  const std::string kExtensionId = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(kExtensionId));

  syncer::FakeSyncChangeProcessor* processor_raw = nullptr;
  {
    auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
    processor_raw = processor.get();
    extension_sync_service()->MergeDataAndStartSyncing(
        syncer::EXTENSIONS, syncer::SyncDataList(), std::move(processor));
  }
  processor_raw->changes().clear();

  // Sync state says the extension is disabled (e.g. on another machine).
  DisableExtensionFromSync(*extension,
                           extensions::disable_reason::DISABLE_USER_ACTION);

  // The extension should still be enabled, since it's default-installed.
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kExtensionId));
  EXPECT_TRUE(processor_raw->changes().empty());

  // Now disable the extension locally. Sync should *not* push new state.
  registrar()->DisableExtension(
      kExtensionId, {extensions::disable_reason::DISABLE_USER_ACTION});
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(kExtensionId));
  EXPECT_TRUE(processor_raw->changes().empty());

  // Sync state says the extension is enabled.
  EnableExtensionFromSync(*extension);

  // As above, the extension should not have been affected by sync.
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(kExtensionId));
  EXPECT_TRUE(processor_raw->changes().empty());

  // And re-enabling the extension should not push new state to sync.
  registrar()->EnableExtension(kExtensionId);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kExtensionId));
  EXPECT_TRUE(processor_raw->changes().empty());
}

TEST_F(ExtensionSyncServiceTest, IgnoreSyncChangesWhenLocalStateIsMoreRecent) {
  // Start the extension service with three extensions already installed.
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
  InitializeExtensionService(std::move(params));

  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());
  ASSERT_EQ(3u, loaded_extensions().size());

  ASSERT_TRUE(registrar()->IsExtensionEnabled(kGood0));
  ASSERT_TRUE(registrar()->IsExtensionEnabled(kGood2));

  // Disable and re-enable kGood0 before first sync data arrives.
  registrar()->DisableExtension(
      kGood0, {extensions::disable_reason::DISABLE_USER_ACTION});
  ASSERT_FALSE(registrar()->IsExtensionEnabled(kGood0));
  registrar()->EnableExtension(kGood0);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(kGood0));
  // Disable kGood2 before first sync data arrives (good1 is considered
  // non-syncable because it has plugin permission).
  registrar()->DisableExtension(
      kGood2, {extensions::disable_reason::DISABLE_USER_ACTION});
  ASSERT_FALSE(registrar()->IsExtensionEnabled(kGood2));

  const Extension* extension0 =
      registry()->enabled_extensions().GetByID(kGood0);
  const Extension* extension2 =
      registry()->disabled_extensions().GetByID(kGood2);
  ASSERT_TRUE(extensions::sync_helper::IsSyncable(extension0));
  ASSERT_TRUE(extensions::sync_helper::IsSyncable(extension2));

  // Now sync data comes in that says to disable kGood0 and enable kGood2.
  ExtensionSyncData disable_good0(
      *extension0, false, {extensions::disable_reason::DISABLE_USER_ACTION},
      false, false, extension_urls::GetWebstoreUpdateUrl());
  ExtensionSyncData enable_kGood2(*extension2, true,
                                  /*disable_reasons=*/{}, false, false,
                                  extension_urls::GetWebstoreUpdateUrl());
  syncer::SyncDataList sync_data;
  sync_data.push_back(disable_good0.GetSyncData());
  sync_data.push_back(enable_kGood2.GetSyncData());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, sync_data,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Both sync changes should be ignored, since the local state was changed
  // before sync started, and so the local state is considered more recent.
  EXPECT_TRUE(registrar()->IsExtensionEnabled(kGood0));
  EXPECT_FALSE(registrar()->IsExtensionEnabled(kGood2));
}

TEST_F(ExtensionSyncServiceTest, DontSelfNotify) {
  // Start the extension service with three extensions already installed.
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
  InitializeExtensionService(std::move(params));

  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());
  ASSERT_EQ(3u, loaded_extensions().size());
  ASSERT_TRUE(registrar()->IsExtensionEnabled(kGood0));

  syncer::FakeSyncChangeProcessor* processor =
      new syncer::FakeSyncChangeProcessor;
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(), base::WrapUnique(processor));

  processor->changes().clear();

  // Simulate various incoming sync changes, and make sure they don't result in
  // any outgoing changes.

  {
    const Extension* extension =
        registry()->enabled_extensions().GetByID(kGood0);
    ASSERT_TRUE(extension);

    // Disable the extension.
    ExtensionSyncData data(
        *extension, false, {extensions::disable_reason::DISABLE_USER_ACTION},
        false, false, extension_urls::GetWebstoreUpdateUrl());
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension =
        registry()->disabled_extensions().GetByID(kGood0);
    ASSERT_TRUE(extension);

    // Set incognito enabled to true.
    ExtensionSyncData data(*extension, false,
                           /*disable_reasons=*/{}, true, false,
                           extension_urls::GetWebstoreUpdateUrl());
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension =
        registry()->disabled_extensions().GetByID(kGood0);
    ASSERT_TRUE(extension);

    // Add another disable reason.
    ExtensionSyncData data(
        *extension, false,
        {extensions::disable_reason::DISABLE_USER_ACTION,
         extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE},
        false, false, extension_urls::GetWebstoreUpdateUrl());
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension =
        registry()->disabled_extensions().GetByID(kGood0);
    ASSERT_TRUE(extension);

    // Uninstall the extension.
    ExtensionSyncData data(
        *extension, false,
        {extensions::disable_reason::DISABLE_USER_ACTION,
         extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE},
        false, false, extension_urls::GetWebstoreUpdateUrl());
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_DELETE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }
}

TEST_F(ExtensionSyncServiceTest, GetSyncData) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = registry()->GetInstalledExtension(kGoodCrx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 1U);
  std::unique_ptr<ExtensionSyncData> data =
      ExtensionSyncData::CreateFromSyncData(list[0]);
  ASSERT_TRUE(data.get());
  EXPECT_EQ(extension->id(), data->id());
  EXPECT_FALSE(data->uninstalled());
  EXPECT_EQ(registrar()->IsExtensionEnabled(kGoodCrx), data->enabled());
  EXPECT_EQ(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()),
            data->incognito_enabled());
  EXPECT_EQ(data->version(), extension->version());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(extension),
            data->update_url());
}

TEST_F(ExtensionSyncServiceTest, GetSyncDataDisableReasons) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_TRUE(data->disable_reasons().empty());
  }

  // Syncable disable reason, should propagate to sync.
  registrar()->DisableExtension(
      kGoodCrx, {extensions::disable_reason::DISABLE_USER_ACTION});
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_THAT(data->disable_reasons(),
                testing::UnorderedElementsAre(
                    extensions::disable_reason::DISABLE_USER_ACTION));
  }
  registrar()->EnableExtension(kGoodCrx);

  // Non-syncable disable reason. The sync data should still say "enabled".
  registrar()->DisableExtension(kGoodCrx,
                                {extensions::disable_reason::DISABLE_RELOAD});
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_TRUE(data->disable_reasons().empty());
  }
  registrar()->EnableExtension(kGoodCrx);

  // Both a syncable and a non-syncable disable reason, only the former should
  // propagate to sync.
  registrar()->DisableExtension(
      kGoodCrx, {extensions::disable_reason::DISABLE_USER_ACTION,
                 extensions::disable_reason::DISABLE_RELOAD});
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_THAT(data->disable_reasons(),
                testing::UnorderedElementsAre(
                    extensions::disable_reason::DISABLE_USER_ACTION));
  }
  registrar()->EnableExtension(kGoodCrx);
}

TEST_F(ExtensionSyncServiceTest, GetSyncDataTerminated) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(kGoodCrx);
  const Extension* extension = registry()->GetInstalledExtension(kGoodCrx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 1U);
  std::unique_ptr<ExtensionSyncData> data =
      ExtensionSyncData::CreateFromSyncData(list[0]);
  ASSERT_TRUE(data.get());
  EXPECT_EQ(extension->id(), data->id());
  EXPECT_FALSE(data->uninstalled());
  EXPECT_EQ(registrar()->IsExtensionEnabled(kGoodCrx), data->enabled());
  EXPECT_EQ(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()),
            data->incognito_enabled());
  EXPECT_EQ(data->version(), extension->version());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(extension),
            data->update_url());
}

TEST_F(ExtensionSyncServiceTest, GetSyncDataFilter) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = registry()->GetInstalledExtension(kGoodCrx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 0U);
}

TEST_F(ExtensionSyncServiceTest, GetSyncExtensionDataUserSettings) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = registry()->GetInstalledExtension(kGoodCrx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_FALSE(data->incognito_enabled());
  }

  registrar()->DisableExtension(
      kGoodCrx, {extensions::disable_reason::DISABLE_USER_ACTION});
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_FALSE(data->incognito_enabled());
  }

  extensions::util::SetIsIncognitoEnabled(kGoodCrx, profile(), true);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->incognito_enabled());
  }

  registrar()->EnableExtension(kGoodCrx);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->incognito_enabled());
  }
}

TEST_F(ExtensionSyncServiceTest, SyncForUninstalledExternalExtension) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"),
             ManifestLocation::kExternalPref, INSTALL_NEW, Extension::NO_FLAGS);
  const Extension* extension = registry()->GetInstalledExtension(kGoodCrx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  StartSyncing(syncer::APPS);

  UninstallExtension(kGoodCrx);
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsExternalExtensionUninstalled(kGoodCrx));

  sync_pb::EntitySpecifics specifics;
  sync_pb::AppSpecifics* app_specifics = specifics.mutable_app();
  sync_pb::ExtensionSpecifics* extension_specifics =
      app_specifics->mutable_extension();
  extension_specifics->set_id(kGoodCrx);
  extension_specifics->set_version("1.0");
  extension_specifics->set_enabled(true);

  SyncChangeList list =
      MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsExternalExtensionUninstalled(kGoodCrx));
}

TEST_F(ExtensionSyncServiceTest, DontSyncPolicyUninstalls) {
  InitializeEmptyExtensionService();
  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  extensions::StatefulChangeProcessor extensions_processor(
      syncer::DataType::EXTENSIONS);
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      extensions_processor.GetWrapped());

  // 1. Install an extension.
  const Extension* extension =
      InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  // The installation should cause one ADD change.
  ASSERT_EQ(1u, extensions_processor.data().size());
  ASSERT_EQ(1u, extensions_processor.changes().size());
  EXPECT_EQ(SyncChange::ACTION_ADD,
            extensions_processor.changes()[0].change_type());

  // Clear changes to prepare for uninstall check.
  extensions_processor.changes().clear();

  // 2. Uninstall the extension with reason INTERNAL_MANAGEMENT.
  UninstallExtension(extension_id,
                     UninstallExtensionFileDeleteType::kDeleteAllVersions,
                     extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT);
  EXPECT_FALSE(registry()->GetInstalledExtension(extension_id));

  // 3. Verify that no deletion was synced.
  EXPECT_TRUE(extensions_processor.changes().empty());
  EXPECT_EQ(1u, extensions_processor.data().size());
}

#if !BUILDFLAG(IS_ANDROID)
// Disabled on Android since Android does not support Chrome Apps.
TEST_F(ExtensionSyncServiceTest, GetSyncAppDataUserSettings) {
  InitializeEmptyExtensionService();
  const Extension* app =
      PackAndInstallCRX(data_dir().AppendASCII("app"), INSTALL_NEW);
  ASSERT_TRUE(app);
  ASSERT_TRUE(app->is_app());

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    std::unique_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->page_ordinal()));
  }

  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  sorting->SetAppLaunchOrdinal(app->id(), initial_ordinal.CreateAfter());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    std::unique_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(app_sync_data.get());
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->page_ordinal()));
  }

  sorting->SetPageOrdinal(app->id(), initial_ordinal.CreateAfter());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    std::unique_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(app_sync_data.get());
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->page_ordinal()));
  }
}

// TODO (rdevlin.cronin): The OnExtensionMoved() method has been removed from
// ExtensionService, so this test probably needs a new home. Unfortunately, it
// relies pretty heavily on things like InitializeExtension[Sync]Service() and
// PackAndInstallCRX(). When we clean up a bit more, this should move out.
// Disabled on Android since Android does not support Chrome Apps.
TEST_F(ExtensionSyncServiceTest, GetSyncAppDataUserSettingsOnExtensionMoved) {
  InitializeEmptyExtensionService();
  const size_t kAppCount = 3;
  std::array<const Extension*, kAppCount> apps;
  apps[0] = PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  apps[1] = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  apps[2] = PackAndInstallCRX(data_dir().AppendASCII("app4"), INSTALL_NEW);
  for (size_t i = 0; i < kAppCount; ++i) {
    ASSERT_TRUE(apps[i]);
    ASSERT_TRUE(apps[i]->is_app());
  }

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      base::WrapUnique(new syncer::FakeSyncChangeProcessor()));

  extension_system()->app_sorting()->OnExtensionMoved(
      apps[0]->id(), apps[1]->id(), apps[2]->id());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::APPS);
    ASSERT_EQ(list.size(), 3U);

    std::array<std::unique_ptr<ExtensionSyncData>, kAppCount> data;
    for (size_t i = 0; i < kAppCount; ++i) {
      data[i] = ExtensionSyncData::CreateFromSyncData(list[i]);
      ASSERT_TRUE(data[i].get());
    }

    // The sync data is not always in the same order our apps were installed in,
    // so we do that sorting here so we can make sure the values are changed as
    // expected.
    std::array<syncer::StringOrdinal, kAppCount> app_launch_ordinals;
    for (size_t i = 0; i < kAppCount; ++i) {
      for (size_t j = 0; j < kAppCount; ++j) {
        if (apps[i]->id() == data[j]->id()) {
          app_launch_ordinals[i] = data[j]->app_launch_ordinal();
        }
      }
    }

    EXPECT_TRUE(app_launch_ordinals[1].LessThan(app_launch_ordinals[0]));
    EXPECT_TRUE(app_launch_ordinals[0].LessThan(app_launch_ordinals[2]));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ExtensionSyncServiceTest, GetSyncDataList) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("page_action.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("theme.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("theme2.crx"), INSTALL_NEW);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  registrar()->DisableExtension(
      kPageActionCrx, {extensions::disable_reason::DISABLE_USER_ACTION});
  TerminateExtension(kTheme2Crx);

  EXPECT_EQ(
      0u,
      extension_sync_service()->GetAllSyncDataForTesting(syncer::APPS).size());
  EXPECT_EQ(2u, extension_sync_service()
                    ->GetAllSyncDataForTesting(syncer::EXTENSIONS)
                    .size());
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataUninstall) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(kGoodCrx);
  ext_specifics->set_version("1.0");

  SyncChangeList list =
      MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_DELETE);

  // Should do nothing.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(
      registry()->GetExtensionById(kGoodCrx, ExtensionRegistry::EVERYTHING));

  // Install the extension.
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  InstallCRX(extension_path, INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kGoodCrx));

  // Should uninstall the extension.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(
      registry()->GetExtensionById(kGoodCrx, ExtensionRegistry::EVERYTHING));

  // Should again do nothing.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(
      registry()->GetExtensionById(kGoodCrx, ExtensionRegistry::EVERYTHING));
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataWrongType) {
  InitializeEmptyExtensionService();
  StartSyncing(syncer::EXTENSIONS);
  StartSyncing(syncer::APPS);

  // Install the extension.
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  InstallCRX(extension_path, INSTALL_NEW);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kGoodCrx));

  sync_pb::EntitySpecifics specifics;
  sync_pb::AppSpecifics* app_specifics = specifics.mutable_app();
  sync_pb::ExtensionSpecifics* extension_specifics =
      app_specifics->mutable_extension();
  extension_specifics->set_id(kGoodCrx);
  extension_specifics->set_version(
      registry()->GetInstalledExtension(kGoodCrx)->version().GetString());

  {
    extension_specifics->set_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_DELETE);

    // Should do nothing
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(registry()->enabled_extensions().GetByID(kGoodCrx));
  }

  {
    extension_specifics->set_enabled(false);

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    // Should again do nothing.
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(registry()->enabled_extensions().GetByID(kGoodCrx));
  }
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataSettings) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(kGoodCrx);
  ext_specifics->set_version(
      registry()->GetInstalledExtension(kGoodCrx)->version().GetString());
  ext_specifics->set_enabled(false);

  {
    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(registrar()->IsExtensionEnabled(kGoodCrx));
    EXPECT_FALSE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));
  }

  {
    ext_specifics->set_enabled(true);
    ext_specifics->set_incognito_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
    EXPECT_TRUE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));
  }

  {
    ext_specifics->set_enabled(false);
    ext_specifics->set_incognito_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(registrar()->IsExtensionEnabled(kGoodCrx));
    EXPECT_TRUE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));
  }

  {
    ext_specifics->set_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
  }

  EXPECT_FALSE(extensions::PendingExtensionManager::Get(profile())->IsIdPending(
      kGoodCrx));
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataNewExtension) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  const base::FilePath path = data_dir().AppendASCII("good.crx");
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  struct TestCase {
    const char* name;   // For failure output only.
    bool sync_enabled;  // The "enabled" flag coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The disable reason(s) that should be set on the installed extension.
    // This will usually be the same as `sync_disable_reasons`, but see the
    // "Legacy" case.
    extensions::DisableReasonSet expect_disable_reasons;
    // Whether the extension's permissions should be auto-granted during
    // installation.
    bool expect_permissions_granted;
  } test_cases[] = {
      // Standard case: Extension comes in enabled; permissions should be
      // granted
      // during installation.
      {"Standard", true, 0, {}, true},
      // If the extension comes in disabled, its permissions should still be
      // granted (the user already approved them on another machine).
      {"Disabled",
       false,
       extensions::disable_reason::DISABLE_USER_ACTION,
       {extensions::disable_reason::DISABLE_USER_ACTION},
       true},
      // Legacy case (<M45): No disable reasons come in from Sync (see
      // crbug.com/484214). After installation, the reason should be set to
      // DISABLE_USER_ACTION (default assumption).
      {"Legacy",
       false,
       -1,
       {extensions::disable_reason::DISABLE_USER_ACTION},
       true},
      // If the extension came in disabled due to a permissions increase, then
      // the
      // user has *not* approved the permissions, and they shouldn't be granted.
      // crbug.com/484214
      {"PermissionsIncrease",
       false,
       extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE,
       {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE},
       false},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(kGoodCrx);
    ext_specifics->set_version(base::Version("1").GetString());
    ext_specifics->set_enabled(test_case.sync_enabled);
    if (test_case.sync_disable_reasons != -1) {
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);
    }

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);
    extensions::PendingExtensionManager* pending_extension_manager =
        extensions::PendingExtensionManager::Get(profile());

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    ASSERT_TRUE(pending_extension_manager->IsIdPending(kGoodCrx));
    UpdateExtension(kGoodCrx, path,
                    test_case.sync_enabled ? ENABLED : DISABLED);
    EXPECT_EQ(test_case.expect_disable_reasons,
              prefs->GetDisableReasons(kGoodCrx));
    std::unique_ptr<const PermissionSet> permissions =
        prefs->GetGrantedPermissions(kGoodCrx);
    EXPECT_EQ(test_case.expect_permissions_granted, !permissions->IsEmpty());
    ASSERT_FALSE(pending_extension_manager->IsIdPending(kGoodCrx));
    if (test_case.sync_enabled) {
      EXPECT_TRUE(registry()->enabled_extensions().GetByID(kGoodCrx));
    } else {
      EXPECT_TRUE(registry()->disabled_extensions().GetByID(kGoodCrx));
    }

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(kGoodCrx);
  }
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataTerminatedExtension) {
  InitializeExtensionServiceWithUpdater();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(kGoodCrx);
  EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(kGoodCrx);
  ext_specifics->set_version(
      registry()->GetInstalledExtension(kGoodCrx)->version().GetString());
  ext_specifics->set_enabled(false);
  ext_specifics->set_incognito_enabled(true);

  SyncChangeList list =
      MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_TRUE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));

  EXPECT_FALSE(extensions::PendingExtensionManager::Get(profile())->IsIdPending(
      kGoodCrx));
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataVersionCheck) {
  InitializeExtensionServiceWithUpdater();
  auto* updater = extensions::ExtensionUpdater::Get(profile());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(kGoodCrx);
  ext_specifics->set_enabled(true);

  const base::Version installed_version =
      registry()->GetInstalledExtension(kGoodCrx)->version();

  {
    ext_specifics->set_version(installed_version.GetString());

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    // Should do nothing if extension version == sync version.
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(updater->WillCheckSoon());
    // Make sure the version we'll send back to sync didn't change.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    std::unique_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(installed_version, extension_data->version());
  }

  // Should do nothing if extension version > sync version.
  {
    ext_specifics->set_version("0.0.0.0");

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(updater->WillCheckSoon());
    // Make sure the version we'll send back to sync didn't change.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    std::unique_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(installed_version, extension_data->version());
  }

  // Should kick off an update if extension version < sync version.
  {
    const base::Version new_version("9.9.9.9");
    ext_specifics->set_version(new_version.GetString());

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(updater->WillCheckSoon());
    // Make sure that we'll send the NEW version back to sync, even though we
    // haven't actually updated yet. This is to prevent the data in sync from
    // flip-flopping back and forth until all clients are up to date.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    std::unique_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(new_version, extension_data->version());
  }

  EXPECT_FALSE(extensions::PendingExtensionManager::Get(profile())->IsIdPending(
      kGoodCrx));
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataNotInstalled) {
  InitializeExtensionServiceWithUpdater();
  auto* updater = extensions::ExtensionUpdater::Get(profile());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(kGoodCrx);
  ext_specifics->set_enabled(false);
  ext_specifics->set_incognito_enabled(true);
  ext_specifics->set_update_url("http://www.google.com/");
  ext_specifics->set_version("1.2.3.4");

  SyncChangeList list =
      MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

  EXPECT_TRUE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(updater->WillCheckSoon());
  EXPECT_FALSE(registrar()->IsExtensionEnabled(kGoodCrx));
  EXPECT_TRUE(extensions::util::IsIncognitoEnabled(kGoodCrx, profile()));

  const extensions::PendingExtensionInfo* info;
  EXPECT_TRUE(
      (info = extensions::PendingExtensionManager::Get(profile())->GetById(
           kGoodCrx)));
  EXPECT_EQ(ext_specifics->update_url(), info->update_url().spec());
  EXPECT_TRUE(info->is_from_sync());
  EXPECT_EQ(ManifestLocation::kInternal, info->install_source());
  // TODO(akalin): Figure out a way to test `info.ShouldAllowInstall()`.
}

TEST_F(ExtensionSyncServiceTest, ProcessSyncDataEnableDisable) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  constexpr int kUnknownDisableReason_1 =
      (extensions::disable_reason::DISABLE_REASON_LAST << 2);
  constexpr int kUnknownDisableReason_2 =
      (extensions::disable_reason::DISABLE_REASON_LAST << 3);

  struct TestCase {
    const char* name;  // For failure output only.
    // Set of disable reasons before any Sync data comes in. If this is non
    // empty, the extension is disabled. Use a set of raw integers instead of
    // DisableReasonSet because we test for unknown reasons.
    base::flat_set<int> previous_disable_reasons;
    bool sync_enable;  // The enabled flag coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The expected set of disable reasons after processing the Sync update. The
    // extension should be disabled iff this is non empty. Use a set of raw
    // integers instead of DisableReasonSet because we test for unknown reasons.
    base::flat_set<int> expect_disable_reasons;
  } test_cases[] = {
      {/*name=*/"NopEnable",
       /*previous_disable_reasons=*/{},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/0,
       /*expect_disable_reasons=*/{}},
      {/*name=*/"NopDisable",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/extensions::disable_reason::DISABLE_USER_ACTION,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"Enable",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/0,
       /*expect_disable_reasons=*/{}},
      {/*name=*/"Disable",
       /*previous_disable_reasons=*/
       {},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/extensions::disable_reason::DISABLE_USER_ACTION,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"AddDisableReason",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_REMOTE_INSTALL},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/
       extensions::disable_reason::DISABLE_REMOTE_INSTALL |
           extensions::disable_reason::DISABLE_USER_ACTION,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_REMOTE_INSTALL,
        extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"RemoveDisableReason",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_REMOTE_INSTALL,
        extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/extensions::disable_reason::DISABLE_USER_ACTION,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"PreserveLocalDisableReason",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_RELOAD},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/0,
       /*expect_disable_reasons=*/{extensions::disable_reason::DISABLE_RELOAD}},
      {/*name=*/"PreserveOnlyLocalDisableReason",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION,
        extensions::disable_reason::DISABLE_RELOAD},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/0,
       /*expect_disable_reasons=*/{extensions::disable_reason::DISABLE_RELOAD}},

      // The disable reasons not known to the client should be considered as
      // syncable.
      {/*name=*/"UnknownDisableReasons",
       /*previous_disable_reasons=*/

       // Existing disable reasons. We keep one syncable, one local and
       // one unknown one (which should be considered as syncable).
       {extensions::disable_reason::DISABLE_USER_ACTION,
        extensions::disable_reason::DISABLE_RELOAD, kUnknownDisableReason_1},

       // Incoming enabled state.
       false,

       // Incoming reasons via sync. We keep one known syncable reason and one
       // unknown one. The unknown one which is incoming is different from the
       // one which is present locally.
       extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE |
           kUnknownDisableReason_2,

       // Expected reasons after processing the sync update. Local
       // reasons should be preserved. Syncable reasons should be replaced with
       // the incoming ones.
       {extensions::disable_reason::DISABLE_RELOAD,
        extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE,
        kUnknownDisableReason_2}},

      // Interaction with Chrome clients <=M44, which don't sync disable_reasons
      // at all (any existing reasons are preserved).
      {/*name=*/"M44Enable",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/-1,
       /*expect_disable_reasons=*/{}},
      // An M44 client enables an extension that had been disabled on a new
      // client. The disable reasons are still be there, but should be ignored.
      {/*name=*/"M44ReEnable",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/true,
       /*sync_disable_reasons=*/extensions::disable_reason::DISABLE_USER_ACTION,
       /*expect_disable_reasons=*/{}},
      {/*name=*/"M44Disable",
       /*previous_disable_reasons=*/
       {},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/-1,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"M44ReDisable",
       /*previous_disable_reasons=*/
       {},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/0,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/"M44AlreadyDisabledByUser",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/-1,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_USER_ACTION}},
      {/*name=*/
       "M44AlreadyDisabledWithOtherReason",
       /*previous_disable_reasons=*/
       {extensions::disable_reason::DISABLE_REMOTE_INSTALL},
       /*sync_enable=*/false,
       /*sync_disable_reasons=*/-1,
       /*expect_disable_reasons=*/
       {extensions::disable_reason::DISABLE_REMOTE_INSTALL,
        extensions::disable_reason::DISABLE_USER_ACTION}},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    std::string id;
    std::string version;
    // Don't keep `extension` around longer than necessary.
    {
      const Extension* extension =
          InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
      // The extension should now be installed and enabled.
      ASSERT_TRUE(extension);
      id = extension->id();
      version = extension->VersionString();
    }
    ASSERT_TRUE(registry()->enabled_extensions().Contains(id));
    auto passkey =
        extensions::ExtensionPrefs::DisableReasonRawManipulationPasskey();

    // Disable it if the test case says so.
    if (!test_case.previous_disable_reasons.empty()) {
      registrar()->DisableExtensionWithRawReasons(
          passkey, id, test_case.previous_disable_reasons);
      ASSERT_TRUE(registry()->disabled_extensions().Contains(id));
    }

    // Now a sync update comes in.
    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(id);
    ext_specifics->set_enabled(test_case.sync_enable);
    ext_specifics->set_version(version);
    if (test_case.sync_disable_reasons != -1) {
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);
    }

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    // Check expectations.
    const bool expect_enabled = test_case.expect_disable_reasons.empty();
    EXPECT_EQ(expect_enabled, registrar()->IsExtensionEnabled(id));
    EXPECT_EQ(test_case.expect_disable_reasons,
              prefs->GetRawDisableReasons(passkey, id));

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(id);
  }
}

// Test that incoming sync changes (which should be from a signed in user) will
// correctly link an existing extension to the user's account data. This is done
// by checking an extension's AccountExtensionType.
TEST_F(ExtensionSyncServiceTest, AccountExtensionTypeChangesWithSync) {
  InitializeEmptyExtensionService();
  service()->Init();

  auto load_extension = [this](const std::string& extension_path) {
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(true);
    return extension_loader.LoadExtension(
        data_dir().AppendASCII(extension_path));
  };

  // Install two extensions: `first_extension` before a user signs in, and
  // `second_extension` after a user signs in.
  scoped_refptr<const Extension> first_extension =
      load_extension("simple_with_file");
  ASSERT_TRUE(first_extension);
  const std::string first_extension_id = first_extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(first_extension_id));

  // Use a test identity environment to mimic signing a user in with sync
  // enabled.
  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()
      ->MakePrimaryAccountAvailable("testy@mctestface.com",
                                    signin::ConsentLevel::kSync);

  scoped_refptr<const Extension> second_extension =
      load_extension("simple_with_icon");
  ASSERT_TRUE(second_extension);
  const std::string second_extension_id = second_extension->id();

  // After the user has signed in but before any sync data is received,
  // `first_extension` is treated as a local extension and `second_extension` is
  // treated as an account extension since it was installed after sign in.
  // Note that both extensions are syncable.
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(first_extension_id));
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(second_extension_id));

  // Sync starts up.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Then sync data arrives telling us to disable both `first_extension_id` and
  // `second_extension_id`. In practice, any incoming sync will do. Note if
  // incoming sync data contains an extension ID, then that extension is part of
  // a user's account data.
  ExtensionSyncData disable_first_extension(
      *first_extension, false,
      {extensions::disable_reason::DISABLE_USER_ACTION},
      /*incognito_enabled=*/false, /*remote_install=*/false,
      extension_urls::GetWebstoreUpdateUrl());
  ExtensionSyncData disable_second_extension(
      *second_extension, false,
      {extensions::disable_reason::DISABLE_USER_ACTION},
      /*incognito_enabled=*/false, /*remote_install=*/false,
      extension_urls::GetWebstoreUpdateUrl());
  SyncChangeList list;
  list.push_back(
      disable_first_extension.GetSyncChange(SyncChange::ACTION_UPDATE));
  list.push_back(
      disable_second_extension.GetSyncChange(SyncChange::ACTION_UPDATE));

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  ASSERT_FALSE(registrar()->IsExtensionEnabled(first_extension_id));
  ASSERT_FALSE(registrar()->IsExtensionEnabled(second_extension_id));

  // `first_extension` has the AccountExtensionType `kAccountInstalledLocally`
  // since it's part of the signed in user's account data but was first
  // installed on this device before the user has signed in. Note that the
  // incoming sync above links it to the user's account data.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledLocally,
      GetAccountExtensionType(first_extension_id));

  // `second_extension`'s AccountExtensionType should remain unchanged since we
  // already know it's part of the signed in user's account data.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(second_extension_id));
}

class ExtensionSyncServiceCustomGalleryTest : public ExtensionSyncServiceTest {
 public:
  void SetUp() override {
    ExtensionSyncServiceTest::SetUp();

    // This is the update URL specified in the permissions test extension.
    // Setting it here is necessary to make the extension considered syncable.
    extension_test_util::SetGalleryUpdateURL(
        GURL("http://localhost/autoupdate/updates.xml"));
  }
};

TEST_F(ExtensionSyncServiceCustomGalleryTest, ProcessSyncDataDeferredEnable) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as the extension object will be destroyed during updating.
  std::string id = extension->id();

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(id, path, pem_path, DISABLED);

  // Now a sync update comes in, telling us to re-enable a *newer* version.
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_version("3");
  ext_specifics->set_enabled(true);
  ext_specifics->set_disable_reasons(extensions::disable_reason::DISABLE_NONE);

  SyncChangeList list =
      MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  // Since the version didn't match, the extension should still be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));

  // After we update to the matching version, the extension should get enabled.
  path = base_path.AppendASCII("v3");
  PackCRXAndUpdateExtension(id, path, pem_path, ENABLED);
}

TEST_F(ExtensionSyncServiceCustomGalleryTest,
       ProcessSyncDataPermissionApproval) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  const base::FilePath base_path =
      data_dir().AppendASCII("permissions_increase");
  const base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  const base::FilePath path_v1 = base_path.AppendASCII("v1");
  const base::FilePath path_v2 = base_path.AppendASCII("v2");

  base::ScopedTempDir crx_dir;
  ASSERT_TRUE(crx_dir.CreateUniqueTempDir());
  const base::FilePath crx_path_v1 = crx_dir.GetPath().AppendASCII("temp1.crx");
  PackCRX(path_v1, pem_path, crx_path_v1);
  const base::FilePath crx_path_v2 = crx_dir.GetPath().AppendASCII("temp2.crx");
  PackCRX(path_v2, pem_path, crx_path_v2);

  const std::string v1("1");
  const std::string v2("2");

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  struct TestCase {
    const char* name;  // For failure output only.
    const raw_ref<const std::string>
        sync_version;  // The version coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The expected set of disable reasons after processing the Sync update. The
    // extension should be enabled iff this is 0.
    extensions::DisableReasonSet expect_disable_reasons;
    // Whether the extension's permissions should be auto-granted.
    bool expect_permissions_granted;
  } test_cases[] = {
      // Sync tells us to re-enable an older version. No permissions should be
      // granted, since we can't be sure if the user actually approved the right
      // set of permissions.
      {"OldVersion",
       raw_ref(v1),
       0,
       {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE},
       false},
      // Legacy case: Sync tells us to re-enable the extension, but doesn't
      // specify disable reasons. No permissions should be granted.
      {"Legacy",
       raw_ref(v2),
       -1,
       {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE},
       false},
      // Sync tells us to re-enable the extension and explicitly removes the
      // disable reasons. Now the extension should have its permissions granted.
      {"GrantPermissions", raw_ref(v2), 0, {}, true},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    std::string id;
    // Don't keep `extension` around longer than necessary (it'll be destroyed
    // during updating).
    {
      const Extension* extension = InstallCRX(crx_path_v1, INSTALL_NEW);
      // The extension should now be installed and enabled.
      ASSERT_TRUE(extension);
      ASSERT_EQ(v1, extension->VersionString());
      id = extension->id();
    }
    ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

    std::unique_ptr<const PermissionSet> granted_permissions_v1 =
        prefs->GetGrantedPermissions(id);

    // Update to a new version with increased permissions.
    UpdateExtension(id, crx_path_v2, DISABLED);

    // Now the extension should be disabled due to a permissions increase.
    {
      const Extension* extension =
          registry()->disabled_extensions().GetByID(id);
      ASSERT_TRUE(extension);
      ASSERT_EQ(v2, extension->VersionString());
    }
    ASSERT_TRUE(prefs->HasDisableReason(
        id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE));

    // No new permissions should have been granted.
    std::unique_ptr<const PermissionSet> granted_permissions_v2 =
        prefs->GetGrantedPermissions(id);
    ASSERT_EQ(*granted_permissions_v1, *granted_permissions_v2);

    // Now a sync update comes in.
    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(id);
    ext_specifics->set_enabled(true);
    ext_specifics->set_version(*test_case.sync_version);
    if (test_case.sync_disable_reasons != -1) {
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);
    }

    SyncChangeList list =
        MakeSyncChangeList(kGoodCrx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    // Check expectations.
    const bool expect_enabled = test_case.expect_disable_reasons.empty();
    EXPECT_EQ(expect_enabled, registrar()->IsExtensionEnabled(id));
    EXPECT_EQ(test_case.expect_disable_reasons, prefs->GetDisableReasons(id));
    std::unique_ptr<const PermissionSet> granted_permissions =
        prefs->GetGrantedPermissions(id);
    if (test_case.expect_permissions_granted) {
      std::unique_ptr<const PermissionSet> active_permissions =
          prefs->GetDesiredActivePermissions(id);
      EXPECT_EQ(*granted_permissions, *active_permissions);
    } else {
      EXPECT_EQ(*granted_permissions, *granted_permissions_v1);
    }

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(id);
  }
}

#if !BUILDFLAG(IS_ANDROID)
// Disabled on Android since Android does not support themes.
// Regression test for crbug.com/558299
TEST_F(ExtensionSyncServiceTest, DontSyncThemes) {
  InitializeEmptyExtensionService();

  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  syncer::FakeSyncChangeProcessor* processor =
      new syncer::FakeSyncChangeProcessor;
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(), base::WrapUnique(processor));

  processor->changes().clear();

  // Sanity check: Installing an extension should result in a sync change.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, processor->changes().size());

  processor->changes().clear();

  // Installing a theme should not result in a sync change (themes are handled
  // separately by ThemeSyncableService).
  test::ThemeServiceChangedWaiter waiter(
      ThemeServiceFactory::GetForProfile(profile()));
  InstallCRX(data_dir().AppendASCII("theme.crx"), INSTALL_NEW);
  waiter.WaitForThemeChanged();
  EXPECT_TRUE(processor->changes().empty());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests sync behavior in the case of an item that starts out as an app and gets
// updated to become an extension.
TEST_F(ExtensionSyncServiceTest, AppToExtension) {
  InitializeEmptyExtensionService();
  service()->Init();
  ASSERT_TRUE(extension_system()->is_ready());

  // Install v1, which is an app.
  const Extension* v1 =
      InstallCRX(data_dir().AppendASCII("sync_datatypes").AppendASCII("v1.crx"),
                 INSTALL_NEW);
  EXPECT_TRUE(v1->is_app());
  EXPECT_FALSE(v1->is_extension());
  std::string id = v1->id();

  extensions::StatefulChangeProcessor extensions_processor(
      syncer::DataType::EXTENSIONS);
  extensions::StatefulChangeProcessor apps_processor(syncer::DataType::APPS);
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      extensions_processor.GetWrapped());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(), apps_processor.GetWrapped());

  // Check the app/extension change processors to be sure the right data was
  // added.
  EXPECT_TRUE(extensions_processor.changes().empty());
  EXPECT_TRUE(extensions_processor.data().empty());
  EXPECT_EQ(1u, apps_processor.data().size());
  ASSERT_EQ(1u, apps_processor.changes().size());
  const SyncChange& app_change = apps_processor.changes()[0];
  EXPECT_EQ(SyncChange::ACTION_ADD, app_change.change_type());
  std::unique_ptr<ExtensionSyncData> app_data =
      ExtensionSyncData::CreateFromSyncData(app_change.sync_data());
  EXPECT_TRUE(app_data->is_app());
  EXPECT_EQ(id, app_data->id());
  EXPECT_EQ(v1->version(), app_data->version());

  // Update the app to v2, which is an extension.
  const Extension* v2 =
      InstallCRX(data_dir().AppendASCII("sync_datatypes").AppendASCII("v2.crx"),
                 INSTALL_UPDATED);
  EXPECT_FALSE(v2->is_app());
  EXPECT_TRUE(v2->is_extension());
  EXPECT_EQ(id, v2->id());

  // Make sure we saw an extension item added.
  ASSERT_EQ(1u, extensions_processor.changes().size());
  const SyncChange& extension_change = extensions_processor.changes()[0];
  EXPECT_EQ(SyncChange::ACTION_ADD, extension_change.change_type());
  std::unique_ptr<ExtensionSyncData> extension_data =
      ExtensionSyncData::CreateFromSyncData(extension_change.sync_data());
  EXPECT_FALSE(extension_data->is_app());
  EXPECT_EQ(id, extension_data->id());
  EXPECT_EQ(v2->version(), extension_data->version());

  // Get the current data from the change processors to use as the input to
  // the following call to MergeDataAndStartSyncing. This simulates what should
  // happen with sync.
  syncer::SyncDataList extensions_data = extensions_processor.data();
  syncer::SyncDataList apps_data = apps_processor.data();

  // Stop syncing, then start again.
  extension_sync_service()->StopSyncing(syncer::EXTENSIONS);
  extension_sync_service()->StopSyncing(syncer::APPS);
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, extensions_data, extensions_processor.GetWrapped());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, apps_data, apps_processor.GetWrapped());

  // Make sure we saw an app item deleted.
  bool found_delete = false;
  for (const auto& change : apps_processor.changes()) {
    if (change.change_type() == SyncChange::ACTION_DELETE) {
      std::unique_ptr<ExtensionSyncData> data =
          ExtensionSyncData::CreateFromSyncChange(change);
      if (data->id() == id) {
        found_delete = true;
        break;
      }
    }
  }
  EXPECT_TRUE(found_delete);

  // Make sure there is one extension, and there are no more apps.
  EXPECT_EQ(1u, extensions_processor.data().size());
  EXPECT_TRUE(apps_processor.data().empty());
}

class BlocklistedExtensionSyncServiceTest : public ExtensionSyncServiceTest {
 public:
  BlocklistedExtensionSyncServiceTest() = default;

  BlocklistedExtensionSyncServiceTest(
      const BlocklistedExtensionSyncServiceTest&) = delete;
  BlocklistedExtensionSyncServiceTest& operator=(
      const BlocklistedExtensionSyncServiceTest&) = delete;

  void SetUp() override {
    ExtensionSyncServiceTest::SetUp();

    InitializeEmptyExtensionService();

    test_blocklist_.Attach(service()->blocklist_);
    service()->Init();

    // Load up a simple extension.
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(true);
    extension_ = extension_loader.LoadExtension(
        data_dir().AppendASCII("simple_with_file"));
    ASSERT_TRUE(extension_);
    extension_id_ = extension_->id();
    ASSERT_TRUE(registry()->enabled_extensions().GetByID(extension_id_));

    {
      auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
      processor_raw_ = processor.get();
      extension_sync_service()->MergeDataAndStartSyncing(
          syncer::EXTENSIONS, syncer::SyncDataList(), std::move(processor));
    }
    processor_raw_->changes().clear();
  }

  void ForceBlocklistUpdate() {
    service()->OnBlocklistUpdated();
    content::RunAllTasksUntilIdle();
  }

  syncer::FakeSyncChangeProcessor* processor() { return processor_raw_; }

  const Extension* extension() { return extension_.get(); }

  extensions::ExtensionId& extension_id() { return extension_id_; }

  extensions::TestBlocklist& test_blocklist() { return test_blocklist_; }

 private:
  raw_ptr<syncer::FakeSyncChangeProcessor> processor_raw_;
  scoped_refptr<const Extension> extension_;
  extensions::ExtensionId extension_id_;
  extensions::TestBlocklist test_blocklist_;
};

// Test that sync cannot enable blocklisted extensions.
TEST_F(BlocklistedExtensionSyncServiceTest, SyncBlocklistedExtension) {
  // The extension should be syncable before being blocklisted.
  EXPECT_TRUE(extensions::sync_util::ShouldSync(profile(), extension()));

  // Blocklist the extension.
  std::string& extension_id = this->extension_id();
  test_blocklist().SetBlocklistState(extension_id,
                                     extensions::BLOCKLISTED_MALWARE, true);
  ForceBlocklistUpdate();

  // Try enabling the extension via sync.
  EnableExtensionFromSync(*extension());

  // The extension should not be enabled.
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(extension_id));
  EXPECT_TRUE(processor()->changes().empty());

  // Double check that the extension is not syncable.
  EXPECT_FALSE(extensions::sync_util::ShouldSync(profile(), extension()));
}

// Test that some greylisted extensions can be enabled through sync.
TEST_F(BlocklistedExtensionSyncServiceTest, SyncAllowedGreylistedExtension) {
  std::string& extension_id = this->extension_id();

  // Greylist the extension.
  test_blocklist().SetBlocklistState(
      extension_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED, true);
  ForceBlocklistUpdate();

  EXPECT_FALSE(registry()->enabled_extensions().GetByID(extension_id));
  {
    ASSERT_EQ(1u, processor()->changes().size());
    const SyncChange& change = processor()->changes()[0];
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(change.sync_data());
    EXPECT_EQ(extension_id, data->id());
    EXPECT_THAT(
        data->disable_reasons(),
        testing::ElementsAre(extensions::disable_reason::DISABLE_GREYLIST));
    EXPECT_FALSE(data->enabled());
  }
  processor()->changes().clear();

  // Manually re-enabling the extension should work.
  registrar()->EnableExtension(extension_id);
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(extension_id));
  {
    ASSERT_EQ(1u, processor()->changes().size());
    const SyncChange& change = processor()->changes()[0];
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(change.sync_data());
    EXPECT_EQ(extension_id, data->id());
    EXPECT_TRUE(data->disable_reasons().empty());
    EXPECT_TRUE(data->enabled());
  }
  processor()->changes().clear();
}

// Test that blocklisted extension cannot be installed/synchronized.
TEST_F(BlocklistedExtensionSyncServiceTest, InstallBlocklistedExtension) {
  const std::string extension_id = kGoodCrx;
  test_blocklist().SetBlocklistState(extension_id,
                                     extensions::BLOCKLISTED_MALWARE, true);
  ForceBlocklistUpdate();

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);

  ASSERT_TRUE(registry()->GetInstalledExtension(extension_id));
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(extension_id));
  EXPECT_TRUE(registry()->blocklisted_extensions().GetByID(extension_id));
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->GetDisableReasons(extension_id).empty());
  EXPECT_TRUE(processor()->changes().empty());
}

// Users should not be able to sign into transport mode on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
class ExtensionSyncServiceTransportModeTest : public ExtensionSyncServiceTest {
 public:
  ExtensionSyncServiceTransportModeTest() = default;
  ExtensionSyncServiceTransportModeTest(
      const ExtensionSyncServiceTransportModeTest&) = delete;
  ExtensionSyncServiceTransportModeTest& operator=(
      const ExtensionSyncServiceTransportModeTest&) = delete;

  void SetUp() override {
    ExtensionSyncServiceTest::SetUp();
    InitializeEmptyExtensionService();
    service()->Init();

    AccountExtensionTracker::Get(profile());
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

 protected:
  scoped_refptr<const Extension> LoadExtension(
      const std::string& extension_path,
      bool pack_extension) {
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(pack_extension);
    return extension_loader.LoadExtension(
        data_dir().AppendASCII(extension_path));
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
};

// Test that only extensions associated with the signed in user will be synced
// in transport mode.
TEST_F(ExtensionSyncServiceTransportModeTest, OnlySyncAccountExtensions) {
  // Sync starts up.
  syncer::FakeSyncChangeProcessor* processor_raw = nullptr;
  {
    auto processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
    processor_raw = processor.get();
    extension_sync_service()->MergeDataAndStartSyncing(
        syncer::EXTENSIONS, syncer::SyncDataList(), std::move(processor));
  }
  processor_raw->changes().clear();

  // Install two extensions: `first_extension` before a user signs in, and
  // `second_extension` after a user signs in. `second_extension` is associated
  // with the user's account where as `first_extension` is not.
  scoped_refptr<const Extension> first_extension =
      LoadExtension("simple_with_file", /*pack_extension=*/true);
  ASSERT_TRUE(first_extension);
  const std::string first_extension_id = first_extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(first_extension_id));

  // Mimic signing a user into transport mode with syncing for extensions
  // enabled via an explicit sign in.
  extensions::signin_test_util::SimulateExplicitSignIn(profile(),
                                                       identity_test_env());

  scoped_refptr<const Extension> second_extension =
      LoadExtension("simple_with_icon", /*pack_extension=*/true);
  ASSERT_TRUE(second_extension);
  const std::string second_extension_id = second_extension->id();

  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(first_extension_id));
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(second_extension_id));

  // In transport mode, only `second_extension` should sync since it's
  // associated with the user's account.
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncDataForTesting(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_EQ(second_extension_id, data->id());
  }

  // Disable both `first_extension` and `second_extension`. Only
  // `second_extension` should be captured in the sync state to be pushed.
  processor_raw->changes().clear();
  registrar()->DisableExtension(
      first_extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(first_extension_id));

  registrar()->DisableExtension(
      second_extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(second_extension_id));
  {
    ASSERT_EQ(1u, processor_raw->changes().size());
    const SyncChange& change = processor_raw->changes()[0];
    EXPECT_EQ(SyncChange::ACTION_UPDATE, change.change_type());
    std::unique_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(change.sync_data());
    EXPECT_EQ(second_extension_id, data->id());
    EXPECT_THAT(
        data->disable_reasons(),
        testing::ElementsAre(extensions::disable_reason::DISABLE_USER_ACTION));
    EXPECT_FALSE(data->enabled());
  }
}

// Test that local changes made to an extension before initial sync data is
// received is counted as more recent, but the account extension state should
// still be updated.
TEST_F(ExtensionSyncServiceTransportModeTest,
       OnlyUpdateAccountExtensionTypeWhenLocalStateIsMoreRecent) {
  scoped_refptr<const Extension> first_extension =
      LoadExtension("simple_with_file", /*pack_extension=*/true);
  ASSERT_TRUE(first_extension);
  const std::string first_extension_id = first_extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(first_extension_id));

  // Mimic signing a user into transport mode with syncing for extensions
  // enabled via an explicit sign in.
  extensions::signin_test_util::SimulateExplicitSignIn(profile(),
                                                       identity_test_env());

  // Disable and re-enable `first_extension` before first sync data arrives.
  registrar()->DisableExtension(
      first_extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
  ASSERT_FALSE(registrar()->IsExtensionEnabled(first_extension_id));
  registrar()->EnableExtension(first_extension_id);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(first_extension_id));

  // After the user has signed in but before any sync data is received,
  // `first_extension` is treated as a local extension.
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(first_extension_id));

  // Sync starts up. Initial data arrives telling us to disable
  // `first_extension_id`. However, local changes (disabling and re-enabling) is
  // considered more recent so the extension should ignore this change.
  ExtensionSyncData disable_first_extension(
      *first_extension, false,
      {extensions::disable_reason::DISABLE_USER_ACTION},
      /*incognito_enabled=*/false, /*remote_install=*/false,
      extension_urls::GetWebstoreUpdateUrl());

  syncer::SyncDataList list;
  list.push_back(disable_first_extension.GetSyncData());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, list,
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  ASSERT_TRUE(registrar()->IsExtensionEnabled(first_extension_id));

  // `first_extension` has the AccountExtensionType `kAccountInstalledLocally`
  // since it's part of the signed in user's account data, despite having its
  // local state take precedence over the incoming sync state.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledLocally,
      GetAccountExtensionType(first_extension_id));
}

// Same test as ExtensionSyncServiceTest version, to test that local extensions
// get promoted to account extensions from incoming sync data.
TEST_F(ExtensionSyncServiceTransportModeTest,
       AccountExtensionTypeChangesWithSync) {
  // Install two extensions: `first_extension` before a user signs in, and
  // `second_extension` after a user signs in.
  scoped_refptr<const Extension> first_extension =
      LoadExtension("simple_with_file", /*pack_extension=*/true);
  ASSERT_TRUE(first_extension);
  const std::string first_extension_id = first_extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(first_extension_id));

  // Mimic signing a user into transport mode with syncing for extensions
  // enabled via an explicit sign in.
  extensions::signin_test_util::SimulateExplicitSignIn(profile(),
                                                       identity_test_env());

  scoped_refptr<const Extension> second_extension =
      LoadExtension("simple_with_icon", /*pack_extension=*/true);
  ASSERT_TRUE(second_extension);
  const std::string second_extension_id = second_extension->id();

  // After the user has signed in but before any sync data is received,
  // `first_extension` is treated as a local extension and `second_extension` is
  // treated as an account extension since it was installed after sign in.
  // Note that both extensions are syncable.
  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(first_extension_id));
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(second_extension_id));

  // Sync starts up.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>());

  // Then sync data arrives telling us to disable both `first_extension_id` and
  // `second_extension_id`. In practice, any incoming sync will do. Note if
  // incoming sync data contains an extension ID, then that extension is part of
  // a user's account data.
  ExtensionSyncData disable_first_extension(
      *first_extension, false,
      {extensions::disable_reason::DISABLE_USER_ACTION},
      /*incognito_enabled=*/false, /*remote_install=*/false,
      extension_urls::GetWebstoreUpdateUrl());
  ExtensionSyncData disable_second_extension(
      *second_extension, false,
      {extensions::disable_reason::DISABLE_USER_ACTION},
      /*incognito_enabled=*/false, /*remote_install=*/false,
      extension_urls::GetWebstoreUpdateUrl());
  SyncChangeList list;
  list.push_back(
      disable_first_extension.GetSyncChange(SyncChange::ACTION_UPDATE));
  list.push_back(
      disable_second_extension.GetSyncChange(SyncChange::ACTION_UPDATE));

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  ASSERT_FALSE(registrar()->IsExtensionEnabled(first_extension_id));
  ASSERT_FALSE(registrar()->IsExtensionEnabled(second_extension_id));

  // `first_extension` has the AccountExtensionType `kAccountInstalledLocally`
  // since it's part of the signed in user's account data but was first
  // installed on this device before the user has signed in. Note that the
  // incoming sync above links it to the user's account data.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledLocally,
      GetAccountExtensionType(first_extension_id));

  // `second_extension`'s AccountExtensionType should remain unchanged since we
  // already know it's part of the signed in user's account data.
  EXPECT_EQ(
      AccountExtensionTracker::AccountExtensionType::kAccountInstalledSignedIn,
      GetAccountExtensionType(second_extension_id));

  // Install a third, unsyncable extension. Should be a local extension.
  scoped_refptr<const Extension> third_extension =
      LoadExtension("simple_with_host", /*pack_extension=*/false);
  ASSERT_TRUE(third_extension);
  const std::string third_extension_id = third_extension->id();

  EXPECT_EQ(AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(third_extension_id));

  // Verify that only the second extension counts as a signed in account
  // extension.
  std::vector<const Extension*> signed_in_account_extensions =
      AccountExtensionTracker::Get(profile())->GetSignedInAccountExtensions();
  EXPECT_THAT(signed_in_account_extensions,
              ::testing::ElementsAre(second_extension.get()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
