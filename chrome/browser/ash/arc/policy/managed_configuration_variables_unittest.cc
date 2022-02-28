// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace arc {

namespace {

typedef std::pair</*input=*/base::Value, /*expected_output=*/base::Value>
    Parameter;
typedef Parameter ParameterGetter();

constexpr char kTestEmail[] = "username@somedomain.com";
constexpr char kTestEmailName[] = "username";
constexpr char kTestEmailDomain[] = "somedomain.com";
constexpr char kTestDeviceSerialNumber[] = "CAFE1337";
constexpr char kTestDeviceDirectoryId[] = "85729104-ef7a-5718d62e72ca";
constexpr char kTestDeviceAssetId[] = "admin provided test asset ID";
constexpr char kTestDeviceAnnotatedLocation[] = "admin provided test location";

Parameter sampleWithoutVariables() {
  // Set up an |input| Value without variables.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey("key1", "value1");
  input.SetStringKey("key2", "value2");

  // Expected |output| is the same as the input.
  base::Value output = input.Clone();

  return std::make_pair(std::move(input), std::move(output));
}

Parameter sampleWithVariables() {
  constexpr const char kUserEmailKey[] = "user_email";
  constexpr const char kUserNameKey[] = "user_name";
  constexpr const char kUserDomainKey[] = "user_domain";
  constexpr const char kDeviceSerialNumberKey[] = "device_serial_number";
  constexpr const char kDeviceDirectoryIdKey[] = "device_directory_id";
  constexpr const char kDeviceAssetIdKey[] = "device_asset_id";
  constexpr const char kDeviceLocationKey[] = "device_annotated_location_key";

  // Set up an |input| Value with some variables.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey(kUserEmailKey, kUserEmail);
  input.SetStringKey(kUserNameKey, kUserEmailName);
  input.SetStringKey(kUserDomainKey, kUserEmailDomain);
  input.SetStringKey(kDeviceSerialNumberKey, kDeviceSerialNumber);
  input.SetStringKey(kDeviceDirectoryIdKey, kDeviceDirectoryId);
  input.SetStringKey(kDeviceAssetIdKey, kDeviceAssetId);
  input.SetStringKey(kDeviceLocationKey, kDeviceAnnotatedLocation);

  // Set up an |output| Value where variables have been replaced.
  base::Value output(base::Value::Type::DICTIONARY);
  output.SetStringKey(kUserEmailKey, kTestEmail);
  output.SetStringKey(kUserNameKey, kTestEmailName);
  output.SetStringKey(kUserDomainKey, kTestEmailDomain);
  output.SetStringKey(kDeviceSerialNumberKey, kTestDeviceSerialNumber);
  output.SetStringKey(kDeviceDirectoryIdKey, kTestDeviceDirectoryId);
  output.SetStringKey(kDeviceAssetIdKey, kTestDeviceAssetId);
  output.SetStringKey(kDeviceLocationKey, kTestDeviceAnnotatedLocation);

  return std::make_pair(std::move(input), std::move(output));
}

Parameter sampleWithNestedVariables() {
  constexpr const char kNameKey[] = "name";
  constexpr const char kEmailKey[] = "email";
  constexpr const char kSerialNumberKey[] = "serial_number";
  constexpr const char kSubKey[] = "sub";
  constexpr const char kSubSubKey[] = "subsub";
  constexpr const char kNestedEmailKey[] = "sub.subsub.email";
  constexpr const char kNestedSerialNumberKey[] = "sub.subsub.serial_number";
  constexpr const char kKey0[] = "key0";
  constexpr const char kKey1[] = "key1";
  constexpr const char kKey2[] = "key2";
  constexpr const char kValue0[] = "value0";
  constexpr const char kValue1[] = "value1";
  constexpr const char kValue2[] = "value2";

  // Set up an |input| Value with variables in nested values.
  base::Value nestedInput2(base::Value::Type::DICTIONARY);
  nestedInput2.SetStringKey(kEmailKey, kUserEmail);
  nestedInput2.SetStringKey(kKey2, kValue2);
  nestedInput2.SetStringKey(kSerialNumberKey, kDeviceSerialNumber);
  base::Value nestedInput1(base::Value::Type::DICTIONARY);
  nestedInput1.SetKey(kSubSubKey, std::move(nestedInput2));
  nestedInput1.SetStringKey(kKey1, kValue1);
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey(kKey0, kValue0);
  input.SetKey(kSubKey, std::move(nestedInput1));
  input.SetStringKey(kNameKey, kTestEmailName);

  // |output| is the same as |input| except the variables have been replaced.
  base::Value output = input.Clone();
  output.SetStringKey(kNameKey, kTestEmailName);
  output.SetStringPath(kNestedEmailKey, kTestEmail);
  output.SetStringPath(kNestedSerialNumberKey, kTestDeviceSerialNumber);

  return std::make_pair(std::move(input), std::move(output));
}

}  // namespace

class ManagedConfigurationVariablesTest
    : public testing::TestWithParam<ParameterGetter*> {
 protected:
  void SetUp() override {
    // Run the ParameterGetter.
    parameter = (*GetParam())();

    // Set up fake StatisticsProvider.
    statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, kTestDeviceSerialNumber);
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    // Set up a fake user and capture its profile.
    profile_.reset(IdentityTestEnvironmentProfileAdaptor::
                       CreateProfileForIdentityTestEnvironment()
                           .release());
    IdentityTestEnvironmentProfileAdaptor adaptor(profile_.get());
    adaptor.identity_test_env()->SetPrimaryAccount(kTestEmail,
                                                   signin::ConsentLevel::kSync);

    // Set up fake device attributes.
    fake_device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    fake_device_attributes_->SetFakeDirectoryApiId(kTestDeviceDirectoryId);
    fake_device_attributes_->SetFakeDeviceAssetId(kTestDeviceAssetId);
    fake_device_attributes_->SetFakeDeviceAnotatedLocation(
        kTestDeviceAnnotatedLocation);
  }

  void TearDown() override { profile_.reset(); }

  const Profile* profile() { return profile_.get(); }

  base::Value& mutable_input() { return parameter.first; }

  const base::Value& expected_output() { return parameter.second; }

  policy::FakeDeviceAttributes* device_attributes() {
    return fake_device_attributes_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;

  chromeos::system::FakeStatisticsProvider statistics_provider_;

  std::unique_ptr<policy::FakeDeviceAttributes> fake_device_attributes_;

  Parameter parameter;
};

TEST_P(ManagedConfigurationVariablesTest, ReplacesVariables) {
  RecursivelyReplaceManagedConfigurationVariables(
      profile(), device_attributes(), &mutable_input());
  EXPECT_EQ(mutable_input(), expected_output());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ManagedConfigurationVariablesTest,
                         testing::Values(&sampleWithoutVariables,
                                         &sampleWithVariables,
                                         &sampleWithNestedVariables));

}  // namespace arc
