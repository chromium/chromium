// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/test/model/fake_sync_change_processor.h"
#include "components/sync/test/model/sync_error_factory_mock.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

class TestSyncService : public syncer::TestSyncService {
 public:
  TestSyncService() {}
  ~TestSyncService() override {}

  // syncer::SyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    ASSERT_FALSE(observer_);
    observer_ = observer;
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    EXPECT_EQ(observer_, observer);
  }

  void FireOnStateChanged() {
    ASSERT_TRUE(observer_);
    observer_->OnStateChanged(this);
  }

 private:
  syncer::SyncServiceObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>();
}

}  // namespace

// Test version of ExternalPrefLoader that doesn't do any IO.
class TestExternalPrefLoader : public ExternalPrefLoader {
 public:
  TestExternalPrefLoader(Profile* profile, base::OnceClosure load_callback)
      : ExternalPrefLoader(
            // Invalid value, doesn't matter since it's not used.
            -1,
            // Make sure ExternalPrefLoader waits for priority sync.
            ExternalPrefLoader::DELAY_LOAD_UNTIL_PRIORITY_SYNC,
            profile),
        load_callback_(std::move(load_callback)) {}

  void LoadOnFileThread() override {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(load_callback_));
  }

 private:
  ~TestExternalPrefLoader() override {}
  base::OnceClosure load_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalPrefLoader);
};

class ExternalPrefLoaderTest : public testing::Test {
 public:
  ExternalPrefLoaderTest() {}
  ~ExternalPrefLoaderTest() override {}

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    sync_service_ = static_cast<TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
    sync_service_->SetFirstSetupComplete(true);
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  TestSyncService* sync_service() { return sync_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  TestSyncService* sync_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefLoaderTest);
};

// TODO(lazyboy): Add a test to cover
// PrioritySyncReadyWaiter::OnIsSyncingChanged().

// Tests that we fire pref reading correctly after priority sync state
// is resolved by ExternalPrefLoader.
TEST_F(ExternalPrefLoaderTest, PrefReadInitiatesCorrectly) {
  // This test is only relevant pre-SplitSettingsSync.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chromeos::features::kSplitSettingsSync);

  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader(
      new TestExternalPrefLoader(profile(), run_loop.QuitWhenIdleClosure()));
  ExternalProviderImpl provider(
      nullptr, loader, profile(), ManifestLocation::kInvalidLocation,
      ManifestLocation::kInvalidLocation, Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  // Initially CanSyncFeatureStart() returns true, returning false will let
  // |loader| proceed.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  ASSERT_FALSE(sync_service()->CanSyncFeatureStart());
  sync_service()->FireOnStateChanged();
  run_loop.Run();
}

class ExternalPrefLoaderSplitSettingsSyncTest : public ExternalPrefLoaderTest {
 public:
  ExternalPrefLoaderSplitSettingsSyncTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);
  }
  ~ExternalPrefLoaderSplitSettingsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExternalPrefLoaderSplitSettingsSyncTest, OsSyncEnabled) {
  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader =
      base::MakeRefCounted<TestExternalPrefLoader>(
          profile(), run_loop.QuitWhenIdleClosure());
  ExternalProviderImpl provider(
      /*service=*/nullptr, loader, profile(),
      ManifestLocation::kInvalidLocation, ManifestLocation::kInvalidLocation,
      Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  // Simulate OOBE screen completion with OS sync enabled.
  sync_service()->GetUserSettings()->SetOsSyncFeatureEnabled(true);
  prefs->SetBoolean(chromeos::prefs::kSyncOobeCompleted, true);

  // Simulate OS prefs starting to sync.
  sync_preferences::PrefServiceSyncable* pref_sync =
      profile()->GetTestingPrefService();
  // This is an ugly cast, but it's how other tests do it.
  sync_preferences::PrefModelAssociator* pref_sync_service =
      static_cast<sync_preferences::PrefModelAssociator*>(
          pref_sync->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES));
  pref_sync_service->MergeDataAndStartSyncing(
      syncer::OS_PRIORITY_PREFERENCES, syncer::SyncDataList(),
      std::make_unique<syncer::FakeSyncChangeProcessor>(),
      std::make_unique<syncer::SyncErrorFactoryMock>());

  run_loop.Run();
  // |loader| completed loading.
}

TEST_F(ExternalPrefLoaderSplitSettingsSyncTest, OsSyncDisable) {
  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader =
      base::MakeRefCounted<TestExternalPrefLoader>(
          profile(), run_loop.QuitWhenIdleClosure());
  ExternalProviderImpl provider(
      /*service=*/nullptr, loader, profile(),
      ManifestLocation::kInvalidLocation, ManifestLocation::kInvalidLocation,
      Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(chromeos::prefs::kSyncOobeCompleted));

  // Simulate OOBE screen completion with OS sync disabled.
  sync_service()->GetUserSettings()->SetOsSyncFeatureEnabled(false);
  prefs->SetBoolean(chromeos::prefs::kSyncOobeCompleted, true);

  // Loader doesn't need to wait, since OS pref sync is disabled.
  run_loop.Run();
  // |loader| completed loading.
}

TEST_F(ExternalPrefLoaderSplitSettingsSyncTest, SyncDisabledByPolicy) {
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  ASSERT_FALSE(sync_service()->CanSyncFeatureStart());

  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader =
      base::MakeRefCounted<TestExternalPrefLoader>(
          profile(), run_loop.QuitWhenIdleClosure());
  ExternalProviderImpl provider(
      /*service=*/nullptr, loader, profile(),
      ManifestLocation::kInvalidLocation, ManifestLocation::kInvalidLocation,
      Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  // Loader doesn't need to wait, because sync will never enable.
  run_loop.Run();
  // |loader| completed loading.
}

}  // namespace extensions
