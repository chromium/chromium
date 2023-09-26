// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_attributes_ash.h"

#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kErrorUserNotAffiliated[] = "Access denied.";

constexpr char kFakeAnnotatedLocation[] = "fake annotated location";
constexpr char kFakeAssetId[] = "fake asset ID";
constexpr char kFakeSerialNumber[] = "fake serial number";
constexpr char kFakeDirectoryApiId[] = "fake directory API ID";
constexpr char kFakeHostname[] = "fake-hostname";

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

namespace crosapi {

class DeviceAttributesAshTest
    : public testing::TestWithParam<TestProfileChoice> {
 public:
  DeviceAttributesAshTest() = default;
  ~DeviceAttributesAshTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    // Set up fake device attributes.
    device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes_->SetFakeDirectoryApiId(kFakeDirectoryApiId);
    device_attributes_->SetFakeDeviceSerialNumber(kFakeSerialNumber);
    device_attributes_->SetFakeDeviceAssetId(kFakeAssetId);
    device_attributes_->SetFakeDeviceAnnotatedLocation(kFakeAnnotatedLocation);
    device_attributes_->SetFakeDeviceHostname(kFakeHostname);

    device_attributes_ash_ = std::make_unique<DeviceAttributesAsh>();
    device_attributes_ash_->SetDeviceAttributesForTesting(
        std::move(device_attributes_));
    device_attributes_ash_->BindReceiver(
        device_attributes_remote_.BindNewPipeAndPassReceiver());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);

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
  }

  void TearDown() override { device_attributes_ash_.reset(); }

  void AddUser(bool is_affiliated = true) {
    AccountId account_id =
        AccountId::FromUserEmail(TestingProfile::kDefaultProfileUserName);
    const user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);
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
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<policy::FakeDeviceAttributes> device_attributes_;
  mojo::Remote<mojom::DeviceAttributes> device_attributes_remote_;
  std::unique_ptr<DeviceAttributesAsh> device_attributes_ash_;
};

TEST_P(DeviceAttributesAshTest, GetDirectoryDeviceId) {
  base::test::TestFuture<mojom::DeviceAttributesStringResultPtr> future;

  device_attributes_remote_->GetDirectoryDeviceId(future.GetCallback());

  mojom::DeviceAttributesStringResultPtr result = future.Take();
  const bool expect_contents = IsSigninProfileOrBelongsToAffiliatedUser();
  ASSERT_EQ(expect_contents, result->is_contents());
  if (expect_contents) {
    ASSERT_EQ(kFakeDirectoryApiId, result->get_contents());
  } else {
    ASSERT_EQ(kErrorUserNotAffiliated, result->get_error_message());
  }
}

TEST_P(DeviceAttributesAshTest, GetDeviceSerialNumber) {
  base::test::TestFuture<mojom::DeviceAttributesStringResultPtr> future;

  device_attributes_remote_->GetDeviceSerialNumber(future.GetCallback());

  mojom::DeviceAttributesStringResultPtr result = future.Take();
  const bool expect_contents = IsSigninProfileOrBelongsToAffiliatedUser();
  ASSERT_EQ(expect_contents, result->is_contents());
  if (expect_contents) {
    ASSERT_EQ(kFakeSerialNumber, result->get_contents());
  } else {
    ASSERT_EQ(kErrorUserNotAffiliated, result->get_error_message());
  }
}

TEST_P(DeviceAttributesAshTest, GetDeviceAssetId) {
  base::test::TestFuture<mojom::DeviceAttributesStringResultPtr> future;

  device_attributes_remote_->GetDeviceAssetId(future.GetCallback());

  mojom::DeviceAttributesStringResultPtr result = future.Take();
  const bool expect_contents = IsSigninProfileOrBelongsToAffiliatedUser();
  ASSERT_EQ(expect_contents, result->is_contents());
  if (expect_contents) {
    ASSERT_EQ(kFakeAssetId, result->get_contents());
  } else {
    ASSERT_EQ(kErrorUserNotAffiliated, result->get_error_message());
  }
}

TEST_P(DeviceAttributesAshTest, GetDeviceAnnotatedLocation) {
  base::test::TestFuture<mojom::DeviceAttributesStringResultPtr> future;

  device_attributes_remote_->GetDeviceAnnotatedLocation(future.GetCallback());

  mojom::DeviceAttributesStringResultPtr result = future.Take();
  const bool expect_contents = IsSigninProfileOrBelongsToAffiliatedUser();
  ASSERT_EQ(expect_contents, result->is_contents());
  if (expect_contents) {
    ASSERT_EQ(kFakeAnnotatedLocation, result->get_contents());
  } else {
    ASSERT_EQ(kErrorUserNotAffiliated, result->get_error_message());
  }
}

TEST_P(DeviceAttributesAshTest, GetDeviceHostname) {
  base::test::TestFuture<mojom::DeviceAttributesStringResultPtr> future;

  device_attributes_remote_->GetDeviceHostname(future.GetCallback());

  mojom::DeviceAttributesStringResultPtr result = future.Take();
  const bool expect_contents = IsSigninProfileOrBelongsToAffiliatedUser();
  ASSERT_EQ(expect_contents, result->is_contents());
  if (expect_contents) {
    ASSERT_EQ(kFakeHostname, result->get_contents());
  } else {
    ASSERT_EQ(kErrorUserNotAffiliated, result->get_error_message());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceAttributesAshTest,
    ::testing::Values(TestProfileChoice::kSigninProfile,
                      TestProfileChoice::kAffiliatedProfile,
                      TestProfileChoice::kNonAffiliatedProfile),
    &ParamToString);

}  // namespace crosapi
