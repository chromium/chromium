// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/default_geolocation_policy_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DefaultGeolocationPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest,
      public testing::WithParamInterface<ContentSetting> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(ash::features::kCrosPrivacyHub);
    handler_list_.AddHandler(
        base::WrapUnique(new DefaultGeolocationPolicyHandler));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class DefaultGeolocationPolicyHandlerTestWithPHEnabled
    : public DefaultGeolocationPolicyHandlerTest {
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ash::features::kCrosPrivacyHub);
    handler_list_.AddHandler(
        base::WrapUnique(new DefaultGeolocationPolicyHandler));
  }
};

TEST_P(DefaultGeolocationPolicyHandlerTest, All) {
  // DefaultGeolocationSetting of CONTENT_SETTING_ASK (AskGeolocation) should
  // not translate to the ArcLocationServiceEnabled preference.
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  PolicyMap policy;
  policy.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(GetParam()),
             nullptr);
  UpdateProviderPolicy(policy);

  // DefaultGeolocationSetting should only affect `ArcLocationServiceEnabled`
  // preference when it is set to CONTENT_SETTING_BLOCK.
  if (GetParam() == CONTENT_SETTING_BLOCK) {
    const base::Value* value = nullptr;
    EXPECT_TRUE(
        store_->GetValue(arc::prefs::kArcLocationServiceEnabled, &value));
    EXPECT_EQ(base::Value(false), *value);
  } else {
    EXPECT_FALSE(
        store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  }
}

TEST_P(DefaultGeolocationPolicyHandlerTestWithPHEnabled, All) {
  // DefaultGeolocationSetting policy should NO LONGER affect the ARC++ location
  // setting. For more details, see GoogleLocationServicesEnabled policy.
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
  PolicyMap policy;
  policy.Set(key::kDefaultGeolocationSetting, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(GetParam()),
             nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(
      store_->GetValue(arc::prefs::kArcLocationServiceEnabled, nullptr));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DefaultGeolocationPolicyHandlerTest,
                         testing::Values(CONTENT_SETTING_BLOCK,
                                         CONTENT_SETTING_ASK,
                                         CONTENT_SETTING_ALLOW));

INSTANTIATE_TEST_SUITE_P(All,
                         DefaultGeolocationPolicyHandlerTestWithPHEnabled,
                         testing::Values(CONTENT_SETTING_BLOCK,
                                         CONTENT_SETTING_ASK,
                                         CONTENT_SETTING_ALLOW));

}  // namespace policy
