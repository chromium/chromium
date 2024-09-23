// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

typedef std::pair</*input=*/base::Value::Dict,
                  /*expected_output=*/base::Value::Dict>
    Parameter;
typedef Parameter ParameterGetter(bool);

constexpr const char kKey1[] = "key1";
constexpr const char kKey2[] = "key2";

constexpr char kTestGaiaId[] = "0123456789";
constexpr char kTestEmail[] = "username@somedomain.com";
constexpr char kTestEmailName[] = "username";
constexpr char kTestEmailDomain[] = "somedomain.com";
constexpr char kTestDeviceSerialNumber[] = "CAFE1337";
constexpr char kTestDeviceDirectoryId[] = "85729104-ef7a-5718d62e72ca";
constexpr char kTestDeviceAssetId[] = "admin provided test asset ID";
constexpr char kTestDeviceAnnotatedLocation[] = "admin provided test location";
constexpr const char kVariablePattern[] = "${%s}";

Parameter SampleWithoutVariables(bool is_affiliated) {
  // Set up an |input| Value without variables.
  base::Value::Dict input;
  input.Set(kKey1, "value1");
  input.Set(kKey2, "value2");

  // Expected |output| is the same as the input.
  base::Value::Dict output = input.Clone();

  return std::make_pair(std::move(input), std::move(output));
}

Parameter SampleWithVariables(bool is_affiliated) {
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
  base::Value::Dict input;
  input.Set(kUserEmailKey, kUserEmailVariable);
  input.Set(kUserNameKey, kUserEmailNameVariable);
  input.Set(kUserDomainKey, kUserEmailDomainVariable);
  input.Set(kDeviceSerialNumberKey, kDeviceSerialNumberVariable);
  input.Set(kDeviceDirectoryIdKey, kDeviceDirectoryIdVariable);
  input.Set(kDeviceAssetIdKey, kDeviceAssetIdVariable);
  input.Set(kDeviceLocationKey, kDeviceAnnotatedLocationVariable);

  // Set up an |output| Value where variables have been replaced.
  base::Value::Dict output;
  output.Set(kUserEmailKey, kTestEmail);
  output.Set(kUserNameKey, kTestEmailName);
  output.Set(kUserDomainKey, kTestEmailDomain);
  output.Set(kDeviceSerialNumberKey,
             is_affiliated ? kTestDeviceSerialNumber : "");
  output.Set(kDeviceDirectoryIdKey,
             is_affiliated ? kTestDeviceDirectoryId : "");
  output.Set(kDeviceAssetIdKey, is_affiliated ? kTestDeviceAssetId : "");
  output.Set(kDeviceLocationKey,
             is_affiliated ? kTestDeviceAnnotatedLocation : "");

  return std::make_pair(std::move(input), std::move(output));
}

Parameter SampleWithNestedVariables(bool is_affiliated) {
  constexpr const char kNameKey[] = "name";
  constexpr const char kEmailKey[] = "email";
  constexpr const char kSerialNumberKey[] = "serial_number";
  constexpr const char kSubKey[] = "sub";
  constexpr const char kSubSubKey[] = "subsub";
  constexpr const char kNestedEmailKey[] = "sub.subsub.email";
  constexpr const char kNestedSerialNumberKey[] = "sub.subsub.serial_number";
  constexpr const char kKey0[] = "key0";
  constexpr const char kValue0[] = "value0";
  constexpr const char kValue1[] = "value1";
  constexpr const char kValue2[] = "value2";

  const std::string kUserEmailVariable =
      base::StringPrintf(kVariablePattern, kUserEmail);
  const std::string kDeviceSerialNumberVariable =
      base::StringPrintf(kVariablePattern, kDeviceSerialNumber);

  // Set up an |input| Value with variables in nested values.
  base::Value::Dict nestedInput2;
  nestedInput2.Set(kEmailKey, kUserEmailVariable);
  nestedInput2.Set(kKey2, kValue2);
  nestedInput2.Set(kSerialNumberKey, kDeviceSerialNumberVariable);
  base::Value::Dict nestedInput1;
  nestedInput1.Set(kSubSubKey, std::move(nestedInput2));
  nestedInput1.Set(kKey1, kValue1);
  base::Value::Dict input;
  input.Set(kKey0, kValue0);
  input.Set(kSubKey, std::move(nestedInput1));
  input.Set(kNameKey, kTestEmailName);

  // |output| is the same as |input| except the variables have been replaced.
  base::Value::Dict output = input.Clone();
  output.Set(kNameKey, kTestEmailName);
  output.SetByDottedPath(kNestedEmailKey, kTestEmail);
  output.SetByDottedPath(kNestedSerialNumberKey,
                         is_affiliated ? kTestDeviceSerialNumber : "");

  return std::make_pair(std::move(input), std::move(output));
}

Parameter SampleWithVariableChains(bool is_affiliated) {
  constexpr const char kChain1[] = "chain1";
  constexpr const char kChain2[] = "chain2";
  constexpr const char kChain3[] = "chain3";
  constexpr const char kChain2Pattern[] = "chain ${%s:%s} like so";
  constexpr const char kChain3Pattern[] = "chain ${%s:%s:%s} like so";
  constexpr const char kChainReplacedPattern[] = "chain %s like so";

  const std::string kChainVariable1 =
      base::StringPrintf(kChain2Pattern, kUserEmail, kUserEmailDomain);
  const std::string kChainVariable2 =
      base::StringPrintf(kChain3Pattern, kDeviceAssetId,
                         kDeviceAnnotatedLocation, kUserEmailDomain);
  const std::string kChainVariable3 = base::StringPrintf(
      kChain2Pattern, kDeviceAnnotatedLocation, kDeviceAssetId);

  const std::string kReplacedChain1 =
      base::StringPrintf(kChainReplacedPattern, kTestEmail);
  const std::string kReplacedChain2 =
      base::StringPrintf(kChainReplacedPattern,
                         is_affiliated ? kTestDeviceAssetId : kTestEmailDomain);
  const std::string kReplacedChain3 = base::StringPrintf(
      kChainReplacedPattern, is_affiliated ? kTestDeviceAnnotatedLocation : "");

  // Set up an |input| Value with some variable chains.
  base::Value::Dict input;
  input.Set(kChain1, kChainVariable1);
  input.Set(kChain2, kChainVariable2);
  input.Set(kChain3, kChainVariable3);

  // Set up an |output| Value where variables have been replaced.
  base::Value::Dict output;
  output.Set(kChain1, kReplacedChain1);
  output.Set(kChain2, kReplacedChain2);
  output.Set(kChain3, kReplacedChain3);

  return std::make_pair(std::move(input), std::move(output));
}

}  // namespace

class ManagedConfigurationVariablesBase {
 public:
  void DoSetUp(bool is_affiliated) {
    // Set up fake StatisticsProvider.
    statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                             kTestDeviceSerialNumber);
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Set up a fake user and capture its profile.
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kTestEmail, kTestGaiaId));
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        kTestEmail, IdentityTestEnvironmentProfileAdaptor::
                        GetIdentityTestEnvironmentFactories());
    ASSERT_TRUE(profile_);

    const auto adaptor =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    adaptor->identity_test_env()->SetPrimaryAccount(
        kTestEmail, signin::ConsentLevel::kSignin);

    // Set up fake device attributes.
    fake_device_attributes_ = std::make_unique<policy::FakeDeviceAttributes>();
    fake_device_attributes_->SetFakeDirectoryApiId(kTestDeviceDirectoryId);
    fake_device_attributes_->SetFakeDeviceAssetId(kTestDeviceAssetId);
    fake_device_attributes_->SetFakeDeviceAnnotatedLocation(
        kTestDeviceAnnotatedLocation);
  }

  void DoTearDown() {
    fake_device_attributes_.reset();
    profile_manager_.reset();
    fake_user_manager_.Reset();
  }

  const Profile* profile() { return profile_; }

  policy::FakeDeviceAttributes* device_attributes() {
    return fake_device_attributes_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<TestingProfileManager> profile_manager_;

  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

  ash::system::FakeStatisticsProvider statistics_provider_;

  std::unique_ptr<policy::FakeDeviceAttributes> fake_device_attributes_;
};

class ManagedConfigurationVariablesTest
    : public ManagedConfigurationVariablesBase,
      public testing::Test {
 protected:
  void SetUp() override { DoSetUp(/*is_affiliated=*/true); }
  void TearDown() override { DoTearDown(); }
};

class ManagedConfigurationVariablesAffiliatedTest
    : public ManagedConfigurationVariablesBase,
      public testing::TestWithParam<std::tuple<bool, ParameterGetter*>> {
 protected:
  void SetUp() override { DoSetUp(is_affiliated()); }

  void TearDown() override { DoTearDown(); }

  // Return the input parameter.
  base::Value::Dict& mutable_input() { return parameter().first; }

  // Return the expected output parameter.
  const base::Value::Dict& expected_output() { return parameter().second; }

 private:
  bool is_affiliated() { return std::get<0>(GetParam()); }

  Parameter& parameter() {
    if (!parameter_.has_value())
      parameter_ = (*std::get<1>(GetParam()))(is_affiliated());
    return parameter_.value();
  }

  std::optional<Parameter> parameter_;
};

TEST_F(ManagedConfigurationVariablesTest, VariableChains) {
  // Setup a |dict| where values are chains of variables.
  constexpr const char kKey[] = "variable_chain";
  const std::string kChain =
      base::StringPrintf("${%s:%s:%s}", kDeviceAnnotatedLocation,
                         kDeviceAssetId, kDeviceDirectoryId);
  base::Value::Dict dict;
  dict.Set(kKey, kChain);

  // Initially all values in the chain are set, expect annotated location will
  // be returned.
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  EXPECT_EQ(*dict.FindString(kKey), kTestDeviceAnnotatedLocation);

  // Clear location and expect chain resolves to asset ID.
  device_attributes()->SetFakeDeviceAnnotatedLocation("");
  dict.Set(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  EXPECT_EQ(*dict.FindString(kKey), kTestDeviceAssetId);

  // Clear asset ID and expect chain resolves to directory ID.
  device_attributes()->SetFakeDeviceAssetId("");
  dict.Set(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  EXPECT_EQ(*dict.FindString(kKey), kTestDeviceDirectoryId);

  // Clear directory ID and expect chain resolves to the empty string.
  device_attributes()->SetFakeDirectoryApiId("");
  dict.Set(kKey, kChain);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  EXPECT_EQ(*dict.FindString(kKey), "");
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

  base::Value::Dict dict;
  dict.Set(kValidKey, kValidChain);
  dict.Set(kInvalidKey1, kInvalidChain1);
  dict.Set(kInvalidKey2, kInvalidChain2);
  dict.Set(kInvalidKey3, kInvalidChain3);

  // Clear location, valid chain should resolve to asset ID.
  device_attributes()->SetFakeDeviceAnnotatedLocation("");
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  // Expect the valid chain was replaced.
  EXPECT_EQ(*dict.FindString(kValidKey), kTestDeviceAssetId);
  // Expect none of the invalid chains were replaced.
  EXPECT_EQ(*dict.FindString(kInvalidKey1), kInvalidChain1);
  EXPECT_EQ(*dict.FindString(kInvalidKey2), kInvalidChain2);
  EXPECT_EQ(*dict.FindString(kInvalidKey3), kInvalidChain3);
}

TEST_F(ManagedConfigurationVariablesTest, RespectsSpecialCharacters) {
  // Setup a |dict| with an asset ID variable.
  const std::string kVariable =
      base::StringPrintf(kVariablePattern, kDeviceAssetId);

  base::Value::Dict dict;
  dict.Set(kKey1, kVariable);

  // Setup a fake asset ID using special characters.
  constexpr char kSpecialCharacters[] =
      "`~!@#$%^&*(),_-+={[}}|\\\\:,;\"'<,>.?/{}\",";
  device_attributes()->SetFakeDeviceAssetId(kSpecialCharacters);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  // Expect special characters were replaced correctly.
  EXPECT_EQ(*dict.FindString(kKey1), kSpecialCharacters);
}

TEST_F(ManagedConfigurationVariablesTest,
       RecursiveValuesAreNotReplacedMoreThanOnce) {
  // Setup a |dict| with asset ID and location variables.
  const std::string kVariable1 =
      base::StringPrintf(kVariablePattern, kDeviceAssetId);
  const std::string kVariable2 =
      base::StringPrintf(kVariablePattern, kDeviceAnnotatedLocation);

  base::Value::Dict dict;
  dict.Set(kKey1, kVariable1);
  dict.Set(kKey2, kVariable2);

  // Setup fake asset ID and location that are also valid variables.
  device_attributes()->SetFakeDeviceAssetId(kVariable2);
  device_attributes()->SetFakeDeviceAnnotatedLocation(kVariable1);
  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);
  // Expect variables are replaced only once without an infinite loop.
  EXPECT_EQ(*dict.FindString(kKey1), kVariable2);
  EXPECT_EQ(*dict.FindString(kKey2), kVariable1);
}

TEST_F(ManagedConfigurationVariablesTest, ReplacesVariablesInLists) {
  const std::string kVariable1 =
      base::StringPrintf(kVariablePattern, kDeviceAssetId);

  base::Value::Dict dict = base::Value::Dict().Set(
      kKey1,
      base::Value::List().Append(base::Value::Dict().Set(kKey2, kVariable1)));

  device_attributes()->SetFakeDeviceAssetId(kTestDeviceAssetId);

  RecursivelyReplaceManagedConfigurationVariables(profile(),
                                                  device_attributes(), dict);

  ASSERT_EQ(1U, dict.size());
  base::Value::List* nestedList = dict.FindList(kKey1);
  ASSERT_NE(nullptr, nestedList);
  ASSERT_EQ(1U, nestedList->size());
  base::Value::Dict& leafDict = (*nestedList)[0].GetDict();
  ASSERT_EQ(kTestDeviceAssetId, *leafDict.FindString(kKey2));
}

TEST_P(ManagedConfigurationVariablesAffiliatedTest, ReplacesVariables) {
  RecursivelyReplaceManagedConfigurationVariables(
      profile(), device_attributes(), mutable_input());
  EXPECT_EQ(mutable_input(), expected_output());
}

INSTANTIATE_TEST_SUITE_P(
    WIP,
    ManagedConfigurationVariablesAffiliatedTest,
    testing::Combine(testing::Bool(),
                     testing::Values(&SampleWithoutVariables,
                                     &SampleWithVariables,
                                     &SampleWithNestedVariables,
                                     &SampleWithVariableChains)));

}  // namespace arc
