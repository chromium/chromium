// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom.h"
#include "chromeos/ash/experiences/arc/session/adb_sideloading_availability_delegate.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_app_instance.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kPackageName[] = "com.example.third_party_app";
constexpr char kUserEmail[] = "user@test";

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
  ArcActivationNecessityCheckerTest() = default;
  ~ArcActivationNecessityCheckerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kEnableArcVm);

    ash::ConciergeClient::InitializeFake();

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->local_state(),
        ash::CrosSettings::Get()));

    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId("1234567890"));
    user_manager_->EnsureUser(account_id, user_manager::UserType::kRegular,
                              /*is_ephemeral=*/false);
    user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));

    ash::ScopedAccountIdAnnotator annotator(
        testing_profile_manager_.profile_manager(), account_id);
    profile_ = testing_profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

    profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
    profile_->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
    profile_->GetPrefs()->SetBoolean(prefs::kArcPackagesIsUpToDate, true);

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

    profile_ = nullptr;
    testing_profile_manager_.DeleteAllTestingProfiles();

    ash::ConciergeClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  ash::ScopedStubInstallAttributes install_attributes_;
  ash::ScopedTestingCrosSettings testing_cros_settings_;
  session_manager::SessionManager session_manager_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  user_manager::ScopedUserManager user_manager_;

  raw_ptr<TestingProfile> profile_ = nullptr;
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kArcOnDemandV2);
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

TEST_F(ArcActivationNecessityCheckerTest, InstalledAppWithFeatureDisabled) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kArcOnDemandV2);

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

TEST_F(ArcActivationNecessityCheckerTest, ManagementTransition) {
  profile_->GetPrefs()->SetInteger(
      prefs::kArcManagementTransition,
      int(ArcManagementTransition::CHILD_TO_REGULAR));

  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, AlwaysOnVpn) {
  profile_->GetPrefs()->SetString(prefs::kAlwaysOnVpnPackage, "vpn.app.fake");
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcActivationNecessityCheckerTest, CoralFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  // Coral feature is enabled when both flags below are enabled.
  feature_list.InitWithFeatures(
      /* enabled_features */ {ash::features::kCoralFeature,
                              ash::features::kCoralFeatureAllowed},
      /* disabled_features */ {});
  base::test::TestFuture<bool> future;
  checker_->Check(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace

}  // namespace arc
