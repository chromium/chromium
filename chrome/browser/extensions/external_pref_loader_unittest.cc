// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/test/test_sync_service.h"
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

  TestSyncService(const TestSyncService&) = delete;
  TestSyncService& operator=(const TestSyncService&) = delete;

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
  raw_ptr<syncer::SyncServiceObserver, DanglingUntriaged> observer_ = nullptr;
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

  TestExternalPrefLoader(const TestExternalPrefLoader&) = delete;
  TestExternalPrefLoader& operator=(const TestExternalPrefLoader&) = delete;

  void LoadOnFileThread() override {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(load_callback_));
  }

 private:
  ~TestExternalPrefLoader() override {}
  base::OnceClosure load_callback_;
};

class ExternalPrefLoaderTest : public ::testing::Test {
 public:
  ExternalPrefLoaderTest(ExternalPrefLoaderTest&) = delete;
  ExternalPrefLoaderTest& operator=(ExternalPrefLoaderTest&) = delete;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    sync_service_ = static_cast<TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&TestingSyncFactoryFunction)));
    sync_service_->SetInitialSyncFeatureSetupComplete(true);
  }

  void TearDown() override { profile_.reset(); }

  TestingProfile* profile() { return profile_.get(); }

  TestSyncService* sync_service() { return sync_service_; }

 protected:
  ExternalPrefLoaderTest() = default;
  ~ExternalPrefLoaderTest() override = default;

  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestSyncService, DanglingUntriaged> sync_service_ = nullptr;
};

// TODO(lazyboy): Add a test to cover
// PrioritySyncReadyWaiter::OnIsSyncingChanged().

// Tests that we fire pref reading correctly after priority sync state
// is resolved by ExternalPrefLoader.
TEST_F(ExternalPrefLoaderTest, PrefReadInitiatesCorrectly) {
  base::RunLoop run_loop;
  scoped_refptr<ExternalPrefLoader> loader(
      new TestExternalPrefLoader(profile(), run_loop.QuitWhenIdleClosure()));
  ExternalProviderImpl provider(
      nullptr, loader, profile(), ManifestLocation::kInvalidLocation,
      ManifestLocation::kInvalidLocation, Extension::NO_FLAGS);
  provider.VisitRegisteredExtension();

  // Initially CanSyncFeatureStart() returns true, returning false will let
  // |loader| proceed.
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_FALSE(sync_service()->CanSyncFeatureStart());
  sync_service()->FireOnStateChanged();
  run_loop.Run();
}

}  // namespace extensions
