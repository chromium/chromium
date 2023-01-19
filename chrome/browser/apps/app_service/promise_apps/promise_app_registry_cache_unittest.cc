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
  bool IsPromiseAppRegistered(const PromiseAppRegistryCache& cache,
                              const PackageId& package_id) {
    return cache.promise_app_map_.contains(package_id);
  }
};

TEST_F(PromiseAppRegistryCacheTest, AddPromiseApp) {
  PromiseAppRegistryCache cache;
  PackageId package_id = PackageId(AppType::kArc, "test.package.name");
  auto promise_app = std::make_unique<PromiseApp>(package_id);
  ASSERT_FALSE(IsPromiseAppRegistered(cache, package_id));
  cache.AddPromiseApp(std::move(promise_app));
  ASSERT_TRUE(IsPromiseAppRegistered(cache, package_id));
}

}  // namespace apps
