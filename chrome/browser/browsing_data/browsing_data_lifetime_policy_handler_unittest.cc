// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_lifetime_policy_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/browsing_data/core/features.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_policy_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// When the kDataRetentionPoliciesDisableSyncTypesNeeded feature is not set, the
// data retention policies will fail if SyncDisabled is not to set, or if
// SyncDisabled is set to false. There will be an error, but no other messages
// in the entry.
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
  handler.PrepareForDisplaying(&policy_map);
  EXPECT_FALSE(policy_map.Get(policy::key::kBrowsingDataLifetime)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
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
  handler.PrepareForDisplaying(&policy_map);
  EXPECT_FALSE(policy_map.Get(policy::key::kBrowsingDataLifetime)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
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
  handler.PrepareForDisplaying(&policy_map);
  EXPECT_FALSE(policy_map.Get(policy::key::kBrowsingDataLifetime)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
}

// When browser sign in is disabled by policy, the data deletion policy should
// be applied and the error map and messages should be empty
#if !BUILDFLAG(IS_CHROMEOS)
TEST(BrowsingDataLifetimePolicyHandler, BrowserSigninDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      browsing_data::features::kDataRetentionPoliciesDisableSyncTypesNeeded,
      true);
  policy::PolicyMap policy_map;
  policy::PolicyErrorMap errors;

  policy_map.Set(policy::key::kBrowsingDataLifetime,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(base::Value::Type::LIST), nullptr);
  policy_map.Set(policy::key::kBrowserSignin, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(0), nullptr);

  BrowsingDataLifetimePolicyHandler handler(
      policy::key::kBrowsingDataLifetime,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  handler.CheckPolicySettings(policy_map, &errors);
  EXPECT_TRUE(errors.empty());
  handler.PrepareForDisplaying(&policy_map);
  EXPECT_FALSE(policy_map.Get(policy::key::kBrowsingDataLifetime)
                   ->HasMessage(policy::PolicyMap::MessageType::kInfo));
}
#endif

// Check that the policies work correctly when set together with sync.
TEST(BrowsingDataLifetimePolicyHandler,
     SyncTypesListDisabledWithDataRetentionPolicies) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatureState(
      browsing_data::features::kDataRetentionPoliciesDisableSyncTypesNeeded,
      true);

  // Start with prefs enabled so we can sense that they have changed.
  PrefValueMap prefs;
  prefs.SetBoolean(syncer::prefs::kSyncAutofill, true);
  prefs.SetBoolean(syncer::prefs::kSyncPreferences, true);
  prefs.SetBoolean(syncer::prefs::kSyncTypedUrls, true);
  prefs.SetBoolean(syncer::prefs::kSyncTabs, true);
  prefs.SetBoolean(syncer::prefs::kSyncSavedTabGroups, true);
  prefs.SetBoolean(syncer::prefs::kSyncPasswords, true);
  prefs.SetBoolean(syncer::prefs::kSyncBookmarks, true);

  // Set sync types to bookmarks and create handler.
  policy::PolicyMap policy;
  base::Value::List sync_types_disabled_by_policy;
  sync_types_disabled_by_policy.Append("bookmarks");
  policy.Set(policy::key::kSyncTypesListDisabled,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(sync_types_disabled_by_policy)), nullptr);
  syncer::SyncPolicyHandler sync_handler;

  // Set BrowsinggDataOnExitList for some types.
  base::Value::Dict browsing_data_types_first_dict =
      base::Value::Dict()
          .Set("data_types", base::Value::List()
                                 .Append("browsing_history")
                                 .Append("site_settings")
                                 .Append("cached_images_and_files")
                                 .Append("cookies_and_other_site_data"))
          .Set("time_to_live_in_hours", 1);

  base::Value browsing_data_lifetime_value = base::Value(
      base::Value::List().Append(std::move(browsing_data_types_first_dict)));

  policy.Set(policy::key::kBrowsingDataLifetime, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(browsing_data_lifetime_value)), nullptr);

  BrowsingDataLifetimePolicyHandler browsing_data_lieftime_handler(
      policy::key::kBrowsingDataLifetime,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  // Apply sync policy and BrowsingDataLifetime then check the prefs.
  policy::PolicyErrorMap errors;
  sync_handler.ApplyPolicySettings(policy, &prefs);
  browsing_data_lieftime_handler.CheckPolicySettings(policy, &errors);
  EXPECT_TRUE(errors.empty());

  // Check that an info message is added for the BrowsingDataLifetimeHandler.
  browsing_data_lieftime_handler.ApplyPolicySettings(policy, &prefs);
  browsing_data_lieftime_handler.PrepareForDisplaying(&policy);
  EXPECT_TRUE(policy.Get(policy::key::kBrowsingDataLifetime)
                  ->HasMessage(policy::PolicyMap::MessageType::kInfo));

  // Check that prefs are set for sync types disabled as a result of both
  // policies.
  bool enabled;
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncBookmarks, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncPreferences, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncTypedUrls, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncTabs, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncSavedTabGroups, &enabled));
  EXPECT_FALSE(enabled);

  // Set ClearBrowsingDataOnExitList for some other types.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  base::Value::List clear_browsing_data_list = base::Value::List()
                                                   .Append("autofill")
                                                   .Append("password_signin")
                                                   .Append("hosted_app_data")
                                                   .Append("download_history");

  policy.Set(policy::key::kClearBrowsingDataOnExitList,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
             policy::POLICY_SOURCE_CLOUD,
             base::Value(std::move(clear_browsing_data_list)), nullptr);

  BrowsingDataLifetimePolicyHandler clear_browsing_data_handler(
      policy::key::kClearBrowsingDataOnExitList,
      browsing_data::prefs::kBrowsingDataLifetime,
      policy::Schema::Wrap(policy::GetChromeSchemaData()));

  clear_browsing_data_handler.CheckPolicySettings(policy, &errors);
  EXPECT_TRUE(errors.empty());

  // Check that an info message is added for the BrowsingDataLifetimeHandler.
  clear_browsing_data_handler.ApplyPolicySettings(policy, &prefs);
  clear_browsing_data_handler.PrepareForDisplaying(&policy);
  EXPECT_TRUE(policy.Get(policy::key::kBrowsingDataLifetime)
                  ->HasMessage(policy::PolicyMap::MessageType::kInfo));

  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncAutofill, &enabled));
  EXPECT_FALSE(enabled);
  ASSERT_TRUE(prefs.GetBoolean(syncer::prefs::kSyncPasswords, &enabled));
  EXPECT_FALSE(enabled);
#endif
}
