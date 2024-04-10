// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"

#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_utils.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

const PackageId kTestPackageId(PackageType::kArc, "test.package.name");

class PromiseAppRegistryCacheTest : public testing::Test {
 public:
  void SetUp() override {
    cache_ = std::make_unique<PromiseAppRegistryCache>();
  }

  PromiseAppRegistryCache* cache() { return cache_.get(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  std::unique_ptr<PromiseAppRegistryCache> cache_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PromiseAppRegistryCacheTest, AddPromiseAppToCache) {
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  ASSERT_FALSE(cache()->HasPromiseApp(kTestPackageId));
  cache()->OnPromiseApp(std::move(promise_app));
  ASSERT_TRUE(cache()->HasPromiseApp(kTestPackageId));
  histogram_tester().ExpectBucketCount(
      kPromiseAppLifecycleEventHistogram,
      PromiseAppLifecycleEvent::kCreatedInCache, 1);
}

TEST_F(PromiseAppRegistryCacheTest, UpdatePromiseAppProgress) {
  float progress_initial = 0.1;
  float progress_next = 0.9;

  // Check that there aren't any promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  // Pre-register a promise app with no installation progress value.
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));
  EXPECT_FALSE(cache()->GetPromiseApp(kTestPackageId)->progress.has_value());
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 1u);

  // Update the progress value for the correct app and confirm the progress
  // value.
  auto promise_delta = std::make_unique<PromiseApp>(kTestPackageId);
  promise_delta->progress = progress_initial;
  cache()->OnPromiseApp(std::move(promise_delta));
  EXPECT_EQ(cache()->GetPromiseApp(kTestPackageId)->progress, progress_initial);

  // Update the progress value again and check if it is the correct value.
  auto promise_delta_next = std::make_unique<PromiseApp>(kTestPackageId);
  promise_delta_next->progress = progress_next;
  cache()->OnPromiseApp(std::move(promise_delta_next));
  EXPECT_EQ(cache()->GetPromiseApp(kTestPackageId)->progress, progress_next);

  // All these changes should have applied to the same promise app instead
  // of creating new ones.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 1u);
}

TEST_F(PromiseAppRegistryCacheTest, GetAllPromiseApps) {
  // There should be no promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  // Register some promise apps.
  auto package_id_1 = PackageId(PackageType::kArc, "test1");
  auto promise_app_1 = std::make_unique<PromiseApp>(package_id_1);
  cache()->OnPromiseApp(std::move(promise_app_1));

  auto package_id_2 = PackageId(PackageType::kArc, "test2");
  auto promise_app_2 = std::make_unique<PromiseApp>(package_id_2);
  cache()->OnPromiseApp(std::move(promise_app_2));

  // Check that all the promise apps are being retrieved.
  auto promise_app_list = cache()->GetAllPromiseApps();
  EXPECT_EQ(promise_app_list.size(), 2u);
  EXPECT_EQ(promise_app_list[0]->package_id, package_id_1);
  EXPECT_EQ(promise_app_list[1]->package_id, package_id_2);
}

TEST_F(PromiseAppRegistryCacheTest, GetPromiseAppForStringPackageId) {
  // There should be no promise apps registered yet.
  EXPECT_EQ(cache()->GetAllPromiseApps().size(), 0u);

  std::string valid_package_id_1 = "android:something.example.test";
  std::string valid_package_id_2 = "android:other.example.test";
  std::string invalid_package_id = "invalid";
  apps::PackageId package_id =
      PackageId::FromString(valid_package_id_1).value();

  // Register a promise app.
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  cache()->OnPromiseApp(std::move(promise_app));

  // Expect nullptr result for invalid string Package ID or when a Package ID
  // isn't registered.
  EXPECT_FALSE(cache()->GetPromiseAppForStringPackageId(invalid_package_id));
  EXPECT_FALSE(cache()->GetPromiseAppForStringPackageId(valid_package_id_2));

  const PromiseApp* promise_app_result =
      cache()->GetPromiseAppForStringPackageId(valid_package_id_1);
  EXPECT_EQ(promise_app_result->package_id, package_id);
}

TEST_F(PromiseAppRegistryCacheTest, RemoveSuccessfullyInstalledPromiseApp) {
  // Register a promise app.
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app is registered.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Update the promise app with a kSuccess status.
  auto delta = std::make_unique<PromiseApp>(kTestPackageId);
  delta->status = PromiseStatus::kSuccess;
  cache()->OnPromiseApp(std::move(delta));

  // Confirm that the promise app was removed.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
  histogram_tester().ExpectBucketCount(
      kPromiseAppLifecycleEventHistogram,
      PromiseAppLifecycleEvent::kInstallationSucceeded, 1);
}

TEST_F(PromiseAppRegistryCacheTest, RemoveCancelledPromiseApp) {
  // Register a promise app.
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  cache()->OnPromiseApp(std::move(promise_app));

  // Confirm that the promise app is registered.
  EXPECT_TRUE(cache()->HasPromiseApp(kTestPackageId));

  // Update the promise app with a kCancelled status.
  auto delta = std::make_unique<PromiseApp>(kTestPackageId);
  delta->status = PromiseStatus::kCancelled;
  cache()->OnPromiseApp(std::move(delta));

  // Confirm that the promise app was removed.
  EXPECT_FALSE(cache()->HasPromiseApp(kTestPackageId));
  histogram_tester().ExpectBucketCount(
      kPromiseAppLifecycleEventHistogram,
      PromiseAppLifecycleEvent::kInstallationCancelled, 1);
}

class PromiseAppRegistryCacheObserverTest : public testing::Test,
                                            PromiseAppRegistryCache::Observer {
 public:
  void SetUp() override {
    cache_ = std::make_unique<PromiseAppRegistryCache>();
  }

  // apps::PromiseAppRegistryCache::Observer:
  void OnPromiseAppUpdate(const PromiseAppUpdate& update) override {
    EXPECT_EQ(update, *expected_update_);
    on_promise_app_updated_called_ = true;

    if (IsPromiseAppCompleted(update.Status())) {
      on_promise_app_updated_with_completed_status_called_ = true;
    }
    // Verify that the data in promise app registry cache is already updated.
    ASSERT_TRUE(cache()->HasPromiseApp(update.PackageId()));
    const PromiseApp* promise_app_in_cache =
        cache()->GetPromiseApp(update.PackageId());
    EXPECT_EQ(promise_app_in_cache->package_id, update.PackageId());
    EXPECT_EQ(promise_app_in_cache->progress, update.Progress());
    EXPECT_EQ(promise_app_in_cache->status, update.Status());
    EXPECT_EQ(promise_app_in_cache->should_show, update.ShouldShow());
  }

  void OnPromiseAppRemoved(const PackageId& package_id) override {
    on_promise_app_removed_called_ = true;
    EXPECT_FALSE(cache()->HasPromiseApp(package_id));
  }

  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override {
    obs_.Reset();
  }

  void ExpectPromiseAppUpdate(std::unique_ptr<PromiseAppUpdate> update) {
    expected_update_ = std::move(update);
    if (!obs_.IsObserving()) {
      obs_.Observe(cache());
    }
    on_promise_app_updated_called_ = false;
    on_promise_app_updated_with_completed_status_called_ = false;
    on_promise_app_removed_called_ = false;
  }

  bool CheckOnPromiseAppUpdatedCalled() const {
    return on_promise_app_updated_called_;
  }

  bool CheckOnPromiseAppUpdatedWithCompletedStatusCalled() const {
    return on_promise_app_updated_with_completed_status_called_;
  }

  bool CheckOnPromiseAppRemovedCalled() const {
    return on_promise_app_removed_called_;
  }

  PromiseAppRegistryCache* cache() { return cache_.get(); }

 private:
  base::ScopedObservation<PromiseAppRegistryCache,
                          PromiseAppRegistryCache::Observer>
      obs_{this};
  std::unique_ptr<PromiseAppUpdate> expected_update_;
  std::unique_ptr<PromiseAppRegistryCache> cache_;
  bool on_promise_app_updated_called_ = false;
  bool on_promise_app_updated_with_completed_status_called_ = false;
  bool on_promise_app_removed_called_ = false;
};

TEST_F(PromiseAppRegistryCacheObserverTest, OnPromiseAppUpdate_NewPromiseApp) {
  auto promise_app = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app->progress = 0;
  promise_app->name = "Test";
  promise_app->status = PromiseStatus::kPending;
  promise_app->should_show = false;

  ASSERT_FALSE(cache()->HasPromiseApp(kTestPackageId));

  // Check that we get the appropriate update when registering a new promise
  // app.
  ExpectPromiseAppUpdate(
      std::make_unique<PromiseAppUpdate>(nullptr, promise_app.get()));
  cache()->OnPromiseApp(std::move(promise_app));
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_FALSE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_FALSE(CheckOnPromiseAppRemovedCalled());
}

TEST_F(PromiseAppRegistryCacheObserverTest,
       OnPromiseAppUpdate_ModifyPromiseApp) {
  auto promise_app_pending = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_pending->status = PromiseStatus::kPending;
  promise_app_pending->should_show = false;
  ExpectPromiseAppUpdate(
      std::make_unique<PromiseAppUpdate>(nullptr, promise_app_pending.get()));
  cache()->OnPromiseApp(promise_app_pending->Clone());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_FALSE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_FALSE(CheckOnPromiseAppRemovedCalled());

  // Check that we get the appropriate update when going from pending to
  // installing.
  auto promise_app_installing = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_installing->progress = 0.4;
  promise_app_installing->name = "Test";
  promise_app_installing->status = PromiseStatus::kInstalling;
  promise_app_installing->should_show = true;
  ExpectPromiseAppUpdate(std::make_unique<PromiseAppUpdate>(
      promise_app_pending.get(), promise_app_installing.get()));
  EXPECT_FALSE(CheckOnPromiseAppUpdatedCalled());
  cache()->OnPromiseApp(promise_app_installing->Clone());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_FALSE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_FALSE(CheckOnPromiseAppRemovedCalled());

  // Verify that OnPromiseAppRemoved gets called when the promise app gets
  // installed.
  auto promise_app_installed = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_installed->progress = 1.0;
  promise_app_installed->status = PromiseStatus::kSuccess;
  promise_app_installed->should_show = true;
  promise_app_installed->installed_app_id = kTestPackageId.identifier();
  ExpectPromiseAppUpdate(std::make_unique<PromiseAppUpdate>(
      promise_app_installing.get(), promise_app_installed.get()));
  EXPECT_FALSE(CheckOnPromiseAppUpdatedCalled());
  cache()->OnPromiseApp(std::move(promise_app_installed));
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_TRUE(CheckOnPromiseAppRemovedCalled());
}

TEST_F(PromiseAppRegistryCacheObserverTest,
       OnPromiseAppUpdate_CancelPromiseApp) {
  auto promise_app_pending = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_pending->status = PromiseStatus::kPending;
  promise_app_pending->should_show = false;
  ExpectPromiseAppUpdate(
      std::make_unique<PromiseAppUpdate>(nullptr, promise_app_pending.get()));
  cache()->OnPromiseApp(promise_app_pending->Clone());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_FALSE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_FALSE(CheckOnPromiseAppRemovedCalled());

  // Check that we get the appropriate update when going from pending to
  // installing.
  auto promise_app_installing = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_installing->progress = 0.4;
  promise_app_installing->status = PromiseStatus::kInstalling;
  promise_app_installing->should_show = true;
  ExpectPromiseAppUpdate(std::make_unique<PromiseAppUpdate>(
      promise_app_pending.get(), promise_app_installing.get()));
  EXPECT_FALSE(CheckOnPromiseAppUpdatedCalled());
  cache()->OnPromiseApp(promise_app_installing->Clone());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_FALSE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_FALSE(CheckOnPromiseAppRemovedCalled());

  // Verify that OnPromiseAppRemoved gets called when the promise app install
  // gets cancelled.
  auto promise_app_cancelled = std::make_unique<PromiseApp>(kTestPackageId);
  promise_app_cancelled->progress = 1.0;
  promise_app_cancelled->status = PromiseStatus::kCancelled;
  promise_app_cancelled->should_show = true;
  ExpectPromiseAppUpdate(std::make_unique<PromiseAppUpdate>(
      promise_app_installing.get(), promise_app_cancelled.get()));
  EXPECT_FALSE(CheckOnPromiseAppUpdatedCalled());
  cache()->OnPromiseApp(std::move(promise_app_cancelled));
  EXPECT_TRUE(CheckOnPromiseAppUpdatedCalled());
  EXPECT_TRUE(CheckOnPromiseAppUpdatedWithCompletedStatusCalled());
  EXPECT_TRUE(CheckOnPromiseAppRemovedCalled());
}

}  // namespace apps
