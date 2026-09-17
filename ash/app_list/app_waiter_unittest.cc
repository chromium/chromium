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

class AppWaiterTest : public testing::Test {
 protected:
  const AccountId test_account_id_ =
      AccountId::FromUserEmail("test@example.com");
};

}  // namespace

TEST_F(AppWaiterTest, CacheLoadedLater) {
  base::test::TestFuture<std::string> future;
  ash::AppWaiter app_waiter(test_account_id_, future.GetCallback(), kTestAppId);

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

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(test_account_id_,
                                                           &cache);

  EXPECT_EQ(future.Get(), "Test App");
}

TEST_F(AppWaiterTest, AppLoadedWithName) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(test_account_id_,
                                                           &cache);
  ash::AppWaiter app_waiter(test_account_id_, future.GetCallback(), kTestAppId);

  std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
      apps::AppType::kSystemWeb, std::string(kTestAppId));
  app->name = "Test App";

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));

  cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                         /*should_notify_initialized=*/false);

  EXPECT_EQ(future.Get(), "Test App");
}

TEST_F(AppWaiterTest, AppLoadedFirstNameLoadedLater) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(test_account_id_,
                                                           &cache);
  ash::AppWaiter app_waiter(test_account_id_, future.GetCallback(), kTestAppId);

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

TEST_F(AppWaiterTest, AppAlreadyLoaded) {
  base::test::TestFuture<std::string> future;
  apps::AppRegistryCache cache;
  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(test_account_id_,
                                                           &cache);

  std::unique_ptr<apps::App> app = std::make_unique<apps::App>(
      apps::AppType::kSystemWeb, std::string(kTestAppId));
  app->name = "Test App";

  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));

  cache.OnAppsForTesting(std::move(apps), apps::AppType::kSystemWeb,
                         /*should_notify_initialized=*/false);

  ash::AppWaiter app_waiter(test_account_id_, future.GetCallback(), kTestAppId);
  EXPECT_EQ(future.Get(), "Test App");
}
