// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/watermark/watermark_style_policy_handler.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kPolicyName[] = "WatermarkStyle";

constexpr char kSchema[] = R"(
  {
    "type": "object",
    "properties": {
      "WatermarkStyle": {
        "type": "object",
        "properties": {
          "fill_opacity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 100
            },
          "outline_opacity": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100
          },
          "font_size": {
          "type": "integer",
          "minimum": 1
          }
        }
      }
    }
  })";

constexpr char kValidPolicy[] = R"(
  {
    "fill_opacity": 10,
    "outline_opacity": 20,
    "font_size": 30
  }
)";

constexpr char kInvalidFillOpacityTooLowPolicy[] = R"(
  {
    "fill_opacity": -1,
    "outline_opacity": 20,
    "font_size": 30
  }
)";

constexpr char kInvalidFillOpacityTooHighPolicy[] = R"(
  {
    "fill_opacity": 101,
    "outline_opacity": 20,
    "font_size": 30
  }
)";

constexpr char kInvalidOutlineOpacityTooLowPolicy[] = R"(
  {
    "fill_opacity": 10,
    "outline_opacity": -5,
    "font_size": 30
  }
)";

constexpr char kInvalidOutlineOpacityTooHighPolicy[] = R"(
  {
    "fill_opacity": 10,
    "outline_opacity": 102,
    "font_size": 30
  }
)";

constexpr char kInvalidFontSizeTooLowPolicy[] = R"(
  {
    "fill_opacity": 10,
    "outline_opacity": 20,
    "font_size": 0
  }
)";

constexpr char kInvalidFieldTypePolicy[] = R"(
  {
    "fill_opacity": "should_be_integer",
    "outline_opacity": 20,
    "font_size": 30
  }
)";

constexpr char kInvalidTypeStringPolicy[] = R"(
  "this should be a dictionary"
)";

constexpr char kInvalidTypeIntPolicy[] = R"(
  2025
)";

class WatermarkStylePolicyHandlerTest : public testing::Test {
 protected:
  policy::Schema schema() { return schema_; }

 private:
  void SetUp() override {
    ASSIGN_OR_RETURN(schema_, policy::Schema::Parse(kSchema),
                     [](const auto& e) { ADD_FAILURE() << e; });
  }

  policy::Schema schema_;
};

struct InvalidPolicyTestCase {
  std::string test_name;
  const char* policy_json;
};

TEST_F(WatermarkStylePolicyHandlerTest, TestValidPolicy) {
  policy::PolicyMap policy_map;
  policy_map.Set(
      kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
      policy::PolicyScope::POLICY_SCOPE_MACHINE,
      policy::PolicySource::POLICY_SOURCE_CLOUD,
      base::JSONReader::Read(kValidPolicy, base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);

  auto handler = std::make_unique<WatermarkStylePolicyHandler>(schema());

  policy::PolicyErrorMap errors;
  ASSERT_TRUE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_TRUE(errors.empty());

  PrefValueMap prefs;
  handler->ApplyPolicySettings(policy_map, &prefs);

  base::Value* value_in_pref;
  ASSERT_TRUE(prefs.GetValue(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, &value_in_pref));
  int fill_opacity = value_in_pref->GetInt();
  ASSERT_TRUE(
      prefs.GetValue(enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
                     &value_in_pref));
  int outline_opacity = value_in_pref->GetInt();
  ASSERT_TRUE(prefs.GetValue(enterprise_connectors::kWatermarkStyleFontSizePref,
                             &value_in_pref));
  int font_size = value_in_pref->GetInt();

  base::Value::Dict pref_dict;
  pref_dict.Set(enterprise_connectors::kWatermarkStyleFillOpacityFieldName,
                fill_opacity);
  pref_dict.Set(enterprise_connectors::kWatermarkStyleOutlineOpacityFieldName,
                outline_opacity);
  pref_dict.Set(enterprise_connectors::kWatermarkStyleFontSizeFieldName,
                font_size);

  const base::Value* value_in_map =
      policy_map.GetValue(kPolicyName, base::Value::Type::DICT);
  ASSERT_EQ(pref_dict, *value_in_map);
}

TEST_F(WatermarkStylePolicyHandlerTest,
       TestValidPolicyNotAppliedIfNotFromCloud) {
  policy::PolicyMap policy_map;
  policy_map.Set(
      kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
      policy::PolicyScope::POLICY_SCOPE_MACHINE,
      policy::PolicySource::POLICY_SOURCE_PLATFORM,
      base::JSONReader::Read(kValidPolicy, base::JSON_ALLOW_TRAILING_COMMAS),
      nullptr);
  auto handler = std::make_unique<WatermarkStylePolicyHandler>(schema());

  policy::PolicyErrorMap errors;
  ASSERT_FALSE(handler->CheckPolicySettings(policy_map, &errors));
  ASSERT_FALSE(errors.empty());
}

class WatermarkStyleInvalidPolicyHandlerTest
    : public WatermarkStylePolicyHandlerTest,
      public testing::WithParamInterface<InvalidPolicyTestCase> {};

TEST_P(WatermarkStyleInvalidPolicyHandlerTest, TestInvalidPolicies) {
  const InvalidPolicyTestCase& test_case = GetParam();
  policy::PolicyMap policy_map;
  policy_map.Set(kPolicyName, policy::PolicyLevel::POLICY_LEVEL_MANDATORY,
                 policy::PolicyScope::POLICY_SCOPE_MACHINE,
                 policy::PolicySource::POLICY_SOURCE_CLOUD,
                 base::JSONReader::Read(test_case.policy_json,
                                        base::JSON_ALLOW_TRAILING_COMMAS),
                 nullptr);

  auto handler = std::make_unique<WatermarkStylePolicyHandler>(schema());

  policy::PolicyErrorMap errors;
  bool check_policy_settings_result =
      handler->CheckPolicySettings(policy_map, &errors);
  ASSERT_FALSE(check_policy_settings_result);
  ASSERT_FALSE(errors.empty());
}

INSTANTIATE_TEST_SUITE_P(
    InvalidWatermarkPolicies,
    WatermarkStyleInvalidPolicyHandlerTest,
    testing::ValuesIn<InvalidPolicyTestCase>(
        {{"InvalidFillOpacityTooLow", kInvalidFillOpacityTooLowPolicy},
         {"InvalidFillOpacityTooHigh", kInvalidFillOpacityTooHighPolicy},
         {"InvalidOutlineOpacityTooLow", kInvalidOutlineOpacityTooLowPolicy},
         {"InvalidOutlineOpacityTooHigh", kInvalidOutlineOpacityTooHighPolicy},
         {"InvalidFontSizeTooLow", kInvalidFontSizeTooLowPolicy},
         {"InvalidFieldType", kInvalidFieldTypePolicy},
         {"InvalidTypeString", kInvalidTypeStringPolicy},
         {"InvalidTypeInt", kInvalidTypeIntPolicy}}),
    [](const testing::TestParamInfo<
        WatermarkStyleInvalidPolicyHandlerTest::ParamType>& info) {
      return info.param.test_name;
    });
