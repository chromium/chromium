// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_geolocation_policy_handler.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DefaultGeolocationPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
  void SetUp() override {
    handler_list_.AddHandler(base::WrapUnique<ConfigurationPolicyHandler>(
        new DefaultGeolocationPolicyHandler));
  }
};

TEST_F(DefaultGeolocationPolicyHandlerTest, AllowGeolocation) {
  // DefaultGeolocationSetting of CONTENT_SETTING_ALLOW (AllowGeolocation)
  // should not translate to the ArcLocationServiceEnabled preference.
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  PolicyMap policy;
  policy.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(CONTENT_SETTING_ALLOW), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
}

TEST_F(DefaultGeolocationPolicyHandlerTest, BlockGeolocation) {
  // DefaultGeolocationSetting of CONTENT_SETTING_BLOCK (BlockGeolocation)
  // should set the ArcLocationServiceEnabled preference to false.
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  PolicyMap policy;
  policy.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(CONTENT_SETTING_BLOCK), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  EXPECT_TRUE(store_->GetValue(arc::prefs::kArcLocationServiceEnabled, &value));
  EXPECT_EQ(base::Value(false), *value);
}

TEST_F(DefaultGeolocationPolicyHandlerTest, AskGeolocation) {
  // DefaultGeolocationSetting of CONTENT_SETTING_ASK (AskGeolocation) should
  // not translate to the ArcLocationServiceEnabled preference.
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  PolicyMap policy;
  policy.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(CONTENT_SETTING_ASK), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
}

}  // namespace policy
