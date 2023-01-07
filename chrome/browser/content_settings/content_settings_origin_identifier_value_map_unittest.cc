// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"

#include <memory>

#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(OriginIdentifierValueMapTest, SetGetValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::COOKIES));

  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, base::Value(1), {});

  const base::Value* value =
      map.GetValue(GURL("http://www.google.com"), GURL("http://www.google.com"),
                   ContentSettingsType::COOKIES);
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value(1), *value);

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.youtube.com"),
                                  ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.youtube.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::POPUPS));
}

TEST(OriginIdentifierValueMapTest, SetDeleteValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::GEOLOCATION));

  // Set sample values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::GEOLOCATION, base::Value(1), {});

  {
    const base::Value* value = map.GetValue(GURL("http://www.google.com"),
                                            GURL("http://www.google.com"),
                                            ContentSettingsType::GEOLOCATION);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::NOTIFICATIONS));
  // Delete non-existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::NOTIFICATIONS));
  {
    const base::Value* value = map.GetValue(GURL("http://www.google.com"),
                                            GURL("http://www.google.com"),
                                            ContentSettingsType::GEOLOCATION);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }

  // Delete existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsType::GEOLOCATION);

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::GEOLOCATION));
}

TEST(OriginIdentifierValueMapTest, Clear) {
  content_settings::OriginIdentifierValueMap map;
  EXPECT_TRUE(map.empty());

  // Set two values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::GEOLOCATION, base::Value(1), {});
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, base::Value(1), {});
  EXPECT_FALSE(map.empty());
  const base::Value* value =
      map.GetValue(GURL("http://www.google.com"), GURL("http://www.google.com"),
                   ContentSettingsType::GEOLOCATION);
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(1, value->GetInt());

  // Clear the map.
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::GEOLOCATION));
}

TEST(OriginIdentifierValueMapTest, ListEntryPrecedences) {
  content_settings::OriginIdentifierValueMap map;

  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, base::Value(1), {});

  map.SetValue(ContentSettingsPattern::FromString("www.google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, base::Value(2), {});

  {
    const base::Value* value = map.GetValue(GURL("http://mail.google.com"),
                                            GURL("http://www.google.com"),
                                            ContentSettingsType::COOKIES);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(1, value->GetInt());
  }

  {
    const base::Value* value = map.GetValue(GURL("http://www.google.com"),
                                            GURL("http://www.google.com"),
                                            ContentSettingsType::COOKIES);
    ASSERT_TRUE(value->is_int());
    EXPECT_EQ(2, value->GetInt());
  }
}

TEST(OriginIdentifierValueMapTest, IterateEmpty) {
  content_settings::OriginIdentifierValueMap map;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
  EXPECT_FALSE(rule_iterator);
}

TEST(OriginIdentifierValueMapTest, IterateNonempty) {
  // Verify the precedence order.
  content_settings::OriginIdentifierValueMap map;
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.google.com");
  base::Time t1 = base::Time::Now();
  base::Time t2 = t1 + base::Seconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(1),
               {.last_modified = t1});
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(2),
               {.last_modified = t2});

  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
  ASSERT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(sub_pattern, rule.primary_pattern);
  EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
  EXPECT_EQ(t2, rule.metadata.last_modified);

  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(pattern, rule.primary_pattern);
  EXPECT_EQ(1, content_settings::ValueToContentSetting(rule.value));
  EXPECT_EQ(t1, rule.metadata.last_modified);
}

TEST(OriginIdentifierValueMapTest, UpdateLastModified) {
  // Verify that the last_modified timestamp is updated.
  content_settings::OriginIdentifierValueMap map;
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.google.com");

  base::Time t1 = base::Time::Now();
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(1),
               {.last_modified = t1,
                .expiration = base::Time(),
                .session_model = content_settings::SessionModel::Durable});
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(2),
               {.last_modified = t1,
                .expiration = content_settings::GetConstraintExpiration(
                    base::Seconds(100)),
                .session_model = content_settings::SessionModel::UserSession});

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator(
        map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr));
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1, rule.metadata.last_modified);
    ASSERT_FALSE(rule.metadata.expiration.is_null());
    EXPECT_GT(rule.metadata.expiration, base::Time::Now());
    EXPECT_EQ(rule.metadata.session_model,
              content_settings::SessionModel::UserSession);

    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(1, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1, rule.metadata.last_modified);
    ASSERT_TRUE(rule.metadata.expiration.is_null());
    EXPECT_EQ(rule.metadata.session_model,
              content_settings::SessionModel::Durable);
    ASSERT_FALSE(rule_iterator->HasNext());
  }
  base::Time t2 = t1 + base::Seconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(3),
               {.last_modified = t2});

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator =
        map.GetRuleIterator(ContentSettingsType::COOKIES, nullptr);
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t1, rule.metadata.last_modified);
    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(3, content_settings::ValueToContentSetting(rule.value));
    EXPECT_EQ(t2, rule.metadata.last_modified);
    ASSERT_FALSE(rule_iterator->HasNext());
  }
}
