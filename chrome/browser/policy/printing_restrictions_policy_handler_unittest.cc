// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/printing_restrictions_policy_handler.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PrintingRestrictionsPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(base::Value value) {
    policies_.Set(key::kPrintingPaperSizeDefault, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(value),
                  nullptr);
  }

  bool CheckPolicy(base::Value value) {
    SetPolicy(std::move(value));
    return handler_.CheckPolicySettings(policies_, &errors_);
  }

  void ApplyPolicies() { handler_.ApplyPolicySettings(policies_, &prefs_); }

  void CheckValidPolicy(const std::string& policy_value) {
    absl::optional<base::Value> printing_paper_size_default =
        base::JSONReader::Read(policy_value);
    ASSERT_TRUE(printing_paper_size_default.has_value());
    EXPECT_TRUE(CheckPolicy(printing_paper_size_default.value().Clone()));
    EXPECT_EQ(0U, errors_.size());

    ApplyPolicies();
    base::Value* value;
    EXPECT_TRUE(prefs_.GetValue(prefs::kPrintingPaperSizeDefault, &value));
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, printing_paper_size_default.value());
  }

  void CheckInvalidPolicy(const std::string& policy_value) {
    absl::optional<base::Value> printing_paper_size_default =
        base::JSONReader::Read(policy_value);
    ASSERT_TRUE(printing_paper_size_default.has_value());
    EXPECT_FALSE(CheckPolicy(printing_paper_size_default.value().Clone()));
    EXPECT_EQ(1U, errors_.size());
  }

  PrintingPaperSizeDefaultPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(PrintingRestrictionsPolicyHandlerTest, NoPolicy) {
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, StandardPaperSize) {
  const char kStandardPaperSize[] = R"(
    {
      "name": "iso_a5_148x210mm"
    })";
  CheckValidPolicy(kStandardPaperSize);
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, CustomPaperSize) {
  const char kCustomPaperSize[] = R"(
    {
      "name": "custom",
      "custom_size": {
        "width": 100000,
        "height": 100000
      }
    })";
  CheckValidPolicy(kCustomPaperSize);
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, BothStandardAndCustomPaperSize) {
  const char kBothStandardAndCustomPaperSize[] = R"(
    {
      "name": "iso_a5_148x210mm",
      "custom_size": {
        "width": 100000,
        "height": 100000
      }
    })";
  CheckInvalidPolicy(kBothStandardAndCustomPaperSize);
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, NoCustomPaperSize) {
  const char kNoCustomPaperSize[] = R"(
    {
      "name": "custom"
    })";
  CheckInvalidPolicy(kNoCustomPaperSize);
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, NoWidthInCustomSize) {
  const char kNoWidthInCustomSize[] = R"(
    {
      "name": "custom",
      "custom_size": {
        "height": 100000
      }
    })";
  CheckInvalidPolicy(kNoWidthInCustomSize);
}

TEST_F(PrintingRestrictionsPolicyHandlerTest, NoHeightInCustomSize) {
  const char kNoHeightInCustomSize[] = R"(
    {
      "name": "custom",
      "custom_size": {
        "width": 100000
      }
    })";
  CheckInvalidPolicy(kNoHeightInCustomSize);
}

}  // namespace policy
