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

  PromiseApp* GetPromiseApp(const PackageId& package_id) {
    auto promise_iter = cache_.promise_app_map_.find(package_id);
    PromiseApp* promise_app = (promise_iter != cache_.promise_app_map_.end())
                                  ? promise_iter->second.get()
                                  : nullptr;
    return promise_app;
  }

 private:
  PromiseAppRegistryCache cache_;
};

TEST_F(PromiseAppRegistryCacheTest, AddPromiseApp) {
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  ASSERT_FALSE(IsPromiseAppRegistered(package_id));
  cache().AddPromiseApp(std::move(promise_app));
  ASSERT_TRUE(IsPromiseAppRegistered(package_id));
}

TEST_F(PromiseAppRegistryCacheTest, UpdatePromiseAppProgress) {
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
  float progress_initial = 0.1;
  float progress_next = 0.9;

  // Register a promise app with no installation progress value.
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  cache().AddPromiseApp(std::move(promise_app));
  EXPECT_FALSE(GetPromiseApp(package_id)->progress.has_value());

  // Update the progress value for the correct app, check if there is now a
  // progress value.
  cache().UpdatePromiseAppProgress(package_id, progress_initial);
  EXPECT_EQ(GetPromiseApp(package_id)->progress, progress_initial);

  // Update the progress value again and check if it is the correct value.
  cache().UpdatePromiseAppProgress(package_id, progress_next);
  EXPECT_EQ(GetPromiseApp(package_id)->progress, progress_next);
}

}  // namespace apps
