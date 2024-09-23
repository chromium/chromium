// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/adb_sideloading_availability_delegate.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kPackageName[] = "com.example.third_party_app";

class FakeAdbSideloadingAvailabilityDelegate
    : public AdbSideloadingAvailabilityDelegate {
 public:
  FakeAdbSideloadingAvailabilityDelegate() = default;
  ~FakeAdbSideloadingAvailabilityDelegate() override = default;

  void set_result(bool result) { result_ = result; }

  void CanChangeAdbSideloading(
      base::OnceCallback<void(bool can_change_adb_sideloading)> callback)
      override {
    std::move(callback).Run(result_);
  }

 private:
  bool result_ = false;
};

class ArcActivationNecessityCheckerTest : public testing::Test {
 public:
  ArcActivationNecessityCheckerTest()
      : fake_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~ArcActivationNecessityCheckerTest() override = default;

  void SetUp() override {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);

    ash::ConciergeClient::InitializeFake();

    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
    profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
    profile_->GetPrefs()->SetBoolean(prefs::kArcPackagesIsUpToDate, true);

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "1234567890"));
    auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    app_instance_ = std::make_unique<arc::FakeAppInstance>(
        ArcAppListPrefs::Get(profile_.get()));
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());

    arc_session_manager_->SetProfile(profile_.get());

    checker_ = std::make_unique<ArcActivationNecessityChecker>(
        profile_.get(), &adb_sideloading_availability_delegate_);

    // Pre-installed apps shouldn't cause ARC activation.
    auto package_info = mojom::ArcPackageInfo::New();
    package_info->package_name = "org.chromium.preinstalled";
    package_info->preinstalled = true;
    app_instance_->SendPackageAdded(std::move(package_info));
  }

  void TearDown() override {
    checker_.reset();
    app_instance_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    profile_.reset();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  base::test::ScopedFeatureList feature_list_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  FakeAdbSideloadingAvailabilityDelegate adb_sideloading_availability_delegate_;
  std::unique_ptr<ArcActivationNecessityChecker> checker_;
};

TEST_F(ArcActivationNecessityCheckerTest, NotARCVM) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      ash::switches::kEnableArcVm);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", false, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, UnmanagedUserEnabled) {
  base::HistogramTester histogram_tester;
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kArcOnDemandV2);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, UnmanagedUserDisabled) {
  base::HistogramTester histogram_tester;
  profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, AdbSideloadingIsAvailable) {
  base::HistogramTester histogram_tester;
  adb_sideloading_availability_delegate_.set_result(true);
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", false, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, PacakgeListIsNotUpToDate) {
  base::HistogramTester histogram_tester;
  profile_->GetPrefs()->SetBoolean(prefs::kArcPackagesIsUpToDate, false);

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", false, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, AppIsInstalled) {
  base::HistogramTester histogram_tester;
  auto package_info = mojom::ArcPackageInfo::New();
  package_info->package_name = "com.example.third_party_app";
  app_instance_->SendPackageAdded(std::move(package_info));

  std::vector<mojom::AppInfoPtr> fake_apps_;
  mojom::AppInfoPtr app_info = mojom::AppInfo::New(
      base::StringPrintf("Fake App"),
      base::StringPrintf("com.example.third_party_app"),
      base::StringPrintf("fake.app.activity"), false /* sticky */);
  fake_apps_.emplace_back(std::move(app_info));
  app_instance_->SendRefreshAppList(fake_apps_);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, NoNeedToActivate) {
  base::HistogramTester histogram_tester;
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, DelayOnLastLaunchedV2) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["activate_on_app_launch"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(kArcOnDemandV2, params);

  auto package_info = mojom::ArcPackageInfo::New();
  package_info->package_name = kPackageName;
  app_instance_->SendPackageAdded(std::move(package_info));

  std::vector<mojom::AppInfoPtr> fake_apps_;
  mojom::AppInfoPtr app_info = mojom::AppInfo::New(
      base::StringPrintf("Fake App"), base::StringPrintf(kPackageName),
      base::StringPrintf("fake.app.activity"), false /* sticky */);
  fake_apps_.emplace_back(std::move(app_info));
  app_instance_->SendRefreshAppList(fake_apps_);
  base::RunLoop().RunUntilIdle();

  auto* prefs_ = ArcAppListPrefs::Get(profile_.get());
  const std::string app_id = prefs_->GetAppIdByPackageName(kPackageName);
  base::Time timestamp = base::Time::Now();
  prefs_->SetLastLaunchTimeForTesting(app_id, timestamp);

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", false, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, InactiveDaysNotDelayedV2) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["activate_on_app_launch"] = "false";
  params["inactive_interval"] = "5d";
  feature_list.InitAndEnableFeatureWithParameters(kArcOnDemandV2, params);

  auto package_info = mojom::ArcPackageInfo::New();
  package_info->package_name = kPackageName;
  app_instance_->SendPackageAdded(std::move(package_info));

  std::vector<mojom::AppInfoPtr> fake_apps_;
  mojom::AppInfoPtr app_info = mojom::AppInfo::New(
      base::StringPrintf("Fake App"), base::StringPrintf(kPackageName),
      base::StringPrintf("fake.app.activity"), false /* sticky */);
  fake_apps_.emplace_back(std::move(app_info));
  app_instance_->SendRefreshAppList(fake_apps_);
  base::RunLoop().RunUntilIdle();

  auto* prefs_ = ArcAppListPrefs::Get(profile_.get());
  const std::string app_id = prefs_->GetAppIdByPackageName(kPackageName);
  base::Time timestamp = base::Time::Now();
  prefs_->SetLastLaunchTimeForTesting(app_id, timestamp);

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", false, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, InactiveDaysDelayedV2) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["activate_on_app_launch"] = "false";
  params["inactive_interval"] = "5d";
  feature_list.InitAndEnableFeatureWithParameters(kArcOnDemandV2, params);

  auto package_info = mojom::ArcPackageInfo::New();
  package_info->package_name = kPackageName;
  app_instance_->SendPackageAdded(std::move(package_info));

  std::vector<mojom::AppInfoPtr> fake_apps_;
  mojom::AppInfoPtr app_info = mojom::AppInfo::New(
      base::StringPrintf("Fake App"), base::StringPrintf(kPackageName),
      base::StringPrintf("fake.app.activity"), false /* sticky */);
  fake_apps_.emplace_back(std::move(app_info));
  app_instance_->SendRefreshAppList(fake_apps_);
  base::RunLoop().RunUntilIdle();

  auto* prefs_ = ArcAppListPrefs::Get(profile_.get());
  const std::string app_id = prefs_->GetAppIdByPackageName(kPackageName);
  base::Time timestamp = (base::Time::Now() - base::Days(6));
  prefs_->SetLastLaunchTimeForTesting(app_id, timestamp);

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

TEST_F(ArcActivationNecessityCheckerTest, InactiveDays0DayDelayedV2) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["activate_on_app_launch"] = "false";
  params["inactive_interval"] = "0d";
  feature_list.InitAndEnableFeatureWithParameters(kArcOnDemandV2, params);

  auto package_info = mojom::ArcPackageInfo::New();
  package_info->package_name = kPackageName;
  app_instance_->SendPackageAdded(std::move(package_info));

  std::vector<mojom::AppInfoPtr> fake_apps_;
  mojom::AppInfoPtr app_info = mojom::AppInfo::New(
      base::StringPrintf("Fake App"), base::StringPrintf(kPackageName),
      base::StringPrintf("fake.app.activity"), false /* sticky */);
  fake_apps_.emplace_back(std::move(app_info));
  app_instance_->SendRefreshAppList(fake_apps_);
  base::RunLoop().RunUntilIdle();

  auto* prefs_ = ArcAppListPrefs::Get(profile_.get());
  const std::string app_id = prefs_->GetAppIdByPackageName(kPackageName);
  base::Time timestamp = (base::Time::Now() - base::Minutes(1));
  prefs_->SetLastLaunchTimeForTesting(app_id, timestamp);

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_FALSE(future.Get());
  histogram_tester.ExpectUniqueSample(
      "Arc.ArcOnDemandV2.ActivationShouldBeDelayed", true, 1);
}

}  // namespace

}  // namespace arc
