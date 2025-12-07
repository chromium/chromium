// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_waiter.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr std::string_view kTestAppId = "test-app";
const AccountId kTestAccountId = AccountId::FromUserEmail("test@example.com");
}  // namespace

TEST(AppWaiterTest, CacheLoadedLater) {
  base::test::TestFuture<std::string> future;
  ash::AppWaiter app_waiter(kTestAccountId, future.GetCallback(), kTestAppId);

  EXPECT_FALSE(future.IsReady());

  apps::AppRegistryCache cache;

  std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
      apps::AppType::kSystemWeb, std::string(kTestAppId));
  app->name = "Test App";

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));

  cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                         /*should_notify_initialized=*/false);

  EXPECT_FALSE(future.IsReady());

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(kTestAccountId,
                                                           &cache);

  EXPECT_EQ(future.Get(), "Test App");
}

TEST(AppWaiterTest, AppLoadedWithName) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(kTestAccountId,
                                                           &cache);
  ash::AppWaiter app_waiter(kTestAccountId, future.GetCallback(), kTestAppId);

  std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
      apps::AppType::kSystemWeb, std::string(kTestAppId));
  app->name = "Test App";

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));

  cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                         /*should_notify_initialized=*/false);

  EXPECT_EQ(future.Get(), "Test App");
}

TEST(AppWaiterTest, AppLoadedFirstNameLoadedLater) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(kTestAccountId,
                                                           &cache);
  ash::AppWaiter app_waiter(kTestAccountId, future.GetCallback(), kTestAppId);

  {
    std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
        apps::AppType::kSystemWeb, std::string(kTestAppId));

    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));

    cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                           /*should_notify_initialized=*/false);
  }

  EXPECT_FALSE(future.IsReady());

  {
    std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
        apps::AppType::kSystemWeb, std::string(kTestAppId));
    app->name = "Test App";

    std::vector<apps::AppPtr> apps;
    apps.push_back(std::move(app));

    cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                           /*should_notify_initialized=*/false);
  }

  EXPECT_EQ(future.Get(), "Test App");
}

TEST(AppWaiterTest, AppAlreadyLoaded) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(kTestAccountId,
                                                           &cache);

  std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
      apps::AppType::kSystemWeb, std::string(kTestAppId));
  app->name = "Test App";

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));

  cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                         /*should_notify_initialized=*/false);

  ash::AppWaiter app_waiter(kTestAccountId, future.GetCallback(), kTestAppId);
  EXPECT_EQ(future.Get(), "Test App");
}
