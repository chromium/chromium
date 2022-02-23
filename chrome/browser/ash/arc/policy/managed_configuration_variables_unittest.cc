// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
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

  // Set up an |input| Value with some variables.
  base::Value input(base::Value::Type::DICTIONARY);
  input.SetStringKey(kUserEmailKey, kUserEmail);
  input.SetStringKey(kUserNameKey, kUserEmailName);
  input.SetStringKey(kUserDomainKey, kUserEmailDomain);
  input.SetStringKey(kDeviceSerialNumberKey, kDeviceSerialNumber);

  // Set up an |output| Value where variables have been replaced.
  base::Value output(base::Value::Type::DICTIONARY);
  output.SetStringKey(kUserEmailKey, kTestEmail);
  output.SetStringKey(kUserNameKey, kTestEmailName);
  output.SetStringKey(kUserDomainKey, kTestEmailDomain);
  output.SetStringKey(kDeviceSerialNumberKey, kTestDeviceSerialNumber);

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
  }

  void TearDown() override { profile_.reset(); }

  const Profile* profile() { return profile_.get(); }

  base::Value& mutable_input() { return parameter.first; }

  const base::Value& expected_output() { return parameter.second; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;

  chromeos::system::FakeStatisticsProvider statistics_provider_;

  Parameter parameter;
};

TEST_P(ManagedConfigurationVariablesTest, ReplacesVariables) {
  RecursivelyReplaceManagedConfigurationVariables(profile(), &mutable_input());
  EXPECT_EQ(mutable_input(), expected_output());
}

// TODO(http://b/219948590) Update samples to test device attributes.
INSTANTIATE_TEST_SUITE_P(All,
                         ManagedConfigurationVariablesTest,
                         testing::Values(&sampleWithoutVariables,
                                         &sampleWithVariables,
                                         &sampleWithNestedVariables));

}  // namespace arc
