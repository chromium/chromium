// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/javascript_optimizer_provider_android.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using safe_browsing::AdvancedProtectionStatusManager;
using safe_browsing::AdvancedProtectionStatusManagerFactory;
using testing::_;

const char* kTestUrl = "https://www.google.com/";

class TestAdvancedProtectionStatusManager
    : public AdvancedProtectionStatusManager {
 public:
  TestAdvancedProtectionStatusManager() = default;
  ~TestAdvancedProtectionStatusManager() override = default;

  bool IsUnderAdvancedProtection() const override {
    return is_under_advanced_protection_;
  }

  void SetAdvancedProtectionStatusForTesting(bool enrolled) override {
    is_under_advanced_protection_ = enrolled;
    NotifyObserversStatusChanged();
  }

 private:
  bool is_under_advanced_protection_ = false;
};

}  // anonymous namespace

class JavascriptOptimizerProviderAndroidTest : public testing::Test {
 public:
  JavascriptOptimizerProviderAndroidTest() = default;
  ~JavascriptOptimizerProviderAndroidTest() override = default;

  void SetUp() override {
    advanced_protection_manager_ =
        std::make_unique<TestAdvancedProtectionStatusManager>();
  }

  void SetAdvancedProtectionStatus(bool is_under_advanced_protection) {
    advanced_protection_manager_->SetAdvancedProtectionStatusForTesting(
        is_under_advanced_protection);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestAdvancedProtectionStatusManager>
      advanced_protection_manager_;
};

std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
    JavascriptOptimizerProviderAndroid& provider,
    ContentSettingsType content_type) {
  return provider.GetRuleIterator(
      content_type, /*off_the_record=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
}

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRuleIterator_UnderAdvancedProtection) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
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

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRuleIterator_NotUnderAdvancedProtection) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/false);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule_iterator);
}

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRuleIterator_IncompatibleContentType) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::RuleIterator> rule_iterator =
      GetRuleIterator(provider, ContentSettingsType::COOKIES);
  EXPECT_EQ(nullptr, rule_iterator);
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRuleIterator_AfterShutdown) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
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
  // Calling GetRuleIterator() shouldn't crash.
  GetRuleIterator(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
}

std::unique_ptr<content_settings::Rule> GetRule(
    JavascriptOptimizerProviderAndroid& provider,
    ContentSettingsType content_type) {
  return provider.GetRule(
      GURL(kTestUrl), GURL(kTestUrl), content_type,
      /*off_the_record=*/false,
      content_settings::PartitionKey::GetDefaultForTesting());
}

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRule_UnderAdvancedProtection) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule.get() != nullptr);
  EXPECT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule->value);
}

TEST_F(JavascriptOptimizerProviderAndroidTest,
       GetRule_NotUnderAdvancedProtection) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/false);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  EXPECT_EQ(nullptr, rule.get());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_IncompatibleCategory) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::COOKIES);
  EXPECT_EQ(nullptr, rule.get());
}

TEST_F(JavascriptOptimizerProviderAndroidTest, GetRule_AfterShutdown) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);

  std::unique_ptr<content_settings::Rule> rule =
      GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
  ASSERT_TRUE(rule.get() != nullptr);
  ASSERT_EQ(base::Value(CONTENT_SETTING_BLOCK), rule->value);

  provider.ShutdownOnUIThread();
  // Calling GetRule() shouldn't crash.
  GetRule(provider, ContentSettingsType::JAVASCRIPT_OPTIMIZER);
}

TEST_F(JavascriptOptimizerProviderAndroidTest, RecordHistogram) {
  const char* kHistogram =
      "ContentSettings.RegularProfile.DefaultJavascriptOptimizationBlockedByOs";
  bool kTestCases[] = {true, false};

  for (bool is_under_advanced_protection : kTestCases) {
    base::HistogramTester histogram_tester;
    SetAdvancedProtectionStatus(is_under_advanced_protection);
    JavascriptOptimizerProviderAndroid provider(
        advanced_protection_manager_.get(),
        /*should_record_metrics=*/true);
    histogram_tester.ExpectUniqueSample(kHistogram,
                                        is_under_advanced_protection, 1);

    provider.ShutdownOnUIThread();
  }
}

// Test that the JavascriptOptimizerProviderAndroid observers are notified when
// the setting changes.
TEST_F(JavascriptOptimizerProviderAndroidTest, ObserverNotified) {
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/false);
  JavascriptOptimizerProviderAndroid provider(
      advanced_protection_manager_.get(),
      /*should_record_metrics=*/false);
  content_settings::MockObserver mock_observer;
  provider.AddObserver(&mock_observer);

  EXPECT_CALL(
      mock_observer,
      OnContentSettingChanged(_, _, ContentSettingsType::JAVASCRIPT_OPTIMIZER));
  SetAdvancedProtectionStatus(/*is_under_advanced_protection=*/true);

  provider.ShutdownOnUIThread();
}
