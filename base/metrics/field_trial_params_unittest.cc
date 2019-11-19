// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_params.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Call FieldTrialList::FactoryGetFieldTrial().
scoped_refptr<FieldTrial> CreateFieldTrial(
    const std::string& trial_name,
    int total_probability,
    const std::string& default_group_name,
    int* default_group_number) {
  return FieldTrialList::FactoryGetFieldTrial(
      trial_name, total_probability, default_group_name,
      FieldTrial::SESSION_RANDOMIZED, default_group_number);
}

}  // namespace

class FieldTrialParamsTest : public ::testing::Test {
 public:
  FieldTrialParamsTest() = default;

  ~FieldTrialParamsTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  void CreateFeatureWithTrial(const Feature& feature,
                              FeatureList::OverrideState override_state,
                              FieldTrial* trial) {
    std::unique_ptr<FeatureList> feature_list(new FeatureList);
    feature_list->RegisterFieldTrialOverride(feature.name, override_state,
                                             trial);
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

 private:
  test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialParamsTest);
};

TEST_F(FieldTrialParamsTest, AssociateFieldTrialParams) {
  const std::string kTrialName = "AssociateFieldTrialParams";

  {
    std::map<std::string, std::string> params;
    params["a"] = "10";
    params["b"] = "test";
    ASSERT_TRUE(AssociateFieldTrialParams(kTrialName, "A", params));
  }
  {
    std::map<std::string, std::string> params;
    params["a"] = "5";
    ASSERT_TRUE(AssociateFieldTrialParams(kTrialName, "B", params));
  }

  FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("5", GetFieldTrialParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(GetFieldTrialParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("5", params["a"]);
}

TEST_F(FieldTrialParamsTest, AssociateFieldTrialParams_Fail) {
  const std::string kTrialName = "AssociateFieldTrialParams_Fail";
  const std::string kGroupName = "A";

  std::map<std::string, std::string> params;
  params["a"] = "10";
  ASSERT_TRUE(AssociateFieldTrialParams(kTrialName, kGroupName, params));
  params["a"] = "1";
  params["b"] = "2";
  ASSERT_FALSE(AssociateFieldTrialParams(kTrialName, kGroupName, params));

  FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  EXPECT_EQ("10", GetFieldTrialParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "b"));
}

TEST_F(FieldTrialParamsTest, AssociateFieldTrialParams_TrialActiveFail) {
  const std::string kTrialName = "AssociateFieldTrialParams_TrialActiveFail";
  FieldTrialList::CreateFieldTrial(kTrialName, "A");
  ASSERT_EQ("A", FieldTrialList::FindFullName(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_FALSE(AssociateFieldTrialParams(kTrialName, "B", params));
  EXPECT_FALSE(AssociateFieldTrialParams(kTrialName, "A", params));
}

TEST_F(FieldTrialParamsTest, AssociateFieldTrialParams_DoesntActivateTrial) {
  const std::string kTrialName =
      "AssociateFieldTrialParams_DoesntActivateTrial";

  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));
  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  params["a"] = "10";
  EXPECT_TRUE(AssociateFieldTrialParams(kTrialName, "A", params));
  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParams_NoTrial) {
  const std::string kTrialName = "GetFieldTrialParams_NoParams";

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetFieldTrialParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "y"));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParams_NoParams) {
  const std::string kTrialName = "GetFieldTrialParams_NoParams";

  FieldTrialList::CreateFieldTrial(kTrialName, "A");

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetFieldTrialParams(kTrialName, &params));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "x"));
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "y"));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParams_ActivatesTrial) {
  const std::string kTrialName = "GetFieldTrialParams_ActivatesTrial";

  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));
  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_FALSE(GetFieldTrialParams(kTrialName, &params));
  ASSERT_TRUE(FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParamValue_ActivatesTrial) {
  const std::string kTrialName = "GetFieldTrialParamValue_ActivatesTrial";

  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));
  ASSERT_FALSE(FieldTrialList::IsTrialActive(kTrialName));

  std::map<std::string, std::string> params;
  EXPECT_EQ(std::string(), GetFieldTrialParamValue(kTrialName, "x"));
  ASSERT_TRUE(FieldTrialList::IsTrialActive(kTrialName));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParamsByFeature) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";
  const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_TRUE(GetFieldTrialParamsByFeature(kFeature, &actualParams));
  EXPECT_EQ(params, actualParams);
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParamValueByFeature) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";
  const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_EQ(params["x"], GetFieldTrialParamValueByFeature(kFeature, "x"));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParamsByFeature_Disable) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";
  const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_DISABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_FALSE(GetFieldTrialParamsByFeature(kFeature, &actualParams));
}

TEST_F(FieldTrialParamsTest, GetFieldTrialParamValueByFeature_Disable) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";
  const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};

  std::map<std::string, std::string> params;
  params["x"] = "1";
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_DISABLE_FEATURE,
                         trial.get());

  std::map<std::string, std::string> actualParams;
  EXPECT_EQ(std::string(), GetFieldTrialParamValueByFeature(kFeature, "x"));
}

TEST_F(FieldTrialParamsTest, FeatureParamString) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<std::string> a{&kFeature, "a", "default"};
  static const FeatureParam<std::string> b{&kFeature, "b", ""};
  static const FeatureParam<std::string> c{&kFeature, "c", "default"};
  static const FeatureParam<std::string> d{&kFeature, "d", ""};
  static const FeatureParam<std::string> e{&kFeature, "e", "default"};
  static const FeatureParam<std::string> f{&kFeature, "f", ""};

  std::map<std::string, std::string> params;
  params["a"] = "";
  params["b"] = "non-default";
  params["c"] = "non-default";
  params["d"] = "";
  // "e" is not registered
  // "f" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_EQ("default", a.Get());  // empty
  EXPECT_EQ("non-default", b.Get());
  EXPECT_EQ("non-default", c.Get());
  EXPECT_EQ("", d.Get());         // empty
  EXPECT_EQ("default", e.Get());  // not registered
  EXPECT_EQ("", f.Get());         // not registered
}

TEST_F(FieldTrialParamsTest, FeatureParamInt) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<int> a{&kFeature, "a", 0};
  static const FeatureParam<int> b{&kFeature, "b", 0};
  static const FeatureParam<int> c{&kFeature, "c", 0};
  static const FeatureParam<int> d{&kFeature, "d", 0};
  static const FeatureParam<int> e{&kFeature, "e", 0};

  std::map<std::string, std::string> params;
  params["a"] = "1";
  params["b"] = "1.5";
  params["c"] = "foo";
  params["d"] = "";
  // "e" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_EQ(1, GetFieldTrialParamByFeatureAsInt(kFeature, "a", 0));
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsInt(kFeature, "b", 0));  // invalid
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsInt(kFeature, "c", 0));  // invalid
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsInt(kFeature, "d", 0));  // empty
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsInt(kFeature, "e", 0));  // empty

  EXPECT_EQ(1, a.Get());
  EXPECT_EQ(0, b.Get());  // invalid
  EXPECT_EQ(0, c.Get());  // invalid
  EXPECT_EQ(0, d.Get());  // empty
  EXPECT_EQ(0, e.Get());  // empty
}

TEST_F(FieldTrialParamsTest, FeatureParamDouble) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<double> a{&kFeature, "a", 0.0};
  static const FeatureParam<double> b{&kFeature, "b", 0.0};
  static const FeatureParam<double> c{&kFeature, "c", 0.0};
  static const FeatureParam<double> d{&kFeature, "d", 0.0};
  static const FeatureParam<double> e{&kFeature, "e", 0.0};
  static const FeatureParam<double> f{&kFeature, "f", 0.0};

  std::map<std::string, std::string> params;
  params["a"] = "1";
  params["b"] = "1.5";
  params["c"] = "1.0e-10";
  params["d"] = "foo";
  params["e"] = "";
  // "f" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_EQ(1, GetFieldTrialParamByFeatureAsDouble(kFeature, "a", 0));
  EXPECT_EQ(1.5, GetFieldTrialParamByFeatureAsDouble(kFeature, "b", 0));
  EXPECT_EQ(1.0e-10, GetFieldTrialParamByFeatureAsDouble(kFeature, "c", 0));
  EXPECT_EQ(0,
            GetFieldTrialParamByFeatureAsDouble(kFeature, "d", 0));  // invalid
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsDouble(kFeature, "e", 0));  // empty
  EXPECT_EQ(0, GetFieldTrialParamByFeatureAsDouble(kFeature, "f", 0));  // empty

  EXPECT_EQ(1, a.Get());
  EXPECT_EQ(1.5, b.Get());
  EXPECT_EQ(1.0e-10, c.Get());
  EXPECT_EQ(0, d.Get());  // invalid
  EXPECT_EQ(0, e.Get());  // empty
  EXPECT_EQ(0, f.Get());  // empty
}

TEST_F(FieldTrialParamsTest, FeatureParamBool) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<bool> a{&kFeature, "a", false};
  static const FeatureParam<bool> b{&kFeature, "b", true};
  static const FeatureParam<bool> c{&kFeature, "c", false};
  static const FeatureParam<bool> d{&kFeature, "d", true};
  static const FeatureParam<bool> e{&kFeature, "e", true};
  static const FeatureParam<bool> f{&kFeature, "f", true};

  std::map<std::string, std::string> params;
  params["a"] = "true";
  params["b"] = "false";
  params["c"] = "1";
  params["d"] = "False";
  params["e"] = "";
  // "f" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_TRUE(a.Get());
  EXPECT_FALSE(b.Get());
  EXPECT_FALSE(c.Get());  // invalid
  EXPECT_TRUE(d.Get());   // invalid
  EXPECT_TRUE(e.Get());   // empty
  EXPECT_TRUE(f.Get());   // empty
}

enum Hand { ROCK, PAPER, SCISSORS };

TEST_F(FieldTrialParamsTest, FeatureParamEnum) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const FeatureParam<Hand>::Option hands[] = {
      {ROCK, "rock"}, {PAPER, "paper"}, {SCISSORS, "scissors"}};
  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<Hand> a{&kFeature, "a", ROCK, &hands};
  static const FeatureParam<Hand> b{&kFeature, "b", ROCK, &hands};
  static const FeatureParam<Hand> c{&kFeature, "c", ROCK, &hands};
  static const FeatureParam<Hand> d{&kFeature, "d", ROCK, &hands};
  static const FeatureParam<Hand> e{&kFeature, "e", PAPER, &hands};
  static const FeatureParam<Hand> f{&kFeature, "f", SCISSORS, &hands};

  std::map<std::string, std::string> params;
  params["a"] = "rock";
  params["b"] = "paper";
  params["c"] = "scissors";
  params["d"] = "lizard";
  params["e"] = "";
  // "f" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_EQ(ROCK, a.Get());
  EXPECT_EQ(PAPER, b.Get());
  EXPECT_EQ(SCISSORS, c.Get());
  EXPECT_EQ(ROCK, d.Get());      // invalid
  EXPECT_EQ(PAPER, e.Get());     // invalid/empty
  EXPECT_EQ(SCISSORS, f.Get());  // not registered
}

enum class UI { ONE_D, TWO_D, THREE_D };

TEST_F(FieldTrialParamsTest, FeatureParamEnumClass) {
  const std::string kTrialName = "GetFieldTrialParamsByFeature";

  static const FeatureParam<UI>::Option uis[] = {
      {UI::ONE_D, "1d"}, {UI::TWO_D, "2d"}, {UI::THREE_D, "3d"}};
  static const Feature kFeature{"TestFeature", FEATURE_DISABLED_BY_DEFAULT};
  static const FeatureParam<UI> a{&kFeature, "a", UI::ONE_D, &uis};
  static const FeatureParam<UI> b{&kFeature, "b", UI::ONE_D, &uis};
  static const FeatureParam<UI> c{&kFeature, "c", UI::ONE_D, &uis};
  static const FeatureParam<UI> d{&kFeature, "d", UI::ONE_D, &uis};
  static const FeatureParam<UI> e{&kFeature, "e", UI::TWO_D, &uis};
  static const FeatureParam<UI> f{&kFeature, "f", UI::THREE_D, &uis};

  std::map<std::string, std::string> params;
  params["a"] = "1d";
  params["b"] = "2d";
  params["c"] = "3d";
  params["d"] = "4d";
  params["e"] = "";
  // "f" is not registered
  AssociateFieldTrialParams(kTrialName, "A", params);
  scoped_refptr<FieldTrial> trial(
      CreateFieldTrial(kTrialName, 100, "A", nullptr));

  CreateFeatureWithTrial(kFeature, FeatureList::OVERRIDE_ENABLE_FEATURE,
                         trial.get());

  EXPECT_EQ(UI::ONE_D, a.Get());
  EXPECT_EQ(UI::TWO_D, b.Get());
  EXPECT_EQ(UI::THREE_D, c.Get());
  EXPECT_EQ(UI::ONE_D, d.Get());    // invalid
  EXPECT_EQ(UI::TWO_D, e.Get());    // invalid/empty
  EXPECT_EQ(UI::THREE_D, f.Get());  // not registered
}

}  // namespace base
