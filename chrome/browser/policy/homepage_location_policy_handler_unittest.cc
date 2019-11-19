// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/homepage_location_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kHttpUrl[] = "http://example.com/";
constexpr char kHttpsUrl[] = "https://example.com/";
constexpr char kFileUrl[] = "file:///wohnzimmertapete.html";
constexpr char kUrlWithNoScheme[] = "example.com";
constexpr char kFixedUrlWithNoScheme[] = "http://example.com/";
constexpr char kInvalidSchemeUrl[] = "xss://crazy_hack.js";
constexpr char kJavascript[] =
    "(function()%7Bvar%20script%20%3D%20document.createElement(%22script%22)%"
    "3Bscript.type%3D%22text%2Fjavascript%22%3Bscript.src%3D%22https%3A%2F%"
    "2Fwww.example.com%22%3Bdocument.head.appendChild(script)%3B%7D())%3B";

}  // namespace

class HomepageLocationPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicy(std::unique_ptr<base::Value> value) {
    policies_.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, std::move(value),
                  nullptr);
  }

  bool CheckPolicy(std::unique_ptr<base::Value> value) {
    SetPolicy(std::move(value));
    return handler_.CheckPolicySettings(policies_, &errors_);
  }

  void ApplyPolicies() { handler_.ApplyPolicySettings(policies_, &prefs_); }

  HomepageLocationPolicyHandler handler_;
  PolicyErrorMap errors_;
  PolicyMap policies_;
  PrefValueMap prefs_;
};

TEST_F(HomepageLocationPolicyHandlerTest, NoPolicyDoesntExplode) {
  EXPECT_TRUE(handler_.CheckPolicySettings(policies_, &errors_));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(HomepageLocationPolicyHandlerTest, StandardSchemesAreAccepted) {
  EXPECT_TRUE(CheckPolicy(std::make_unique<base::Value>(kHttpUrl)));
  EXPECT_TRUE(CheckPolicy(std::make_unique<base::Value>(kHttpsUrl)));
  EXPECT_TRUE(CheckPolicy(std::make_unique<base::Value>(kFileUrl)));
  EXPECT_EQ(0U, errors_.size());
}

TEST_F(HomepageLocationPolicyHandlerTest, kUrlWithMissingSchemeIsFixed) {
  EXPECT_TRUE(CheckPolicy(std::make_unique<base::Value>(kUrlWithNoScheme)));
  EXPECT_EQ(0U, errors_.size());

  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHomePage, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), kFixedUrlWithNoScheme);
}

TEST_F(HomepageLocationPolicyHandlerTest, InvalidSchemeIsRejected) {
  EXPECT_FALSE(CheckPolicy(std::make_unique<base::Value>(kInvalidSchemeUrl)));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(HomepageLocationPolicyHandlerTest, JavascriptIsRejected) {
  EXPECT_FALSE(CheckPolicy(std::make_unique<base::Value>(kJavascript)));
  EXPECT_EQ(1U, errors_.size());
}

TEST_F(HomepageLocationPolicyHandlerTest,
       ApplyPolicySettings_SomethingSpecified) {
  SetPolicy(std::make_unique<base::Value>(kHttpUrl));
  ApplyPolicies();
  base::Value* value;
  EXPECT_TRUE(prefs_.GetValue(prefs::kHomePage, &value));
  ASSERT_TRUE(value->is_string());
  EXPECT_EQ(value->GetString(), kHttpUrl);
}

TEST_F(HomepageLocationPolicyHandlerTest,
       ApplyPolicySettings_NothingSpecified) {
  ApplyPolicies();
  EXPECT_FALSE(prefs_.GetValue(prefs::kHomePage, nullptr));
}

}  // namespace policy
