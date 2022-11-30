// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_handler.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeveloperToolsPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  DeveloperToolsPolicyHandlerTest() {
    handler_list_.AddHandler(std::make_unique<DeveloperToolsPolicyHandler>());
  }
};

TEST_F(DeveloperToolsPolicyHandlerTest, NewPolicyOverridesLegacyPolicy) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(0 /*DeveloperToolsDisallowedForForceInstalledExtensions*/),
      nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(DeveloperToolsPolicyHandler::Availability::
                                 kDisallowedForForceInstalledExtensions),
            value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, LegacyPolicyAppliesIfNewPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(5 /*out of range*/), nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
             nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(DeveloperToolsPolicyHandler::Availability::kDisallowed),
      value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Developer mode on extensions UI is also disabled.
  const base::Value* extensions_ui_dev_mode_value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kExtensionsUIDeveloperMode,
                               &extensions_ui_dev_mode_value));
  EXPECT_FALSE(extensions_ui_dev_mode_value->GetBool());
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, NewPolicyAppliesIfLegacyPolicyInvalid) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(1 /*kAllowed*/), nullptr);
  policy.Set(key::kDeveloperToolsDisabled, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(4 /*wrong type*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(DeveloperToolsPolicyHandler::Availability::kAllowed),
      value->GetInt());
}

TEST_F(DeveloperToolsPolicyHandlerTest, DisallowedForForceInstalledExtensions) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(
      key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(0 /*DeveloperToolsDisallowedForForceInstalledExtensions*/),
      nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(static_cast<int>(DeveloperToolsPolicyHandler::Availability::
                                 kDisallowedForForceInstalledExtensions),
            value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, Allowed) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(1 /*DeveloperToolsAllowed*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(DeveloperToolsPolicyHandler::Availability::kAllowed),
      value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // No force-disabling of developer mode on extensions UI.
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, Disallowed) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(2 /*Disallowed*/), nullptr);
  UpdateProviderPolicy(policy);
  const base::Value* value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kDevToolsAvailability, &value));
  EXPECT_EQ(
      static_cast<int>(DeveloperToolsPolicyHandler::Availability::kDisallowed),
      value->GetInt());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Developer mode on extensions UI is also disabled.
  const base::Value* extensions_ui_dev_mode_value = nullptr;
  ASSERT_TRUE(store_->GetValue(prefs::kExtensionsUIDeveloperMode,
                               &extensions_ui_dev_mode_value));
  EXPECT_FALSE(extensions_ui_dev_mode_value->GetBool());
#endif
}

TEST_F(DeveloperToolsPolicyHandlerTest, InvalidValue) {
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

  PolicyMap policy;
  policy.Set(key::kDeveloperToolsAvailability, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(5 /*out of range*/), nullptr);
  UpdateProviderPolicy(policy);
  EXPECT_FALSE(store_->GetValue(prefs::kDevToolsAvailability, nullptr));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(store_->GetValue(prefs::kExtensionsUIDeveloperMode, nullptr));
#endif
}

// Tests the |GetMostRestrictiveAvailability| static function.
TEST_F(DeveloperToolsPolicyHandlerTest, MostRestrictiveAvailability) {
  using Availability = DeveloperToolsPolicyHandler::Availability;

  // kAllowed.
  EXPECT_EQ(Availability::kAllowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kAllowed, Availability::kAllowed));

  // kAllowed and kDisallowed.
  EXPECT_EQ(Availability::kDisallowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kDisallowed, Availability::kAllowed));
  EXPECT_EQ(Availability::kDisallowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kAllowed, Availability::kDisallowed));

  // kAllowed and kDisallowedForForceInstalledExtensions.
  EXPECT_EQ(Availability::kDisallowedForForceInstalledExtensions,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kAllowed,
                Availability::kDisallowedForForceInstalledExtensions));
  EXPECT_EQ(Availability::kDisallowedForForceInstalledExtensions,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kDisallowedForForceInstalledExtensions,
                Availability::kAllowed));

  // kDisallowedForForceInstalledExtensions and kDisallowed.
  EXPECT_EQ(Availability::kDisallowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kDisallowed,
                Availability::kDisallowedForForceInstalledExtensions));
  EXPECT_EQ(Availability::kDisallowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kDisallowedForForceInstalledExtensions,
                Availability::kDisallowed));

  // kDisallowed.
  EXPECT_EQ(Availability::kDisallowed,
            DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
                Availability::kDisallowed, Availability::kDisallowed));
}

}  // namespace policy
