// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/javascript_policy_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

class JavascriptPolicyHandlerTest : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new JavascriptPolicyHandler));
  }
};

TEST_F(JavascriptPolicyHandlerTest, JavascriptEnabled) {
  // This is a boolean policy, but affects an integer preference.
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, nullptr));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, nullptr));
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), *value);
}

TEST_F(JavascriptPolicyHandlerTest, JavascriptEnabledOverridden) {
  EXPECT_FALSE(
      store_->GetValue(prefs::kManagedDefaultJavaScriptSetting, nullptr));
  PolicyMap policy;
  policy.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), *value);
  // DefaultJavaScriptSetting overrides JavascriptEnabled.
  policy.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(CONTENT_SETTING_ALLOW), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_TRUE(store_->GetValue(prefs::kManagedDefaultJavaScriptSetting,
                               &value));
  EXPECT_EQ(base::Value(CONTENT_SETTING_ALLOW), *value);
}

}  // namespace policy
