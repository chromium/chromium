// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_color_policy_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace policy {

class ThemeColorPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(base::Value value) {
    policies_.Set(key::kBrowserThemeColor, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(value),
                  nullptr);
  }

  bool CheckPolicy(base::Value value) {
    SetPolicy(std::move(value));
    return handler_.CheckPolicySettings(policies_, &errors_);
  }

  void ApplyPolicies() { handler_.ApplyPolicySettings(policies_, &prefs_); }

  void CheckValidPolicy(const std::string& policy_value,
                        SkColor expected_color) {
    ASSERT_FALSE(policy_value.empty());
    base::Value theme_color_value(std::move(policy_value));
    EXPECT_FALSE(theme_color_value.GetString().empty());
    EXPECT_TRUE(CheckPolicy(theme_color_value.Clone()));
    EXPECT_EQ(0U, errors_.size());

    ApplyPolicies();
    base::Value* value;
    EXPECT_TRUE(prefs_.GetValue(prefs::kPolicyThemeColor, &value));
    ASSERT_TRUE(value);
    EXPECT_EQ(value->GetInt(), static_cast<int>(expected_color));
  }

  void CheckInvalidPolicy(const base::Value& policy_value) {
    EXPECT_FALSE(CheckPolicy(policy_value.Clone()));
    EXPECT_NE(0U, errors_.size());
  }

  void CheckInvalidValuePolicy(std::string&& policy_value) {
    base::Value theme_color_value(std::move(policy_value));
    CheckInvalidPolicy(theme_color_value);
  }

  ThemeColorPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(ThemeColorPolicyHandlerTest, NoPolicy) {
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(ThemeColorPolicyHandlerTest, ValidValues) {
  CheckValidPolicy("#000000", SkColorSetRGB(0, 0, 0));
  CheckValidPolicy("000000", SkColorSetRGB(0, 0, 0));
  CheckValidPolicy("#999999", SkColorSetRGB(153, 153, 153));
  CheckValidPolicy("#FFFFFF", SkColorSetRGB(255, 255, 255));
  CheckValidPolicy("#ffffff", SkColorSetRGB(255, 255, 255));
  CheckValidPolicy("#0a1B2c", SkColorSetRGB(10, 27, 44));
}

TEST_F(ThemeColorPolicyHandlerTest, InvalidValues) {
  CheckInvalidValuePolicy("#00000");
  CheckInvalidValuePolicy("#000");
  CheckInvalidValuePolicy("#00000G");
  CheckInvalidValuePolicy("#zzzzzz");
  CheckInvalidValuePolicy("#00000!");
  CheckInvalidValuePolicy("#000 000");
  CheckInvalidValuePolicy("# 000000");
  CheckInvalidValuePolicy(" #000000");
  CheckInvalidValuePolicy("#000000 ");
  CheckInvalidValuePolicy("      #000000      ");
}

TEST_F(ThemeColorPolicyHandlerTest, InvalidTypes) {
  base::Value value_int(123);
  CheckInvalidPolicy(value_int);
  base::Value value_float(1.0f);
  CheckInvalidPolicy(value_float);
}

}  // namespace policy
