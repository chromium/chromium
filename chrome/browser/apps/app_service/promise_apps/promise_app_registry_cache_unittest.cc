// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_apps.h"
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
  EXPECT_FALSE(cache().GetPromiseApp(package_id)->progress.has_value());
  EXPECT_EQ(CountPromiseAppsRegistered(), 1);

  // Update the progress value for the correct app and confirm the progress
  // value.
  auto promise_delta = std::make_unique<PromiseApp>(package_id);
  promise_delta->progress = progress_initial;
  cache().OnPromiseApp(std::move(promise_delta));
  EXPECT_EQ(cache().GetPromiseApp(package_id)->progress, progress_initial);

  // Update the progress value again and check if it is the correct value.
  auto promise_delta_next = std::make_unique<PromiseApp>(package_id);
  promise_delta_next->progress = progress_next;
  cache().OnPromiseApp(std::move(promise_delta_next));
  EXPECT_EQ(cache().GetPromiseApp(package_id)->progress, progress_next);

  // All these changes should have applied to the same promise app instead
  // of creating new ones.
  EXPECT_EQ(CountPromiseAppsRegistered(), 1);
}

}  // namespace apps
