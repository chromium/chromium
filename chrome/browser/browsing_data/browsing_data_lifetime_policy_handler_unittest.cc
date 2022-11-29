// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_lifetime_policy_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

TEST(BrowsingDataLifetimePolicyHandler, SyncDisabledNotSet) {
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;

  policy_map.Set(policy::key::kBrowsingDataLifetime,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(base::Value::Type::LIST), nullptr);

  BrowsingDataLifetimePolicyHandler handler(
      policy::key::kBrowsingDataLifetime,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  handler.CheckPolicySettings(policy_map, &errors);
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kBrowsingDataLifetime),
            l10n_util::GetStringFUTF16(
                IDS_POLICY_DEPENDENCY_ERROR,
                base::UTF8ToUTF16(policy::key::kSyncDisabled), u"true"));
}

TEST(BrowsingDataLifetimePolicyHandler, SyncDisabledFalse) {
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;

  policy_map.Set(policy::key::kBrowsingDataLifetime,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(base::Value::Type::LIST), nullptr);
  policy_map.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(false), nullptr);

  BrowsingDataLifetimePolicyHandler handler(
      policy::key::kBrowsingDataLifetime,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  handler.CheckPolicySettings(policy_map, &errors);
  EXPECT_EQ(errors.GetErrorMessages(policy::key::kBrowsingDataLifetime),
            l10n_util::GetStringFUTF16(
                IDS_POLICY_DEPENDENCY_ERROR,
                base::UTF8ToUTF16(policy::key::kSyncDisabled), u"true"));
}

TEST(BrowsingDataLifetimePolicyHandler, SyncDisabledTrue) {
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;

  policy_map.Set(policy::key::kBrowsingDataLifetime,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(base::Value::Type::LIST), nullptr);
  policy_map.Set(policy::key::kSyncDisabled, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(true), nullptr);

  BrowsingDataLifetimePolicyHandler handler(
      policy::key::kBrowsingDataLifetime,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  handler.CheckPolicySettings(policy_map, &errors);
  EXPECT_TRUE(errors.empty());
}
