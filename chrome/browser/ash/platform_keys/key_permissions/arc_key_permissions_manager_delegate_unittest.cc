// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/arc_key_permissions_manager_delegate.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace platform_keys {

namespace {

constexpr char kTestArcPackageName1[] = "com.example.app1";
constexpr char kTestArcPackageName2[] = "com.example.app2";

}  // namespace

class MockArcKpmDelegateObserver : public ArcKpmDelegate::Observer {
 public:
  MockArcKpmDelegateObserver() = default;
  MockArcKpmDelegateObserver(const MockArcKpmDelegateObserver&) = delete;
  MockArcKpmDelegateObserver& operator=(const MockArcKpmDelegateObserver&) =
      delete;
  ~MockArcKpmDelegateObserver() override = default;

  MOCK_METHOD(void,
              OnArcUsageAllowanceForCorporateKeysChanged,
              (bool allowed),
              (override));
};

class ArcKeyPermissionsManagerDelegateTest : public testing::Test {
 public:
  ArcKeyPermissionsManagerDelegateTest() = default;
  ArcKeyPermissionsManagerDelegateTest(
      const ArcKeyPermissionsManagerDelegateTest& other) = delete;
  ArcKeyPermissionsManagerDelegateTest& operator=(
      const ArcKeyPermissionsManagerDelegateTest& other) = delete;
  ~ArcKeyPermissionsManagerDelegateTest() override = default;

  void SetUp() override {
    policy_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    policy_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    std::vector<
        raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>
        providers = {policy_provider_.get()};
    auto policy_service_ =
        std::make_unique<policy::PolicyServiceImpl>(providers);

    TestingProfile::Builder builder;
    builder.SetPolicyService(std::move(policy_service_));
    profile_ = builder.Build();

    arc_app_test_.SetUp(profile_.get());
    app_instance_ = std::make_unique<arc::FakeAppInstance>(
        arc_app_test_.arc_app_list_prefs());

    system_delegate_ = std::make_unique<SystemTokenArcKpmDelegate>();
    SystemTokenArcKpmDelegate::SetSystemTokenArcKpmDelegateForTesting(
        system_delegate_.get());

    CreatePrimaryUserDelegate();
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    if (primary_user_delegate_) {
      ShutDownPrimaryUserDelegate();
    }
    system_delegate_.reset();
    profile_.reset();
  }

 protected:
  void InstallArcPackage(const std::string& package_name) {
    arc::mojom::ArcPackageInfo package;
    package.package_name = package_name;
    app_instance_->InstallPackage(package.Clone());
  }

  void UninstallArcPackage(const std::string& package_name) {
    app_instance_->UninstallPackage(package_name);
  }

  void SetCorporateUsageInPolicyForPackage(const std::string& package_name,
                                           bool allowed) {
    base::Value::Dict corporate_key_usage;
    corporate_key_usage.SetByDottedPath("allowCorporateKeyUsage", allowed);

    base::Value::Dict policy_value;
    policy_value.Set(package_name, base::Value(std::move(corporate_key_usage)));

    policy::PolicyMap policy_map;
    policy_map.Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                   base::Value(policy_value.Clone()), nullptr);

    policy_provider_->UpdateChromePolicy(policy_map);
  }

  void CreatePrimaryUserDelegate() {
    primary_user_delegate_ =
        std::make_unique<UserPrivateTokenArcKpmDelegate>(profile_.get());

    primary_user_delegate_->AddObserver(&mock_arc_kpm_delegate_observer_);
  }

  void ShutDownPrimaryUserDelegate() {
    primary_user_delegate_->RemoveObserver(&mock_arc_kpm_delegate_observer_);
    primary_user_delegate_->Shutdown();
    primary_user_delegate_.reset();
  }

  SystemTokenArcKpmDelegate* system_delegate() {
    return system_delegate_.get();
  }

  UserPrivateTokenArcKpmDelegate* user_delegate() {
    return primary_user_delegate_.get();
  }

  Profile* profile() { return profile_.get(); }

  testing::StrictMock<MockArcKpmDelegateObserver>
      mock_arc_kpm_delegate_observer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> policy_provider_;
  ArcAppTest arc_app_test_;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SystemTokenArcKpmDelegate> system_delegate_;
  std::unique_ptr<UserPrivateTokenArcKpmDelegate> primary_user_delegate_;
};

TEST_F(ArcKeyPermissionsManagerDelegateTest, Basic) {
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(true))
      .Times(1);
  InstallArcPackage(kTestArcPackageName1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, NoArcPackageInstalled) {
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(testing::_))
      .Times(0);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, KeyPermissionsPolicyNotSet) {
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(testing::_))
      .Times(0);
  InstallArcPackage(kTestArcPackageName1);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest,
       PolicyAndInstalledPackages_NoMatch) {
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(testing::_))
      .Times(0);
  InstallArcPackage(kTestArcPackageName1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName2, /*allowed=*/true);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, UninstallArcPackage) {
  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(true))
      .Times(1);
  InstallArcPackage(kTestArcPackageName1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);

  testing::Mock::VerifyAndClearExpectations(&mock_arc_kpm_delegate_observer_);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(false))
      .Times(1);
  UninstallArcPackage(kTestArcPackageName1);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, UninstallIrrelevantArcPackage) {
  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(true))
      .Times(1);
  InstallArcPackage(kTestArcPackageName1);
  InstallArcPackage(kTestArcPackageName2);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);

  testing::Mock::VerifyAndClearExpectations(&mock_arc_kpm_delegate_observer_);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(testing::_))
      .Times(0);
  UninstallArcPackage(kTestArcPackageName2);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, ChangeKeyPermissionsPolicy) {
  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(true))
      .Times(1);
  InstallArcPackage(kTestArcPackageName1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);

  testing::Mock::VerifyAndClearExpectations(&mock_arc_kpm_delegate_observer_);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(false))
      .Times(1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/false);
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, ArcDisabled) {
  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(true))
      .Times(1);
  InstallArcPackage(kTestArcPackageName1);
  SetCorporateUsageInPolicyForPackage(kTestArcPackageName1, /*allowed=*/true);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), true);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), true);

  testing::Mock::VerifyAndClearExpectations(&mock_arc_kpm_delegate_observer_);

  EXPECT_CALL(mock_arc_kpm_delegate_observer_,
              OnArcUsageAllowanceForCorporateKeysChanged(false))
      .Times(1);
  profile()->GetPrefs()->SetBoolean(arc::prefs::kArcEnabled, false);

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
  EXPECT_EQ(user_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

TEST_F(ArcKeyPermissionsManagerDelegateTest, NoPrimaryDelegate) {
  ShutDownPrimaryUserDelegate();

  EXPECT_EQ(system_delegate()->AreCorporateKeysAllowedForArcUsage(), false);
}

}  // namespace platform_keys
}  // namespace ash
