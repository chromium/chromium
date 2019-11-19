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
#include "chrome/test/base/testing_profile.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
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
  content::BrowserTaskEnvironment task_environment_;
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
  test_service->SetFirstSetupComplete(true);

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
  test_service->FireOnStateChanged();
  run_loop.Run();
}

}  // namespace extensions
