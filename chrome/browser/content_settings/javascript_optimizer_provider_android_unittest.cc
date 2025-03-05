// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/javascript_optimizer_provider_android.h"

#include "base/functional/callback_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/tab_android.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char* kTestUrl = "https://www.google.com/";

}  // anonymous namespace

using ::testing::Return;

typedef testing::Test JavascriptOptimizerProviderAndroidTest;

std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
    JavascriptOptimizerProviderAndroid& provider,
    ContentSettingsType content_type) {
  return provider.GetRuleIterator(
      content_type, /*off_the_record=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRuleIterator_NoPermission) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));
  EXPECT_FALSE(rule_iterator->HasNext());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRuleIterator_HasPermission) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(true));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule_iterator);
}

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRuleIterator_IncompatibleContentType) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::COOKIES);
  EXPECT_EQ(nullptr, rule_iterator);
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRuleIterator_AfterShutdown) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  ASSERT_EQ(ContentSettingsPattern::Wildcard(), rule->primary_pattern);
  ASSERT_EQ(CONTENT_SETTING_BLOCK,
            content_settings::ValueToContentSetting(rule->value));
  ASSERT_FALSE(rule_iterator->HasNext());

  provider.ShutdownOnUIThread();
  rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule_iterator);
}

std::unique_ptr<content_settings::Rule> GetRule(
    JavascriptOptimizerProviderAndroid& provider,
    ContentSettingsType content_type) {
  return provider.GetRule(
      GURL(kTestUrl), GURL(kTestUrl), content_type,
      /*off_the_record=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_NoPermission) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule.get() != nullptr);
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule->value);
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_HasPermission) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(true));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule.get());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_IncompatibleCategory) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::COOKIES);
  EXPECT_EQ(nullptr, rule.get());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_AfterShutdown) {
  base::MockRepeatingCallback<bool()> callback;
  ON_CALL(callback, Run()).WillByDefault(Return(false));
  JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                              /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule.get() != nullptr);
  ASSERT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule->value);

  provider.ShutdownOnUIThread();
  rule = GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule.get());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, RecordHistogram) {
  const char* kHistogram =
      "ContentSettings.RegularProfile.DefaultJavascriptOptimizationBlockedByOs";
  bool kTestCases[] = {true, false};

  for (bool os_grants_permission : kTestCases) {
    base::HistogramTester histogram_tester;
    base::MockRepeatingCallback<bool()> callback;
    ON_CALL(callback, Run()).WillByDefault(Return(os_grants_permission));
    JavascriptOptimizerProviderAndroid provider(callback.Get(),
                                                /*should_record_metrics=*/true);
    histogram_tester.ExpectUniqueSample(kHistogram, !os_grants_permission, 1);
  }
}
