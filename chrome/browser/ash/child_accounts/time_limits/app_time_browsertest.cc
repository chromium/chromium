// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/child_accounts/child_user_service.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace app_time {

namespace {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  return package;
}

arc::mojom::AppInfoPtr CreateArcAppInfo(const std::string& package_name) {
  return arc::mojom::AppInfo::New(package_name, package_name,
                                  base::StrCat({package_name, ".", "activity"}),
                                  true /* sticky */);
}

}  // namespace

// Integration tests for Per-App Time Limits feature.
class AppTimeTest : public MixinBasedInProcessBrowserTest {
 protected:
  AppTimeTest() = default;
  AppTimeTest(const AppTimeTest&) = delete;
  AppTimeTest& operator=(const AppTimeTest&) = delete;
  ~AppTimeTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Started());
    logged_in_user_mixin_.LogInUser();

    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);
    arc_app_list_prefs_ = ArcAppListPrefs::Get(browser()->profile());
    EXPECT_TRUE(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    arc_app_instance_ =
        std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);
    arc_app_list_prefs_->app_connection_holder()->SetInstance(
        arc_app_instance_.get());
    WaitForInstanceReady(arc_app_list_prefs_->app_connection_holder());
    arc_app_instance_->set_icon_response_type(
        arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SKIP);
  }

  void TearDownOnMainThread() override {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        arc_app_instance_.get());
    arc_app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  void UpdatePerAppTimeLimitsPolicy(const base::Value::Dict& policy) {
    std::string policy_value;
    base::JSONWriter::Write(policy, &policy_value);

    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_perapptimelimits()
        ->set_value(policy_value);

    logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
        GetCurrentProfile());

    base::RunLoop().RunUntilIdle();
  }

  AppActivityRegistry* GetAppRegistry() {
    auto child_user_service = ChildUserService::TestApi(
        ChildUserServiceFactory::GetForBrowserContext(GetCurrentProfile()));
    EXPECT_TRUE(child_user_service.app_time_controller());
    auto app_time_controller =
        AppTimeController::TestApi(child_user_service.app_time_controller());
    return app_time_controller.app_registry();
  }

  void InstallArcApp(const AppId& app_id) {
    EXPECT_EQ(apps::AppType::kArc, app_id.app_type());
    const std::string& package_name = app_id.app_id();
    arc_app_instance_->SendPackageAdded(
        CreateArcAppPackage(package_name)->Clone());

    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.emplace_back(CreateArcAppInfo(package_name));

    arc_app_instance_->SendPackageAppListRefreshed(package_name, apps);

    base::RunLoop().RunUntilIdle();
  }

  void Equals(const AppLimit& limit1, const AppLimit& limit2) {
    EXPECT_EQ(limit1.restriction(), limit2.restriction());
    EXPECT_EQ(limit1.daily_limit(), limit2.daily_limit());
    // Compare JavaTime, because some precision is lost when serializing
    // and deserializing.
    EXPECT_EQ(limit1.last_updated().InMillisecondsSinceUnixEpoch(),
              limit2.last_updated().InMillisecondsSinceUnixEpoch());
  }

 private:
  Profile* GetCurrentProfile() {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    Profile* profile =
        ProfileHelper::Get()->GetProfileByUser(user_manager->GetActiveUser());
    EXPECT_TRUE(profile);

    return profile;
  }

  LoggedInUserMixin logged_in_user_mixin_{&mixin_host_, /*test_base=*/this,
                                          embedded_test_server(),
                                          LoggedInUserMixin::LogInType::kChild};

  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_prefs_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> arc_app_instance_;
};

IN_PROC_BROWSER_TEST_F(AppTimeTest, AppInstallation) {
  const AppId app1(apps::AppType::kArc, "com.example.app1");
  AppActivityRegistry* app_registry = GetAppRegistry();
  EXPECT_FALSE(app_registry->IsAppInstalled(app1));

  InstallArcApp(app1);

  ASSERT_TRUE(app_registry->IsAppInstalled(app1));
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
}

IN_PROC_BROWSER_TEST_F(AppTimeTest, PerAppTimeLimitsPolicyUpdates) {
  // Install an app.
  const AppId app1(apps::AppType::kArc, "com.example.app1");
  InstallArcApp(app1);

  AppActivityRegistry* app_registry = GetAppRegistry();
  AppActivityRegistry::TestApi app_registry_test(app_registry);
  ASSERT_TRUE(app_registry->IsAppInstalled(app1));
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));

  // Block the app.
  AppTimeLimitsPolicyBuilder block_policy;
  const AppLimit block_limit =
      AppLimit(AppRestriction::kBlocked, std::nullopt, base::Time::Now());
  block_policy.AddAppLimit(app1, block_limit);
  block_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(block_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(block_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit for the app - app should not paused.
  AppTimeLimitsPolicyBuilder time_limit_policy;
  const AppLimit time_limit =
      AppLimit(AppRestriction::kTimeLimit, base::Hours(1), base::Time::Now());
  time_limit_policy.AddAppLimit(app1, time_limit);
  time_limit_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(time_limit_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(time_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit of zero - app should be paused.
  AppTimeLimitsPolicyBuilder zero_time_limit_policy;
  const AppLimit zero_limit =
      AppLimit(AppRestriction::kTimeLimit, base::Hours(0), base::Time::Now());
  zero_time_limit_policy.AddAppLimit(app1, zero_limit);
  zero_time_limit_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(zero_time_limit_policy.value());
  EXPECT_FALSE(app_registry->IsAppAvailable(app1));
  EXPECT_TRUE(app_registry->IsAppTimeLimitReached(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(zero_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit grater then zero again - app should be available again.
  UpdatePerAppTimeLimitsPolicy(time_limit_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry->IsAppTimeLimitReached(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(time_limit, app_registry_test.GetAppLimit(app1).value());

  // Remove app restrictions.
  AppTimeLimitsPolicyBuilder no_limits_policy;
  no_limits_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(no_limits_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));
}

IN_PROC_BROWSER_TEST_F(AppTimeTest, PerAppTimeLimitsPolicyMultipleEntries) {
  // Install apps.
  const AppId app1(apps::AppType::kArc, "com.example.app1");
  InstallArcApp(app1);
  const AppId app2(apps::AppType::kArc, "com.example.app2");
  InstallArcApp(app2);
  const AppId app3(apps::AppType::kArc, "com.example.app3");
  InstallArcApp(app3);
  const AppId app4(apps::AppType::kArc, "com.example.app4");
  InstallArcApp(app4);

  AppActivityRegistry* app_registry = GetAppRegistry();
  AppActivityRegistry::TestApi app_registry_test(app_registry);
  for (const auto& app : {app1, app2, app3, app4}) {
    ASSERT_TRUE(app_registry->IsAppInstalled(app));
    EXPECT_TRUE(app_registry->IsAppAvailable(app));
    EXPECT_FALSE(app_registry_test.GetAppLimit(app));
  }

  // Send policy.
  AppTimeLimitsPolicyBuilder policy;
  policy.SetResetTime(6, 0);
  policy.AddAppLimit(app2, AppLimit(AppRestriction::kBlocked, std::nullopt,
                                    base::Time::Now()));
  policy.AddAppLimit(app3, AppLimit(AppRestriction::kTimeLimit,
                                    base::Minutes(15), base::Time::Now()));
  policy.AddAppLimit(app4, AppLimit(AppRestriction::kTimeLimit, base::Hours(1),
                                    base::Time::Now()));

  UpdatePerAppTimeLimitsPolicy(policy.value());

  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));

  ASSERT_TRUE(app_registry_test.GetAppLimit(app2));
  EXPECT_EQ(AppRestriction::kBlocked,
            app_registry_test.GetAppLimit(app2)->restriction());

  ASSERT_TRUE(app_registry_test.GetAppLimit(app3));
  EXPECT_EQ(AppRestriction::kTimeLimit,
            app_registry_test.GetAppLimit(app3)->restriction());

  ASSERT_TRUE(app_registry_test.GetAppLimit(app4));
  EXPECT_EQ(AppRestriction::kTimeLimit,
            app_registry_test.GetAppLimit(app4)->restriction());
}

}  // namespace app_time
}  // namespace ash
