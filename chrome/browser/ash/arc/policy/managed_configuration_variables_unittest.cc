// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece_forward.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

Parameter SampleWithoutVariables() {
  // Set up an |input| Value without variables.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey("key1", "value1");
  input.SetStringKey("key2", "value2");

  // Expected |output| is the same as the input.
  base::Value output = input.Clone();

  return std::make_pair(std::move(input), std::move(output));
}

Parameter SampleWithVariables() {
  constexpr const char kUserEmailKey[] = "user_email";
  constexpr const char kUserNameKey[] = "user_name";
  constexpr const char kUserDomainKey[] = "user_domain";
  constexpr const char kDeviceSerialNumberKey[] = "device_serial_number";
  constexpr const char kDeviceDirectoryIdKey[] = "device_directory_id";
  constexpr const char kDeviceAssetIdKey[] = "device_asset_id";
  constexpr const char kDeviceLocationKey[] = "device_annotated_location_key";

  const std::string kUserEmailVariable =
      base::StringPrintf("${%s}", kUserEmail);
  const std::string kUserEmailNameVariable =
      base::StringPrintf("${%s}", kUserEmailName);
  const std::string kUserEmailDomainVariable =
      base::StringPrintf("${%s}", kUserEmailDomain);
  const std::string kDeviceSerialNumberVariable =
      base::StringPrintf("${%s}", kDeviceSerialNumber);
  const std::string kDeviceDirectoryIdVariable =
      base::StringPrintf("${%s}", kDeviceDirectoryId);
  const std::string kDeviceAssetIdVariable =
      base::StringPrintf("${%s}", kDeviceAssetId);
  const std::string kDeviceAnnotatedLocationVariable =
      base::StringPrintf("${%s}", kDeviceAnnotatedLocation);

  // Set up an |input| Value with some variables.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey(kUserEmailKey, kUserEmailVariable);
  input.SetStringKey(kUserNameKey, kUserEmailNameVariable);
  input.SetStringKey(kUserDomainKey, kUserEmailDomainVariable);
  input.SetStringKey(kDeviceSerialNumberKey, kDeviceSerialNumberVariable);
  input.SetStringKey(kDeviceDirectoryIdKey, kDeviceDirectoryIdVariable);
  input.SetStringKey(kDeviceAssetIdKey, kDeviceAssetIdVariable);
  input.SetStringKey(kDeviceLocationKey, kDeviceAnnotatedLocationVariable);

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

Parameter SampleWithNestedVariables() {
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
  constexpr const char kVariablePattern[] = "${%s}";

  const std::string kUserEmailVariable =
      base::StringPrintf(kVariablePattern, kUserEmail);
  const std::string kDeviceSerialNumberVariable =
      base::StringPrintf(kVariablePattern, kDeviceSerialNumber);

  // Set up an |input| Value with variables in nested values.
  base::Value nestedInput2(base::Value::Type::DICTIONARY);
  nestedInput2.SetStringKey(kEmailKey, kUserEmailVariable);
  nestedInput2.SetStringKey(kKey2, kValue2);
  nestedInput2.SetStringKey(kSerialNumberKey, kDeviceSerialNumberVariable);
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

Parameter SampleWithVariableChains() {
  constexpr const char kChain1[] = "chain1";
  constexpr const char kChain2[] = "chain2";
  constexpr const char kChain3[] = "chain3";
  constexpr const char kChain2Pattern[] = "${%s:%s}";
  constexpr const char kChain3Pattern[] = "${%s:%s:%s}";

  const std::string kChainVariable1 =
      base::StringPrintf(kChain2Pattern, kUserEmail, kUserEmailDomain);
  const std::string kChainVariable2 =
      base::StringPrintf(kChain3Pattern, kDeviceAssetId,
                         kDeviceAnnotatedLocation, kUserEmailDomain);
  const std::string kChainVariable3 = base::StringPrintf(
      kChain2Pattern, kDeviceAnnotatedLocation, kDeviceAssetId);

  // Set up an |input| Value with some variable chains.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey(kChain1, kChainVariable1);
  input.SetStringKey(kChain2, kChainVariable2);
  input.SetStringKey(kChain3, kChainVariable3);

  // Set up an |output| Value where variables have been replaced.
  base::Value output(base::Value::Type::DICTIONARY);
  output.SetStringKey(kChain1, kTestEmail);
  output.SetStringKey(kChain2, kTestDeviceAssetId);
  output.SetStringKey(kChain3, kTestDeviceAnnotatedLocation);

  return std::make_pair(std::move(input), std::move(output));
}

}  // namespace

class ManagedConfigurationVariablesTest
    : public testing::TestWithParam<ParameterGetter*> {
 protected:
  void SetUp() override {
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

  // Return the input parameter. Must be called from a parameterized test.
  base::Value& mutable_input() { return parameter().first; }

  // Return the expected output parameter. Must be called from a parameterized
  // test.
  const base::Value& expected_output() { return parameter().second; }

  policy::FakeDeviceAttributes* device_attributes() {
    return fake_device_attributes_.get();
  }

 private:
  Parameter& parameter() {
    if (!parameter_.has_value())
      parameter_ = (*GetParam())();
    return parameter_.value();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;

  chromeos::system::FakeStatisticsProvider statistics_provider_;

  std::unique_ptr<policy::FakeDeviceAttributes> fake_device_attributes_;

  absl::optional<Parameter> parameter_;
};

TEST_F(ManagedConfigurationVariablesTest, VariableChains) {
  // Setup a |dict| where values are chains of variables.
  constexpr const char kKey[] = "variable_chain";
  const std::string kChain =
      base::StringPrintf("${%s:%s:%s}", kDeviceAnnotatedLocation,
                         kDeviceAssetId, kDeviceDirectoryId);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kKey, kChain);

  // Initially all values in the chain are set, expect annotated location will
  // be returned.
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), &dict);
  EXPECT_EQ(*dict.FindStringKey(kKey), kTestDeviceAnnotatedLocation);

  // Clear location and expect chain resolves to asset ID.
  device_attributes()->SetFakeDeviceAnotatedLocation("");
  dict.SetStringKey(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), &dict);
  EXPECT_EQ(*dict.FindStringKey(kKey), kTestDeviceAssetId);

  // Clear asset ID and expect chain resolves to directory ID.
  device_attributes()->SetFakeDeviceAssetId("");
  dict.SetStringKey(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), &dict);
  EXPECT_EQ(*dict.FindStringKey(kKey), kTestDeviceDirectoryId);

  // Clear directory ID and expect chain resolves to the empty string.
  device_attributes()->SetFakeDirectoryApiId("");
  dict.SetStringKey(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), &dict);
  EXPECT_EQ(*dict.FindStringKey(kKey), "");
}

TEST_F(ManagedConfigurationVariablesTest, IgnoresInvalidVariables) {
  // Setup a |dict| where some values are valid variables and some are not.
  constexpr const char kValidKey[] = "valid_chain";
  constexpr const char kInvalidKey1[] = "invalid_chain1";
  constexpr const char kInvalidKey2[] = "invalid_chain2";
  constexpr const char kInvalidKey3[] = "invalid_chain3";
  constexpr const char kChain3Pattern[] = "${%s:%s:%s}";
  constexpr const char kWrongVariable[] = "USR_EMAIL";
  const std::string kValidChain =
      base::StringPrintf(kChain3Pattern, kDeviceAnnotatedLocation,
                         kDeviceAssetId, kDeviceDirectoryId);
  const std::string kInvalidChain1 = base::StringPrintf(
      kChain3Pattern, kWrongVariable, kDeviceAnnotatedLocation, kDeviceAssetId);
  const std::string kInvalidChain2 =
      base::StringPrintf("${LOOKS_LIKE_A_VARIABLE_BUT_ISNT}");
  const std::string kInvalidChain3 = base::StringPrintf(
      "${%s:DEVICE_ASsEt_ID:%s}", kDeviceAnnotatedLocation, kDeviceAssetId);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kValidKey, kValidChain);
  dict.SetStringKey(kInvalidKey1, kInvalidChain1);
  dict.SetStringKey(kInvalidKey2, kInvalidChain2);
  dict.SetStringKey(kInvalidKey3, kInvalidChain3);

  // Clear location, valid chain should resolve to asset ID.
  device_attributes()->SetFakeDeviceAnotatedLocation("");
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), &dict);
  // Expect the valid chain was replaced.
  EXPECT_EQ(*dict.FindStringKey(kValidKey), kTestDeviceAssetId);
  // Expect none of the invalid chains were replaced.
  EXPECT_EQ(*dict.FindStringKey(kInvalidKey1), kInvalidChain1);
  EXPECT_EQ(*dict.FindStringKey(kInvalidKey2), kInvalidChain2);
  EXPECT_EQ(*dict.FindStringKey(kInvalidKey3), kInvalidChain3);
}

TEST_P(ManagedConfigurationVariablesTest, ReplacesVariables) {
  RecursivelyReplaceManagedConfigurationVariables(
      profile(), device_attributes(), &mutable_input());
  EXPECT_EQ(mutable_input(), expected_output());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ManagedConfigurationVariablesTest,
                         testing::Values(&SampleWithoutVariables,
                                         &SampleWithVariables,
                                         &SampleWithNestedVariables,
                                         &SampleWithVariableChains));

}  // namespace arc
