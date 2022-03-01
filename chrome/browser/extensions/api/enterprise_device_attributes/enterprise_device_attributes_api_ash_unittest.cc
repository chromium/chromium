// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api_ash.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kFakeDirectoryApiId[] = "fake directory API ID";
constexpr char kFakeDeviceSerialNumber[] = "fake device serial number";
constexpr char kFakeAssetId[] = "fake asset ID";
constexpr char kFakeAnnotatedLocation[] = "fake annotated location";

std::string ParamToString(const testing::TestParamInfo<bool>& info) {
  return info.param ? "SignInProfile" : "NonSignInProfile";
}

}  // namespace

// The bool parameter tells if a test instance should use a sign in profile or
// not. This is useful because the extension APIs should return empty string on
// non sign in and non affiliated profiles.
class EnterpriseDeviceAttributesApiAshTest
    : public testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    // Set up fake device attributes.
    device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes_->SetFakeDirectoryApiId(kFakeDirectoryApiId);
    device_attributes_->SetFakeDeviceAssetId(kFakeAssetId);
    device_attributes_->SetFakeDeviceAnotatedLocation(kFakeAnnotatedLocation);

    // Set up fake serial number.
    statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, kFakeDeviceSerialNumber);
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    // Set up a testing profile. Needs to return true when passed to
    // crosapi::browser_util::IsSigninProfileOrBelongsToAffiliatedUser.
    TestingProfile::Builder builder;
    if (IsSignInProfile()) {
      builder.SetPath(
          base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
    }
    profile_ = builder.Build();
  }

  bool IsSignInProfile() { return GetParam(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::FakeDeviceAttributes> device_attributes_;
  std::unique_ptr<Profile> profile_;

 private:
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDirectoryDeviceIdFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction>(
      std::move(device_attributes_));

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile_.get());
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSignInProfile() ? kFakeDirectoryApiId : "", result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDeviceSerialNumberFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceSerialNumberFunction>();

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile_.get());
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSignInProfile() ? kFakeDeviceSerialNumber : "",
            result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest, GetDeviceAssetIdFunction) {
  auto function =
      base::MakeRefCounted<EnterpriseDeviceAttributesGetDeviceAssetIdFunction>(
          std::move(device_attributes_));

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile_.get());
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSignInProfile() ? kFakeAssetId : "", result->GetString());
}

TEST_P(EnterpriseDeviceAttributesApiAshTest,
       GetDeviceAnnotatedLocationFunction) {
  auto function = base::MakeRefCounted<
      EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction>(
      std::move(device_attributes_));

  std::unique_ptr<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile_.get());
  ASSERT_TRUE(result->is_string());
  EXPECT_EQ(IsSignInProfile() ? kFakeAnnotatedLocation : "",
            result->GetString());
}

INSTANTIATE_TEST_SUITE_P(All,
                         EnterpriseDeviceAttributesApiAshTest,
                         testing::Bool(),
                         &ParamToString);
}  // namespace extensions
