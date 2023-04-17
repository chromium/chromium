// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"

#include <sstream>

#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class PromiseAppRegistryCacheTest : public testing::Test {
 public:
  PromiseAppRegistryCache& cache() { return cache_; }

  bool IsPromiseAppRegistered(const PackageId& package_id) {
    return cache_.promise_app_map_.contains(package_id);
  }

  int CountPromiseAppsRegistered() { return cache_.promise_app_map_.size(); }

 private:
  PromiseAppRegistryCache cache_;
};

TEST_F(PromiseAppRegistryCacheTest, AddPromiseApp) {
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  ASSERT_FALSE(IsPromiseAppRegistered(package_id));
  cache().OnPromiseApp(std::move(promise_app));
  ASSERT_TRUE(IsPromiseAppRegistered(package_id));
}

TEST_F(PromiseAppRegistryCacheTest, UpdatePromiseAppProgress) {
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
  float progress_initial = 0.1;
  float progress_next = 0.9;

  // Check that there aren't any promise apps registered yet.
  EXPECT_EQ(CountPromiseAppsRegistered(), 0);

  // Register a promise app with no installation progress value.
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  cache().OnPromiseApp(std::move(promise_app));
  EXPECT_FALSE(
      cache().GetPromiseAppForTesting(package_id)->progress.has_value());
  EXPECT_EQ(CountPromiseAppsRegistered(), 1);

  // Update the progress value for the correct app and confirm the progress
  // value.
  auto promise_delta = std::make_unique<PromiseApp>(package_id);
  promise_delta->progress = progress_initial;
  cache().OnPromiseApp(std::move(promise_delta));
  EXPECT_EQ(cache().GetPromiseAppForTesting(package_id)->progress,
            progress_initial);

  // Update the progress value again and check if it is the correct value.
  auto promise_delta_next = std::make_unique<PromiseApp>(package_id);
  promise_delta_next->progress = progress_next;
  cache().OnPromiseApp(std::move(promise_delta_next));
  EXPECT_EQ(cache().GetPromiseAppForTesting(package_id)->progress,
            progress_next);

  // All these changes should have applied to the same promise app instead
  // of creating new ones.
  EXPECT_EQ(CountPromiseAppsRegistered(), 1);
}

TEST_F(PromiseAppRegistryCacheTest, GetAllPromiseApps) {
  // There should be no promise apps registered yet.
  EXPECT_EQ(cache().GetAllPromiseApps().size(), 0u);

  // Register some promise apps.
  auto package_id_1 = PackageId(AppType::kArc, "test1");
  auto promise_app_1 = std::make_unique<PromiseApp>(package_id_1);
  cache().OnPromiseApp(std::move(promise_app_1));

  auto package_id_2 = PackageId(AppType::kArc, "test2");
  auto promise_app_2 = std::make_unique<PromiseApp>(package_id_2);
  cache().OnPromiseApp(std::move(promise_app_2));

  // Check that all the promise apps are being retrieved.
  auto promise_app_list = cache().GetAllPromiseApps();
  EXPECT_EQ(promise_app_list.size(), 2u);
  EXPECT_EQ(promise_app_list[0]->package_id, package_id_1);
  EXPECT_EQ(promise_app_list[1]->package_id, package_id_2);
}

}  // namespace apps
