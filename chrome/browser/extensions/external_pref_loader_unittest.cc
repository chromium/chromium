// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestSyncService : public browser_sync::ProfileSyncServiceMock {
 public:
  enum class SyncedTypes { ALL, NONE };

  explicit TestSyncService(Profile* profile)
      : browser_sync::ProfileSyncServiceMock(
            CreateProfileSyncServiceParamsForTest(profile)),
        synced_types_(SyncedTypes::NONE) {}
  ~TestSyncService() override {}

  // FakeSyncService:
  int GetDisableReasons() const override { return disable_reasons_; }
  bool IsFirstSetupComplete() const override { return true; }
  syncer::ModelTypeSet GetActiveDataTypes() const override {
    switch (synced_types_) {
      case SyncedTypes::ALL:
        return syncer::ModelTypeSet::All();
      case SyncedTypes::NONE:
        return syncer::ModelTypeSet();
    }
    NOTREACHED();
    return syncer::ModelTypeSet();
  }
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    ASSERT_FALSE(observer_);
    observer_ = observer;
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    EXPECT_EQ(observer_, observer);
  }

  void SetDisableReasons(int disable_reasons) {
    disable_reasons_ = disable_reasons;
  }

  void FireOnStateChanged(browser_sync::ProfileSyncService* service) {
    ASSERT_TRUE(observer_);
    observer_->OnStateChanged(service);
  }

 private:
  syncer::SyncServiceObserver* observer_ = nullptr;
  int disable_reasons_ = DISABLE_REASON_NONE;

  SyncedTypes synced_types_;
  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestSyncService>(static_cast<Profile*>(context));
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
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
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

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  Profile* profile() { return profile_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefLoaderTest);
};

// TODO(lazyboy): Add a test to cover
// PrioritySyncReadyWaiter::OnIsSyncingChanged().

// Tests that we fire pref reading correctly after priority sync state
// is resolved by ExternalPrefLoader.
TEST_F(ExternalPrefLoaderTest, PrefReadInitiatesCorrectly) {
  TestSyncService* test_service = static_cast<TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&TestingSyncFactoryFunction)));

  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader(
      new TestExternalPrefLoader(profile(), run_loop.QuitWhenIdleClosure()));
  ExternalProviderImpl provider(
      nullptr, loader, profile(), Manifest::INVALID_LOCATION,
      Manifest::INVALID_LOCATION, Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  // Initially CanSyncFeatureStart() returns true, returning false will let
  // |loader| proceed.
  test_service->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  ASSERT_FALSE(test_service->CanSyncFeatureStart());
  test_service->FireOnStateChanged(test_service);
  run_loop.Run();
}

}  // namespace extensions
