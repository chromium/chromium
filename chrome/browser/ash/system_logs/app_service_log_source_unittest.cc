// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/app_service_log_source.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class AppServiceLogSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    constexpr char kEmail[] = "test@test";
    const AccountId account_id = AccountId::FromUserEmail(kEmail);
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    EXPECT_TRUE(profile_manager_->SetUp());
    auto* profile =
        profile_manager_->CreateTestingProfile(kEmail,
                                               /*is_main_profile=*/true);
    profile_ = profile;
    fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false, user_manager::UserType::kRegular, profile);
    fake_user_manager->LoginUser(account_id);
    fake_user_manager->SwitchActiveUser(account_id);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    // Wait for AppServiceProxy to be ready.
    app_service_test_.SetUp(profile_);
  }
  void TearDown() override { user_manager_.reset(); }

 protected:
  void AddApp(const std::string& app_id,
              const std::string& publisher_id,
              apps::AppType app_type) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
    apps::AppPtr app = apps::AppPublisher::MakeApp(
        app_type, app_id, apps::Readiness::kReady, "app-name",
        apps::InstallReason::kUser, apps::InstallSource::kPlayStore);
    app->publisher_id = publisher_id;
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    proxy->OnApps(std::move(deltas), app_type, true);
  }

  void RunApp(const std::string& app_id) {
    apps::InstanceParams params(app_id, nullptr);
    params.state =
        std::make_pair(apps::InstanceState::kRunning, base::Time::Now());
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->InstanceRegistry()
        .CreateOrUpdateInstance(std::move(params));
  }
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  apps::AppServiceTest app_service_test_;
};

TEST_F(AppServiceLogSourceTest, InstalledApps) {
  AddApp("arcappid", "com.google.ArcApp", apps::AppType::kArc);
  AddApp("webappid", "https://web.app", apps::AppType::kWeb);
  base::test::TestFuture<std::unique_ptr<SystemLogsResponse>> future;
  AppServiceLogSource source;
  source.Fetch(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  auto response = future.Get()->begin()->second;
  EXPECT_THAT(response, ::testing::HasSubstr(
                            "app://com.google.ArcApp, Arc, installed\n"));
  EXPECT_THAT(response,
              ::testing::HasSubstr("https://web.app/, WebApp, installed\n"));
}

TEST_F(AppServiceLogSourceTest, RunningApp) {
  AddApp("webappid", "https://web.app", apps::AppType::kWeb);
  RunApp("webappid");
  base::test::TestFuture<std::unique_ptr<SystemLogsResponse>> future;
  AppServiceLogSource source;
  source.Fetch(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  auto response = future.Get()->begin()->second;
  EXPECT_THAT(response,
              ::testing::HasSubstr("https://web.app/, WebApp, running\n"));
}
}  // namespace system_logs
