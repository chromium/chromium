// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"

#include <map>
#include <string>
#include <utility>

#include "base/features.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

BASE_FEATURE(kTestFeature1, "TestFeature1", FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2, "TestFeature2", FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kTestFeatureParam1,
                   &kTestFeature1,
                   "TestFeatureParam1",
                   false);

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

std::string GetActiveFieldTrialGroupName(const std::string& trial_name) {
  FieldTrial::ActiveGroups groups;
  FieldTrialList::GetActiveFieldTrialGroups(&groups);
  for (const auto& group : groups) {
    if (group.trial_name == trial_name) {
      return group.group_name;
    }
  }
  return std::string();
}

}  // namespace

class ScopedFeatureListTest : public testing::Test {
 public:
  ScopedFeatureListTest() {
    // Clear default feature list.
    std::unique_ptr<FeatureList> feature_list(new FeatureList);
    feature_list->InitFromCommandLine(std::string(), std::string());
    original_feature_list_ = FeatureList::ClearInstanceForTesting();
    FeatureList::SetInstance(std::move(feature_list));
  }

  ScopedFeatureListTest(const ScopedFeatureListTest&) = delete;
  ScopedFeatureListTest& operator=(const ScopedFeatureListTest&) = delete;

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
  const char kValue0[] = "value_0";
  std::map<std::string, std::string> parameters;
  parameters[kParam] = kValue;

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine(
      "TestFeature1<foo.bar:param_1/value_0,TestFeature2", std::string());

  // Check initial state.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  // ScopedFeatureList always scope features, field trials and their associated
  // parameters. So ScopedFeatureList::InitFromCommandLine() creates another
  // FieldTrial instance whose trial name, group name and associated parameters
  // are the same as |trial|, and changes |kTestFeature1|'s field trial to
  // be the newly created one.
  EXPECT_NE(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
  EXPECT_EQ("bar", GetActiveFieldTrialGroupName("foo"));

  FieldTrial* trial_for_test_feature1 =
      FeatureList::GetFieldTrial(kTestFeature1);
  EXPECT_EQ("foo", trial_for_test_feature1->trial_name());
  EXPECT_EQ("bar", trial_for_test_feature1->group_name());

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature1, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_NE(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(trial_for_test_feature1,
              FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  }

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitFromCommandLine("TestFeature1<foo.bar2:param_1/value_1",
                                      std::string());
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_NE(trial.get(), FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(trial_for_test_feature1,
              FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));

    // foo's active group is now bar2, not bar.
    EXPECT_TRUE(FieldTrialList::IsTrialActive("foo"));
    EXPECT_EQ("bar2", GetActiveFieldTrialGroupName("foo"));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial_for_test_feature1, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
  // foo's active group is bar, because initial state is restored.
  EXPECT_EQ("bar", GetActiveFieldTrialGroupName("foo"));

  {
    // Override feature with no existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature2, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
    EXPECT_EQ(kValue, GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
    EXPECT_EQ(trial_for_test_feature1->trial_name(),
              FeatureList::GetFieldTrial(kTestFeature1)->trial_name());
    EXPECT_EQ(trial_for_test_feature1->group_name(),
              FeatureList::GetFieldTrial(kTestFeature1)->group_name());
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial_for_test_feature1, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));
}

TEST_F(ScopedFeatureListTest, OverrideWithFeatureMultipleParameters) {
  const char kParam1[] = "param_1";
  const char kValue1[] = "value_1";
  const char kParam2[] = "param_2";
  const char kValue2[] = "value_2";
  const char kValue3[] = "value_3";
  std::map<std::string, std::string> parameters;
  parameters[kParam1] = kValue3;

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine(
      "TestFeature1<foo.bar:param_1/value_1/param_2/"
      "value_2,TestFeature2:param_1/value_2",
      std::string());

  // Check initial state.
  ExpectFeatures("TestFeature1<foo,TestFeature2<StudyTestFeature2",
                 std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature2, kParam1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam2));

  FieldTrial* trial = FieldTrialList::Find("foo");
  EXPECT_EQ("bar", trial->GetGroupNameWithoutActivation());
  EXPECT_EQ("bar", GetActiveFieldTrialGroupName("foo"));

  FieldTrial* trial2 = FieldTrialList::Find("StudyTestFeature2");
  EXPECT_EQ("GroupTestFeature2", trial2->GetGroupNameWithoutActivation());
  EXPECT_EQ("GroupTestFeature2",
            GetActiveFieldTrialGroupName("StudyTestFeature2"));

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitAndEnableFeatureWithParameters(kTestFeature1, parameters);
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ(kValue3,
              GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
    // param_2 is not set.
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
    EXPECT_EQ(kValue2,
              GetFieldTrialParamValueByFeature(kTestFeature2, kParam1));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam2));
    EXPECT_NE(trial, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(trial2, FeatureList::GetFieldTrial(kTestFeature2));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2<StudyTestFeature2",
                 std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(trial2, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature2, kParam1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam2));
  // foo's active group is bar, because initial state is restored.
  EXPECT_EQ("bar", GetActiveFieldTrialGroupName("foo"));
  EXPECT_EQ("GroupTestFeature2",
            GetActiveFieldTrialGroupName("StudyTestFeature2"));

  {
    // Override feature with existing field trial.
    test::ScopedFeatureList feature_list2;

    feature_list2.InitFromCommandLine("TestFeature1<foo.bar2:param_2/value_3",
                                      std::string());
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
    EXPECT_EQ(kValue3,
              GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
    EXPECT_EQ(kValue2,
              GetFieldTrialParamValueByFeature(kTestFeature2, kParam1));
    EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam2));
    EXPECT_NE(trial, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature1));
    EXPECT_EQ("foo", FeatureList::GetFieldTrial(kTestFeature1)->trial_name());
    EXPECT_EQ("bar2", FeatureList::GetFieldTrial(kTestFeature1)->group_name());
    EXPECT_NE(trial2, FeatureList::GetFieldTrial(kTestFeature2));
    EXPECT_NE(nullptr, FeatureList::GetFieldTrial(kTestFeature2));

    // foo's active group is now bar2, not bar.
    EXPECT_TRUE(FieldTrialList::IsTrialActive("foo"));
    EXPECT_EQ("bar2", GetActiveFieldTrialGroupName("foo"));
  }

  // Check that initial state is restored.
  ExpectFeatures("TestFeature1<foo,TestFeature2<StudyTestFeature2",
                 std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ(trial, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(trial2, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue1, GetFieldTrialParamValueByFeature(kTestFeature1, kParam1));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature1, kParam2));
  EXPECT_EQ(kValue2, GetFieldTrialParamValueByFeature(kTestFeature2, kParam1));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam2));
  EXPECT_EQ("bar", GetActiveFieldTrialGroupName("foo"));
}

TEST_F(ScopedFeatureListTest, OverrideMultipleFeaturesWithParameters) {
  scoped_refptr<FieldTrial> trial1 =
      FieldTrialList::CreateFieldTrial("foo1", "bar1");
  const char kParam[] = "param_1";
  const char kValue0[] = "value_0";
  const char kValue1[] = "value_1";
  const char kValue2[] = "value_2";
  std::map<std::string, std::string> parameters1;
  parameters1[kParam] = kValue1;
  std::map<std::string, std::string> parameters2;
  parameters2[kParam] = kValue2;

  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine(
      "TestFeature1<foo1:param_1/value_0,TestFeature2", std::string());

  // Check initial state.
  ExpectFeatures("TestFeature1<foo1,TestFeature2", std::string());
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature2));
  EXPECT_EQ("foo1", FeatureList::GetFieldTrial(kTestFeature1)->trial_name());
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
  EXPECT_EQ("", GetFieldTrialParamValueByFeature(kTestFeature2, kParam));

  // InitFromCommandLine() scopes field trials.
  FieldTrial* trial = FieldTrialList::Find("foo1");
  // --enable-features will create a group whose name is "Group" + feature
  // name if no group name is specified. In this case, the feature name is
  // "TestFeature1". So the group name is "GroupTestFeature1".
  EXPECT_EQ("GroupTestFeature1", trial->GetGroupNameWithoutActivation());
  EXPECT_NE(trial1.get(), trial);
  // Because of "scoped", "bar1" disappears. Instead, "GroupTestFeature1"
  // is created and activated.
  EXPECT_NE("bar1", GetActiveFieldTrialGroupName("foo1"));
  EXPECT_EQ("GroupTestFeature1", GetActiveFieldTrialGroupName("foo1"));

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
  EXPECT_EQ(trial, FeatureList::GetFieldTrial(kTestFeature1));
  EXPECT_EQ(nullptr, FeatureList::GetFieldTrial(kTestFeature2));
  EXPECT_EQ(kValue0, GetFieldTrialParamValueByFeature(kTestFeature1, kParam));
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

TEST_F(ScopedFeatureListTest,
       RestoreFieldTrialParamsCorrectlyWhenLeakedFieldTrialCreated) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitFromCommandLine("TestFeature1:TestParam/TestValue1", "");
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_EQ("TestValue1",
            GetFieldTrialParamValueByFeature(kTestFeature1, "TestParam"));

  // content::InitializeFieldTrialAndFeatureList() creates a leaked
  // FieldTrialList. To emulate the leaked one, declare
  // unique_ptr<FieldTriaList> here and initialize it inside the following
  // child scope.
  std::unique_ptr<FieldTrialList> leaked_field_trial_list;
  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithNullFeatureAndFieldTrialLists();

    leaked_field_trial_list = std::make_unique<FieldTrialList>();
    FeatureList::InitInstance("TestFeature1:TestParam/TestValue2", "", {});
    EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
    EXPECT_EQ("TestValue2",
              GetFieldTrialParamValueByFeature(kTestFeature1, "TestParam"));
  }
  EXPECT_TRUE(FeatureList::IsEnabled(kTestFeature1));
  EXPECT_EQ("TestValue1",
            GetFieldTrialParamValueByFeature(kTestFeature1, "TestParam"));

  {
    FieldTrialList* backup_field_trial =
        FieldTrialList::BackupInstanceForTesting();

    // To free leaked_field_trial_list, need RestoreInstanceForTesting()
    // to pass DCHECK_EQ(this, global_) at ~FieldTrialList().
    FieldTrialList::RestoreInstanceForTesting(leaked_field_trial_list.get());
    leaked_field_trial_list.reset();
    FieldTrialList::RestoreInstanceForTesting(backup_field_trial);
  }
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

TEST_F(ScopedFeatureListTest, InitWithFeatureStates) {
  test::ScopedFeatureList feature_list1;
  feature_list1.InitWithFeatureStates(
      {{kTestFeature1, true}, {kTestFeature2, false}});
  ExpectFeatures(/*enabled_features=*/"TestFeature1",
                 /*disabled_features=*/"TestFeature2");

  {
    test::ScopedFeatureList feature_list2;
    feature_list2.InitWithFeatureStates(
        {{kTestFeature1, false}, {kTestFeature2, true}});
    ExpectFeatures(/*enabled_features=*/"TestFeature2",
                   /*disabled_features=*/"TestFeature1");
  }
}

TEST_F(ScopedFeatureListTest, FeatureParameterCache) {
  // Check the default parameter, and bring it on its local cache if the cache
  // is enabled.
  ASSERT_FALSE(kTestFeatureParam1.Get());

  // Ensure if the parameter override works if the cache feature is enabled
  // outside the ScopedFeatureList.
  test::ScopedFeatureList feature_list_to_override_cached_parameter;
  feature_list_to_override_cached_parameter.InitAndEnableFeatureWithParameters(
      kTestFeature1, {{kTestFeatureParam1.name, "true"}});
  ASSERT_TRUE(kTestFeatureParam1.Get());
}

}  // namespace base::test
