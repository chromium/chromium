// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_supervised_provider.h"

#include <memory>
#include <string>

#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content_settings {

class SupervisedUserProviderTest : public ::testing::Test {
 public:
  SupervisedUserProviderTest() {}

  void SetUp() override;
  void TearDown() override;

 protected:
  SupervisedUserSettingsService service_;
  scoped_refptr<TestingPrefStore> pref_store_;
  std::unique_ptr<SupervisedProvider> provider_;
  content_settings::MockObserver mock_observer_;
};

void SupervisedUserProviderTest::SetUp() {
  pref_store_ = new TestingPrefStore();
  pref_store_->NotifyInitializationCompleted();
  service_.Init(pref_store_);
  service_.SetActive(true);
  provider_ = std::make_unique<SupervisedProvider>(&service_);
  provider_->AddObserver(&mock_observer_);
}

void SupervisedUserProviderTest::TearDown() {
  provider_->RemoveObserver(&mock_observer_);
  provider_->ShutdownOnUIThread();
  service_.Shutdown();
}

TEST_F(SupervisedUserProviderTest, GeolocationTest) {
  std::unique_ptr<RuleIterator> rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::GEOLOCATION, std::string(), false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION,
                                      std::string()));
  service_.SetLocalSetting(supervised_users::kGeolocationDisabled,
                           std::make_unique<base::Value>(true));

  rule_iterator = provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION,
                                             std::string(), false);
  ASSERT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

  // Re-enable the default geolocation setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION,
                                      std::string()));
  service_.SetLocalSetting(supervised_users::kGeolocationDisabled,
                           std::make_unique<base::Value>(false));

  rule_iterator = provider_->GetRuleIterator(ContentSettingsType::GEOLOCATION,
                                             std::string(), false);
  EXPECT_FALSE(rule_iterator);
}

TEST_F(SupervisedUserProviderTest, CookiesTest) {
  std::unique_ptr<RuleIterator> rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::COOKIES, std::string(), false);
  EXPECT_FALSE(rule_iterator);

  // Allow cookies everywhere.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::COOKIES,
                                      std::string()));
  service_.SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                           std::make_unique<base::Value>(true));

  rule_iterator = provider_->GetRuleIterator(ContentSettingsType::COOKIES,
                                             std::string(), false);
  ASSERT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, ValueToContentSetting(&rule.value));

  // Re-enable the default cookie setting.
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(_, _, ContentSettingsType::COOKIES,
                                      std::string()));
  service_.SetLocalSetting(supervised_users::kCookiesAlwaysAllowed,
                           std::make_unique<base::Value>(false));

  rule_iterator = provider_->GetRuleIterator(ContentSettingsType::COOKIES,
                                             std::string(), false);
  EXPECT_FALSE(rule_iterator);
}

TEST_F(SupervisedUserProviderTest, CameraMicTest) {
  std::unique_ptr<RuleIterator> rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_CAMERA, std::string(), false);
  EXPECT_FALSE(rule_iterator);
  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_MIC, std::string(), false);
  EXPECT_FALSE(rule_iterator);

  // Disable the default camera and microphone setting.
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, ContentSettingsType::MEDIASTREAM_CAMERA,
                                  std::string()));
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  service_.SetLocalSetting(supervised_users::kCameraMicDisabled,
                           std::make_unique<base::Value>(true));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_CAMERA, std::string(), false);
  ASSERT_TRUE(rule_iterator->HasNext());
  Rule rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_MIC, std::string(), false);
  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_FALSE(rule_iterator->HasNext());

  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rule.secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, ValueToContentSetting(&rule.value));

  // Re-enable the default camera and microphone setting.
  EXPECT_CALL(mock_observer_, OnContentSettingChanged(
                                  _, _, ContentSettingsType::MEDIASTREAM_CAMERA,
                                  std::string()));
  EXPECT_CALL(mock_observer_,
              OnContentSettingChanged(
                  _, _, ContentSettingsType::MEDIASTREAM_MIC, std::string()));
  service_.SetLocalSetting(supervised_users::kCameraMicDisabled,
                           std::make_unique<base::Value>(false));

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_CAMERA, std::string(), false);
  EXPECT_FALSE(rule_iterator);

  rule_iterator = provider_->GetRuleIterator(
      ContentSettingsType::MEDIASTREAM_MIC, std::string(), false);
  EXPECT_FALSE(rule_iterator);
}

}  // namespace content_settings
