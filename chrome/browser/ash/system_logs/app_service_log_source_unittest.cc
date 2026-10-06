// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/app_service_log_source.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test.com",
                                            GaiaId::Literal("1234567890"));

}  // namespace

class AppServiceLogSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kAccountId));
    user_session_test_environment_->LogIn(kAccountId);
    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kAccountId));
    ASSERT_TRUE(profile_);

    // Wait for AppServiceProxy to be ready.
    app_service_test_.SetUp(profile_);
  }
  void TearDown() override {
    profile_ = nullptr;
    user_session_test_environment_.reset();
  }

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
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<Profile> profile_ = nullptr;
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
