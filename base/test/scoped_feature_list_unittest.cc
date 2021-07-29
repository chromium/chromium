// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include <map>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

namespace {

const Feature kTestFeature1{"TestFeature1", FEATURE_DISABLED_BY_DEFAULT};
const Feature kTestFeature2{"TestFeature2", FEATURE_DISABLED_BY_DEFAULT};

void ExpectFeatures(const std::string& enabled_features,
                    const std::string& disabled_features) {
  FeatureList* list = FeatureList::GetInstance();
  std::string actual_enabled_features;
  std::string actual_disabled_features;

  list->GetFeatureOverrides(&actual_enabled_features,
                            &actual_disabled_features);

  EXPECT_EQ(enabled_features, actual_enabled_features);
  EXPECT_EQ(disabled_features, actual_disabled_features);
}

}  // namespace

class ScopedFeatureListTest : public testing::Test {
 public:
  ScopedFeatureListTest() {
    // Clear default feature list.
    std::unique_ptr<FeatureList> feature_list(new FeatureList);
    feature_list->InitializeFromCommandLine(std::string(), std::string());
    original_feature_list_ = FeatureList::ClearInstanceForTesting();
    FeatureList::SetInstance(std::move(feature_list));
  }

  ~ScopedFeatureListTest() override {
    // Restore feature list.
    if (original_feature_list_) {
      FeatureList::ClearInstanceForTesting();
      FeatureList::RestoreInstanceForTesting(std::move(original_feature_list_));
    }
  }

 private:
  // Save the present FeatureList and restore it after test finish.
  std::unique_ptr<FeatureList> original_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFeatureListTest);
};

TEST_F(ScopedFeatureListTest, BasicScoped) {
  ExpectFeatures(std::string(), std::string());
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeature1));
  {
    test::ScopedFeatureList feature_list1;
    feature_list1.InitFromCommandLine("TestFeature1", std::string());
    ExpectFeatures("TestFeature1", std::string());
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  }
  ExpectFeatures(std::string(), std::string());
  EXPECT_FALSE(FeatureList::IsEnabled(kTestFeature1));
}

TEST_F(ScopedFeatureListTest, InitFromCommandLineWithFeatureParams) {
  const std::map<std::string, std::string> feature_params1 = {{"x", "uma"},
                                                              {"y", "ukm"}};
  const std::map<std::string, std::string> feature_params2 = {{"x", "ukm"},
                                                              {"y", "uma"}};

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("TestFeature1<foo.bar:x/uma/y/ukm", "");

  // Check initial state. Field trial and parameters should be set correctly.
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(1u, active_groups.size());
  FieldTrial* original_field_trial =
      FieldTrialList::Find(active_groups[0].trial_name);
  std::map<std::string, std::string> actualParams;
  EXPECT_TRUE(GetFieldTrialParamsByFeature(kTestFeature1, &actualParams));
  EXPECT_EQ(feature_params1, actualParams);

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature1,
                                                     feature_params2);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_NE(original_field_trial, FeatureList::GetFieldTrial(kTestFeature1));
    actualParams.clear();
    EXPECT_TRUE(GetFieldTrialParamsByFeature(kTestFeature1, &actualParams));
    EXPECT_EQ(feature_params2, actualParams);
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
  }

  // Check that initial state is restored.
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  active_groups.clear();
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(1u, active_groups.size());
  EXPECT_EQ(original_field_trial, FeatureList::GetFieldTrial(kTestFeature1));
  actualParams.clear();
  EXPECT_TRUE(GetFieldTrialParamsByFeature(kTestFeature1, &actualParams));
  EXPECT_EQ(feature_params1, actualParams);
}

TEST_F(ScopedFeatureListTest, EnableWithFeatureParameters) {
  const char kParam1[] = "param_1";
  const char kParam2[] = "param_2";
  const char kValue1[] = "value_1";
  const char kValue2[] = "value_2";
  std::map<std::string, std::string> parameters;
  parameters[kParam1] = kValue1;
  parameters[kParam2] = kValue2;

  ExpectFeatures(std::string(), std::string());
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(0u, active_groups.size());

  {
    test::ScopedFeatureList feature_list;

    feature_list.InitAndEnableFeatureWithParameters(kTestFeature1, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_EQ(kValue1,
              GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
    EXPECT_EQ(kValue2,
              GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
    active_groups.clear();
    FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
    EXPECT_EQ(1u, active_groups.size());
  }

  ExpectFeatures(std::string(), std::string());
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
  active_groups.clear();
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(0u, active_groups.size());
}

TEST_F(ScopedFeatureListTest, OverrideWithFeatureParameters) {
  scoped_refptr<FieldTrial> trial =
      FieldTrialList::CreateFieldTrial("foo", "bar");
  const char kParam[] = "param_1";
  const char kValue[] = "value_1";
  std::map<std::string, std::string> parameters;
  parameters[kParam] = kValue;

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("TestFeature1<foo,TestFeature2",
                                    std::string());

  // Check initial state.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature1, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_NE(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));

  {
    // Override feature with no existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature2, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_EQ(trial.get()->trial_name(),
              FeatureList::GetFieldTrial(kTestFeature1)->trial_name());
    EXPECT_EQ(trial.get()->group_name(),
              FeatureList::GetFieldTrial(kTestFeature1)->group_name());
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
}

TEST_F(ScopedFeatureListTest, OverrideMultipleFeaturesWithParameters) {
  scoped_refptr<FieldTrial> trial1 =
      FieldTrialList::CreateFieldTrial("foo1", "bar1");
  const char kParam[] = "param_1";
  const char kValue1[] = "value_1";
  const char kValue2[] = "value_2";
  std::map<std::string, std::string> parameters1;
  parameters1[kParam] = kValue1;
  std::map<std::string, std::string> parameters2;
  parameters2[kParam] = kValue2;

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("TestFeature1<foo1,TestFeature2",
                                    std::string());

  // Check initial state.
  ExpectFeatures("TestFeature1<foo1,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ("foo1", FeatureList::GetFieldTrial(kTestFeature1)->trial_name());
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));

  {
    // Override multiple features with parameters.
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeaturesAndParameters(
        {{kTestFeature1, parameters1}, {kTestFeature2, parameters2}}, {});

    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
  }

  {
    // Override a feature with a parameter and disable another one.
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeaturesAndParameters({{kTestFeature1, parameters2}},
                                                {kTestFeature2});

    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo1,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial1.get(), FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
}

TEST_F(ScopedFeatureListTest, ParamsWithSpecialCharsPreserved) {
  // Check that special characters in param names and values are preserved.
  const char kParam[] = ";_\\<:>/_!?";
  const char kValue[] = ",;:/'!?";
  FieldTrialParams params0 = {{kParam, kValue}};

  test::ScopedFeatureList feature_list0;
  feature_list0.InitWithFeaturesAndParameters({{kTestFeature1, params0}}, {});
  EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));

  {
    const char kValue1[] = "normal";
    FieldTrialParams params1 = {{kParam, kValue1}};
    test::ScopedFeatureList feature_list1;
    feature_list1.InitWithFeaturesAndParameters({{kTestFeature1, params1}}, {});

    EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  }
  EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));

  {
    const char kValue2[] = "[<(2)>]";
    FieldTrialParams params2 = {{kParam, kValue2}};
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeaturesAndParameters({{kTestFeature2, params2}}, {});

    EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  }
  EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
}

TEST_F(ScopedFeatureListTest, ParamsWithEmptyValue) {
  const char kParam[] = "p";
  const char kEmptyValue[] = "";
  FieldTrialParams params = {{kParam, kEmptyValue}};

  test::ScopedFeatureList feature_list0;
  feature_list0.InitWithFeaturesAndParameters({{kTestFeature1, params}}, {});
  EXPECT_EQ(kEmptyValue,
            GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  {
    const char kValue1[] = "normal";
    FieldTrialParams params1 = {{kParam, kValue1}};
    test::ScopedFeatureList feature_list1;
    feature_list1.InitWithFeaturesAndParameters({{kTestFeature1, params1}}, {});

    EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  }
  EXPECT_EQ(kEmptyValue,
            GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
}

TEST_F(ScopedFeatureListTest, EnableFeatureOverrideDisable) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitWithFeatures({}, {kTestFeature1});

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({kTestFeature1}, {});
    ExpectFeatures("TestFeature1", std::string());
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideNotMakeDuplicate) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitWithFeatures({}, {kTestFeature1});

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({}, {kTestFeature1});
    ExpectFeatures(std::string(), "TestFeature1");
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideFeatureWithDefault) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("*TestFeature1", std::string());

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({kTestFeature1}, {});
    ExpectFeatures("TestFeature1", std::string());
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideFeatureWithDefault2) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("*TestFeature1", std::string());

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({}, {kTestFeature1});
    ExpectFeatures(std::string(), "TestFeature1");
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideFeatureWithEnabledFieldTrial) {
  test::ScopedFeatureList feature_list1;

  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      kTestFeature1.name, FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  feature_list1.InitWithFeatureList(std::move(feature_list));

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({kTestFeature1}, {});
    ExpectFeatures("TestFeature1", std::string());
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideFeatureWithDisabledFieldTrial) {
  test::ScopedFeatureList feature_list1;

  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  FieldTrial* trial = FieldTrialList::CreateFieldTrial("TrialExample", "A");
  feature_list->RegisterFieldTrialOverride(
      kTestFeature1.name, FeatureList::OVERRIDE_DISABLE_FEATURE, trial);
  feature_list1.InitWithFeatureList(std::move(feature_list));

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({kTestFeature1}, {});
    ExpectFeatures("TestFeature1", std::string());
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideKeepsOtherExistingFeature) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitWithFeatures({}, {kTestFeature1});

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({}, {kTestFeature2});
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_FALSE(FeatureList::IsEnabled(kTestFeature2));
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideKeepsOtherExistingFeature2) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitWithFeatures({}, {kTestFeature1});

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({kTestFeature2}, {});
    ExpectFeatures("TestFeature2", "TestFeature1");
  }
}

TEST_F(ScopedFeatureListTest, FeatureOverrideKeepsOtherExistingDefaultFeature) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("*TestFeature1", std::string());

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatures({}, {kTestFeature2});
    ExpectFeatures("*TestFeature1", "TestFeature2");
  }
}

TEST_F(ScopedFeatureListTest, ScopedFeatureListIsNoopWhenNotInitialized) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("*TestFeature1", std::string());

  // A ScopedFeatureList on which Init() is not called should not reset things
  // when going out of scope.
  { test::ScopedFeatureList feature_list2; }

  ExpectFeatures("*TestFeature1", std::string());
}

TEST(ScopedFeatureListTestWithMemberList, ScopedFeatureListLocalOverride) {
  test::ScopedFeatureList initial_feature_list;
  initial_feature_list.InitAndDisableFeature(kTestFeature1);
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(kTestFeature1,
                                                       {{"mode", "nobugs"}});
    ASSERT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  }
}

}  // namespace test
}  // namespace base
