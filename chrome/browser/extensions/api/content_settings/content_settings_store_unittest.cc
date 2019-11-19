// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"

#include <stdint.h>

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Mock;

namespace extensions {

namespace keys = content_settings_api_constants;

namespace {

void CheckRule(const content_settings::Rule& rule,
               const ContentSettingsPattern& primary_pattern,
               const ContentSettingsPattern& secondary_pattern,
               ContentSetting setting) {
  EXPECT_EQ(primary_pattern.ToString(), rule.primary_pattern.ToString());
  EXPECT_EQ(secondary_pattern.ToString(), rule.secondary_pattern.ToString());
  EXPECT_EQ(setting, content_settings::ValueToContentSetting(&rule.value));
}

// Helper class which returns monotonically-increasing base::Time objects.
class FakeTimer {
 public:
  FakeTimer() : internal_(0) {}

  base::Time GetNext() {
    return base::Time::FromInternalValue(++internal_);
  }

 private:
  int64_t internal_;
};

class MockContentSettingsStoreObserver
    : public ContentSettingsStore::Observer {
 public:
  MOCK_METHOD2(OnContentSettingChanged,
               void(const std::string& extension_id, bool incognito));
};

ContentSetting GetContentSettingFromStore(
    const ContentSettingsStore* store,
    const GURL& primary_url, const GURL& secondary_url,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool incognito) {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      store->GetRuleIterator(content_type, resource_identifier, incognito));
  std::unique_ptr<base::Value> setting(
      content_settings::TestUtils::GetContentSettingValueAndPatterns(
          rule_iterator.get(), primary_url, secondary_url, nullptr, nullptr));
  return content_settings::ValueToContentSetting(setting.get());
}

std::vector<content_settings::Rule> GetSettingsForOneTypeFromStore(
    const ContentSettingsStore* store,
    ContentSettingsType content_type,
    const std::string& resource_identifier,
    bool incognito) {
  std::vector<content_settings::Rule> rules;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      store->GetRuleIterator(content_type, resource_identifier, incognito));
  if (rule_iterator) {
    while (rule_iterator->HasNext())
      rules.push_back(rule_iterator->Next());
  }
  return rules;
}

}  // namespace

class ContentSettingsStoreTest : public ::testing::Test {
 public:
  ContentSettingsStoreTest() :
      store_(new ContentSettingsStore()) {
  }

 protected:
  void RegisterExtension(const std::string& ext_id) {
    store_->RegisterExtension(ext_id, timer_.GetNext(), true);
  }

  ContentSettingsStore* store() {
    return store_.get();
  }

 private:
  FakeTimer timer_;
  scoped_refptr<ContentSettingsStore> store_;
};

TEST_F(ContentSettingsStoreTest, RegisterUnregister) {
  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  // Register first extension
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  // Set setting
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->SetExtensionContentSetting(
      ext_id, pattern, pattern, ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_ALLOW, kExtensionPrefsScopeRegular);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->SetExtensionContentSetting(
      ext_id_2, pattern, pattern, ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_BLOCK, kExtensionPrefsScopeRegular);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  // Unregister first extension. This shouldn't change the setting.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));
  store()->UnregisterExtension(ext_id);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));
  Mock::VerifyAndClear(&observer);

  // Unregister second extension. This should reset the setting to its default
  // value.
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id_2, false));
  store()->UnregisterExtension(ext_id_2);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  store()->RemoveObserver(&observer);
}

TEST_F(ContentSettingsStoreTest, GetAllSettings) {
  const bool incognito = false;
  std::vector<content_settings::Rule> rules = GetSettingsForOneTypeFromStore(
      store(), ContentSettingsType::COOKIES, std::string(), incognito);
  ASSERT_EQ(0u, rules.size());

  // Register first extension.
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  store()->SetExtensionContentSetting(
      ext_id, pattern, pattern, ContentSettingsType::COOKIES, std::string(),
      CONTENT_SETTING_ALLOW, kExtensionPrefsScopeRegular);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         std::string(), incognito);
  ASSERT_EQ(1u, rules.size());
  CheckRule(rules[0], pattern, pattern, CONTENT_SETTING_ALLOW);

  // Register second extension.
  std::string ext_id_2("my_second_extension");
  RegisterExtension(ext_id_2);
  ContentSettingsPattern pattern_2 =
      ContentSettingsPattern::FromURL(GURL("http://www.example.com"));
  store()->SetExtensionContentSetting(
      ext_id_2, pattern_2, pattern_2, ContentSettingsType::COOKIES,
      std::string(), CONTENT_SETTING_BLOCK, kExtensionPrefsScopeRegular);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         std::string(), incognito);
  ASSERT_EQ(2u, rules.size());
  // Rules appear in the reverse installation order of the extensions.
  CheckRule(rules[0], pattern_2, pattern_2, CONTENT_SETTING_BLOCK);
  CheckRule(rules[1], pattern, pattern, CONTENT_SETTING_ALLOW);

  // Disable first extension.
  store()->SetExtensionState(ext_id, false);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         std::string(), incognito);
  ASSERT_EQ(1u, rules.size());
  CheckRule(rules[0], pattern_2, pattern_2, CONTENT_SETTING_BLOCK);

  // Uninstall second extension.
  store()->UnregisterExtension(ext_id_2);

  rules = GetSettingsForOneTypeFromStore(store(), ContentSettingsType::COOKIES,
                                         std::string(), incognito);
  ASSERT_EQ(0u, rules.size());
}

TEST_F(ContentSettingsStoreTest, SetFromList) {
  // Force creation of ContentSettingsRegistry, so that the string to content
  // setting type lookup can succeed.
  content_settings::ContentSettingsRegistry::GetInstance();

  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL url("http://www.youtube.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  // Register first extension
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  // Set setting via a list
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(GURL("http://www.youtube.com"));
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false));

  // Build a preference list in JSON format.
  base::ListValue pref_list;
  // {"primaryPattern": pattern, "secondaryPattern": pattern, "type": "cookies",
  //  "setting": "allow"}
  auto dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString(keys::kPrimaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kSecondaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kContentSettingsTypeKey, "cookies");
  dict_value->SetString(keys::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));
  // Test content settings types that have been removed. Should be ignored.
  // {"primaryPattern": pattern, "secondaryPattern": pattern,
  //  "type": "fullscreen", "setting": "allow"}
  dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString(keys::kPrimaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kSecondaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kContentSettingsTypeKey, "fullscreen");
  dict_value->SetString(keys::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));
  // {"primaryPattern": pattern, "secondaryPattern": pattern,
  //  "type": "mouselock", "setting": "allow"}
  dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString(keys::kPrimaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kSecondaryPatternKey, pattern.ToString());
  dict_value->SetString(keys::kContentSettingsTypeKey, "mouselock");
  dict_value->SetString(keys::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  store()->SetExtensionContentSettingFromList(ext_id, &pref_list,
                                              kExtensionPrefsScopeRegular);
  Mock::VerifyAndClear(&observer);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSettingFromStore(store(), url, url,
                                       ContentSettingsType::COOKIES,
                                       std::string(), false));

  store()->RemoveObserver(&observer);
}

// Test that embedded patterns are properly removed.
TEST_F(ContentSettingsStoreTest, RemoveEmbedded) {
  content_settings::ContentSettingsRegistry::GetInstance();

  ::testing::StrictMock<MockContentSettingsStoreObserver> observer;
  store()->AddObserver(&observer);

  GURL primary_url("http://www.youtube.com");
  GURL secondary_url("http://www.google.com");

  // Register first extension.
  std::string ext_id("my_extension");
  RegisterExtension(ext_id);

  // Set setting via a list.
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURL(primary_url);
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromURL(secondary_url);
  EXPECT_CALL(observer, OnContentSettingChanged(ext_id, false)).Times(4);

  // Build a preference list in JSON format.
  base::ListValue pref_list;
  auto dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString(keys::kPrimaryPatternKey, primary_pattern.ToString());
  dict_value->SetString(keys::kSecondaryPatternKey,
                        secondary_pattern.ToString());
  dict_value->SetString(keys::kContentSettingsTypeKey, "cookies");
  dict_value->SetString(keys::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  dict_value = std::make_unique<base::DictionaryValue>();
  dict_value->SetString(keys::kPrimaryPatternKey, primary_pattern.ToString());
  dict_value->SetString(keys::kSecondaryPatternKey,
                        secondary_pattern.ToString());
  dict_value->SetString(keys::kContentSettingsTypeKey, "geolocation");
  dict_value->SetString(keys::kContentSettingKey, "allow");
  pref_list.Append(std::move(dict_value));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        ::features::kPermissionDelegation);
    store()->SetExtensionContentSettingFromList(ext_id, &pref_list,
                                                kExtensionPrefsScopeRegular);

    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              GetContentSettingFromStore(store(), primary_url, secondary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              GetContentSettingFromStore(store(), primary_url, secondary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));

    store()->ClearContentSettingsForExtension(ext_id,
                                              kExtensionPrefsScopeRegular);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(::features::kPermissionDelegation);
    store()->SetExtensionContentSettingFromList(ext_id, &pref_list,
                                                kExtensionPrefsScopeRegular);

    // The embedded geolocation pattern should be removed but cookies kept.
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              GetContentSettingFromStore(store(), primary_url, secondary_url,
                                         ContentSettingsType::COOKIES,
                                         std::string(), false));
    EXPECT_EQ(CONTENT_SETTING_DEFAULT,
              GetContentSettingFromStore(store(), primary_url, secondary_url,
                                         ContentSettingsType::GEOLOCATION,
                                         std::string(), false));
  }

  Mock::VerifyAndClear(&observer);
  store()->RemoveObserver(&observer);
}

}  // namespace extensions
