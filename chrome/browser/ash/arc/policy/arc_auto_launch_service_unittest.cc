// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_auto_launch_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/test/fake_app_instance.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_service_registry.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kTestEmail[] = "user@example.com";
constexpr char kTestPackageName[] = "com.example.app";
constexpr char kTestActivity[] = "com.example.app.MainActivity";

class ArcAutoLaunchServiceTest : public testing::Test {
 public:
  ArcAutoLaunchServiceTest()
      : session_manager_(
            std::make_unique<session_manager::FakeSessionManagerDelegate>()),
        arc_app_test_(ArcAppTest::UserManagerMode::kDoNothing),
        user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {}

  ArcAutoLaunchServiceTest(const ArcAutoLaunchServiceTest&) = delete;
  ArcAutoLaunchServiceTest& operator=(const ArcAutoLaunchServiceTest&) = delete;
  ~ArcAutoLaunchServiceTest() override = default;

  void SetUp() override {
    arc_app_test_.PreProfileSetUp();
    InitializeProfile();
  }

  void TearDown() override {
    const AccountId account_id(AccountId::FromUserEmail(kTestEmail));
    app_service_registry_.Unregister(account_id);
    arc_app_test_.PreProfileTearDown();
    profile_.reset();
    arc_app_test_.PostProfileTearDown();
  }

 protected:
  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void InitializeProfile() {
    profile_ =
        TestingProfile::Builder()
            .SetPath(base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-user")))
            .SetProfileName(kTestEmail)
            .Build();
    const AccountId account_id(AccountId::FromUserEmail(kTestEmail));
    user_manager::User* user = GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());
    ash::AnnotatedAccountId::Set(profile_.get(), account_id);
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    app_service_registry_.Register(account_id, proxy);
    arc_app_test_.PostProfileSetUp(profile_.get());
  }

  // Creates the service through its keyed service factory.
  ArcAutoLaunchService* CreateService() {
    ArcAutoLaunchService* service =
        ArcAutoLaunchService::GetForBrowserContextForTesting(profile_.get());
    CHECK(service);
    return service;
  }

  // Replaces the apps installed in ARC with `apps`, given as pairs of package
  // and activity names. The apps are ready since the ARC instance is connected.
  // Also runs the tasks posted by the service in response.
  void SetInstalledApps(
      const std::vector<std::pair<std::string, std::string>>& apps) {
    std::vector<mojom::AppInfoPtr> app_infos;
    for (const auto& [package_name, activity] : apps) {
      app_infos.push_back(mojom::AppInfo::New("Test App", package_name,
                                              activity, /*sticky=*/false));
    }
    arc_app_test_.app_instance()->SendRefreshAppList(app_infos);
    base::RunLoop().RunUntilIdle();
  }

  void SetAutoLaunchApps(const std::vector<std::string>& packages) {
    base::ListValue auto_launch_apps;
    for (const std::string& package : packages) {
      auto_launch_apps.Append(package);
    }
    profile_->GetPrefs()->SetList(prefs::kArcAutoLaunchApps,
                                  std::move(auto_launch_apps));
  }

  // Returns the packages of the launch requests received by ARC, in order.
  std::vector<std::string> GetLaunchedPackages() {
    std::vector<std::string> packages;
    for (const auto& request :
         arc_app_test_.app_instance()->launch_requests()) {
      packages.push_back(request->package_name());
    }
    return packages;
  }

  // Returns the activities of the launch requests received by ARC, in order.
  std::vector<std::string> GetLaunchedActivities() {
    std::vector<std::string> activities;
    for (const auto& request :
         arc_app_test_.app_instance()->launch_requests()) {
      activities.push_back(request->activity());
    }
    return activities;
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  apps::AppServiceRegistry app_service_registry_;
  session_manager::SessionManager session_manager_;
  ArcAppTest arc_app_test_;
  user_manager::ScopedUserManager user_manager_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(ArcAutoLaunchServiceTest, EmptyPolicy_NoLaunch) {
  SetInstalledApps({{kTestPackageName, kTestActivity}});

  CreateService();

  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

TEST_F(ArcAutoLaunchServiceTest, LaunchesPreInstalledApp) {
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));
}

TEST_F(ArcAutoLaunchServiceTest, RegisteredButNotReady_LaunchesWhenReady) {
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  SetAutoLaunchApps({kTestPackageName});
  // Like at sign-in, the app is known from prefs but ARC hasn't booted yet.
  arc_app_test_.StopArcInstance();

  CreateService();
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  arc_app_test_.RestartArcInstance();
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));
}

TEST_F(ArcAutoLaunchServiceTest, MultipleActivities_LaunchesFirstByName) {
  SetInstalledApps({{kTestPackageName, "com.example.app.Settings"},
                    {kTestPackageName, "com.example.app.Main"},
                    {kTestPackageName, "com.example.app.Zoo"}});
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  EXPECT_THAT(GetLaunchedActivities(), ElementsAre("com.example.app.Main"));
}

TEST_F(ArcAutoLaunchServiceTest, MultipleApps_OnlyFirstLaunched) {
  constexpr char kPackageZ[] = "com.example.zebra";
  constexpr char kPackageA[] = "com.example.apple";
  constexpr char kPackageM[] = "com.example.mango";
  SetInstalledApps({{kPackageA, "com.example.apple.MainActivity"},
                    {kPackageM, "com.example.mango.MainActivity"},
                    {kPackageZ, "com.example.zebra.MainActivity"}});
  SetAutoLaunchApps({kPackageZ, kPackageA, kPackageM});

  CreateService();

  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kPackageZ));
}

TEST_F(ArcAutoLaunchServiceTest, FirstAppNotInstalled_OtherAppsNotLaunched) {
  constexpr char kPackage1[] = "com.example.app1";
  constexpr char kPackage2[] = "com.example.app2";
  // Only the second configured app is installed.
  SetInstalledApps({{kPackage2, "com.example.app2.MainActivity"}});
  SetAutoLaunchApps({kPackage1, kPackage2});

  CreateService();

  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

TEST_F(ArcAutoLaunchServiceTest, LaunchesAppWhenInstalledLater) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  // The app is installed, e.g. by CloudDPC.
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));
}

TEST_F(ArcAutoLaunchServiceTest, OtherConfiguredAppInstalled_NotLaunched) {
  constexpr char kPackage1[] = "com.example.app1";
  constexpr char kPackage2[] = "com.example.app2";
  SetAutoLaunchApps({kPackage1, kPackage2});

  CreateService();

  SetInstalledApps({{kPackage2, "com.example.app2.MainActivity"}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  SetInstalledApps({{kPackage1, "com.example.app1.MainActivity"},
                    {kPackage2, "com.example.app2.MainActivity"}});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kPackage1));
}

TEST_F(ArcAutoLaunchServiceTest, MultiActivityPackage_LaunchesFirstByNameOnce) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  // The activity whose name sorts first is registered last.
  SetInstalledApps({{kTestPackageName, "com.example.app.SettingsActivity"},
                    {kTestPackageName, "com.example.app.MainActivity"}});
  EXPECT_THAT(GetLaunchedActivities(),
              ElementsAre("com.example.app.MainActivity"));
}

TEST_F(ArcAutoLaunchServiceTest,
       NonLaunchableActivity_IgnoredUntilLaunchableActivityRegisters) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  // A non-launchable activity registers first, e.g. through a background task
  // created before the app list is refreshed.
  static_cast<mojom::AppHost*>(ArcAppListPrefs::Get(profile_.get()))
      ->OnTaskCreated(/*task_id=*/1, kTestPackageName,
                      "com.example.app.BackgroundActivity", "Background",
                      /*intent_uri=*/std::nullopt, /*session_id=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedActivities(), ElementsAre(kTestActivity));
}

TEST_F(ArcAutoLaunchServiceTest, TaskEntryBecomesLaunchable_LaunchesApp) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  // The app's main activity starts a task before the app list is refreshed, so
  // it is first registered as a non-launchable runtime entry.
  static_cast<mojom::AppHost*>(ArcAppListPrefs::Get(profile_.get()))
      ->OnTaskCreated(/*task_id=*/1, kTestPackageName, kTestActivity, "Task",
                      /*intent_uri=*/std::nullopt, /*session_id=*/0);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  // The app list refresh then makes the same activity launchable, which is
  // reported as a state change rather than a new registration.
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedActivities(), ElementsAre(kTestActivity));
}

TEST_F(ArcAutoLaunchServiceTest, AppUpdate_DoesNotRelaunch) {
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  SetAutoLaunchApps({kTestPackageName});

  CreateService();
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));

  // Updating the app refreshes the app list.
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));
}

TEST_F(ArcAutoLaunchServiceTest, Timeout_StopsObservation) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  task_environment_.FastForwardBy(ArcAutoLaunchService::kLaunchTimeout);

  // The app is installed too late, so it is not launched.
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

TEST_F(ArcAutoLaunchServiceTest, ArcAppListPrefsDestroyed_CleansUpPending) {
  SetAutoLaunchApps({kTestPackageName});

  ArcAutoLaunchService* service = CreateService();

  service->OnArcAppListPrefsDestroyed();

  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

TEST_F(ArcAutoLaunchServiceTest, Shutdown_StopsPendingLaunch) {
  SetAutoLaunchApps({kTestPackageName});

  ArcAutoLaunchService* service = CreateService();

  service->Shutdown();

  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

TEST_F(ArcAutoLaunchServiceTest, DynamicPolicyArrival_LaunchesApp) {
  SetInstalledApps({{kTestPackageName, kTestActivity}});

  // The service starts before the policy is set.
  CreateService();
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  SetAutoLaunchApps({kTestPackageName});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kTestPackageName));
}

TEST_F(ArcAutoLaunchServiceTest, DynamicPolicyUpdate_PendingAppReplaced) {
  constexpr char kPackage1[] = "com.example.app1";
  constexpr char kPackage2[] = "com.example.app2";
  SetAutoLaunchApps({kPackage1});

  CreateService();

  // The policy changes before app 1 is installed.
  SetAutoLaunchApps({kPackage2});

  // App 1 is not configured anymore, so it is not launched.
  SetInstalledApps({{kPackage1, "com.example.app1.MainActivity"}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());

  SetInstalledApps({{kPackage1, "com.example.app1.MainActivity"},
                    {kPackage2, "com.example.app2.MainActivity"}});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kPackage2));
}

TEST_F(ArcAutoLaunchServiceTest, DynamicPolicyUpdate_AfterLaunch_Ignored) {
  constexpr char kPackage1[] = "com.example.app1";
  constexpr char kPackage2[] = "com.example.app2";
  SetInstalledApps({{kPackage1, "com.example.app1.MainActivity"},
                    {kPackage2, "com.example.app2.MainActivity"}});
  SetAutoLaunchApps({kPackage1});

  CreateService();
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kPackage1));

  // At most one app is launched per session, also after the timeout.
  task_environment_.FastForwardBy(ArcAutoLaunchService::kLaunchTimeout);
  SetAutoLaunchApps({kPackage2});
  EXPECT_THAT(GetLaunchedPackages(), ElementsAre(kPackage1));
}

TEST_F(ArcAutoLaunchServiceTest, DynamicPolicyUpdate_PolicyCleared) {
  SetAutoLaunchApps({kTestPackageName});

  CreateService();

  // The policy is cleared before the app is installed.
  SetAutoLaunchApps({});
  SetInstalledApps({{kTestPackageName, kTestActivity}});
  EXPECT_THAT(GetLaunchedPackages(), IsEmpty());
}

}  // namespace

}  // namespace arc
