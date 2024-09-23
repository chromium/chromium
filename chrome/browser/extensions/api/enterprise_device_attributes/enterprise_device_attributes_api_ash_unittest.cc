// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/device_attributes_ash.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kAccountId[] = "test_1@example.com";
constexpr char kFakeDirectoryApiId[] = "fake directory API ID";
constexpr char kFakeSerialNumber[] = "fake serial number";
constexpr char kFakeHostname[] = "fake-hostname";
constexpr char kFakeAssetId[] = "fake asset ID";
constexpr char kFakeAnnotatedLocation[] = "fake annotated location";

enum class TestProfileChoice {
  kSigninProfile,
  kNonAffiliatedProfile,
  kAffiliatedProfile
};

std::string ParamToString(
    const testing::TestParamInfo<TestProfileChoice>& info) {
  switch (info.param) {
    case TestProfileChoice::kSigninProfile:
      return "SigninProfile";
    case TestProfileChoice::kNonAffiliatedProfile:
      return "NonAffiliatedUser";
    case TestProfileChoice::kAffiliatedProfile:
      return "AffiliatedUser";
  }
}

}  // namespace

// The bool parameter tells if a test instance should use a sign in profile or
// not. This is useful because the extension APIs should return empty string on
// non sign in and non affiliated profiles.
class EnterpriseDeviceAttributesApiAshTest
    : public ash::DeviceSettingsTestBase,
      public testing::WithParamInterface<TestProfileChoice> {
 protected:
  EnterpriseDeviceAttributesApiAshTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    DeviceSettingsTestBase::SetUp();

    testing_profile_ = profile_manager_.CreateTestingProfile(kAccountId);

    switch (GetParam()) {
      case TestProfileChoice::kSigninProfile:
        TestingProfile* signin_profile;
        signin_profile = static_cast<TestingProfile*>(
            ash::ProfileHelper::GetSigninProfile());
        EXPECT_TRUE(ProfileManager::GetPrimaryUserProfile()->IsSameOrParent(
            signin_profile));
        ASSERT_EQ(signin_profile, ProfileManager::GetPrimaryUserProfile());
        break;
      case TestProfileChoice::kNonAffiliatedProfile:
        AddUser(/*is_affiliated=*/false);
        break;
      case TestProfileChoice::kAffiliatedProfile:
        AddUser(/*is_affiliated=*/true);
        break;
    }

    // Set up fake device attributes.
    device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes_->SetFakeDirectoryApiId(kFakeDirectoryApiId);
    device_attributes_->SetFakeDeviceSerialNumber(kFakeSerialNumber);
    device_attributes_->SetFakeDeviceAssetId(kFakeAssetId);
    device_attributes_->SetFakeDeviceAnnotatedLocation(kFakeAnnotatedLocation);
    device_attributes_->SetFakeDeviceHostname(kFakeHostname);

    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
    manager_->crosapi_ash()
        ->device_attributes_ash()
        ->SetDeviceAttributesForTesting(std::move(device_attributes_));
  }

  void TearDown() override {
    manager_.reset();
    ash::DeviceSettingsTestBase::TearDown();
    ash::LoginState::Shutdown();
  }

  void AddUser(bool is_affiliated = true) {
    AccountId account_id = AccountId::FromUserEmail(kAccountId);
    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, is_affiliated, user_manager::UserType::kRegular,
        testing_profile_);
    user_manager_->LoginUser(account_id);
  }

  bool IsSigninProfileOrBelongsToAffiliatedUser() {
    switch (GetParam()) {
      case TestProfileChoice::kSigninProfile:
      case TestProfileChoice::kAffiliatedProfile:
        return true;
      case TestProfileChoice::kNonAffiliatedProfile:
        return false;
    }
  }

 protected:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<crosapi::CrosapiManager> manager_;
  std::unique_ptr<policy::FakeDeviceAttributes> device_attributes_;
};

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDirectoryDeviceIdFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", testing_profile_);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(
      IsSigninProfileOrBelongsToAffiliatedUser() ? kFakeDirectoryApiId : "",
      result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDeviceSerialNumberFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceSerialNumberFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", testing_profile_);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSigninProfileOrBelongsToAffiliatedUser() ? kFakeSerialNumber : "",
            result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDeviceAssetIdFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceAssetIdFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", testing_profile_);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSigninProfileOrBelongsToAffiliatedUser() ? kFakeAssetId : "",
            result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest,
       GetDeviceAnnotatedLocationFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", testing_profile_);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(
      IsSigninProfileOrBelongsToAffiliatedUser() ? kFakeAnnotatedLocation : "",
      result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDeviceHostnameFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceHostnameFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", testing_profile_);
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSigninProfileOrBelongsToAffiliatedUser() ? kFakeHostname : "",
            result->GetString());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    EnterpriseDeviceAttributesApiAshTest,
    ::testing::Values(TestProfileChoice::kSigninProfile,
                      TestProfileChoice::kAffiliatedProfile,
                      TestProfileChoice::kNonAffiliatedProfile),
    &ParamToString);
}  // namespace extensions
