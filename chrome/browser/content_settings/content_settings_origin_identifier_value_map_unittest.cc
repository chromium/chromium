// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::COOKIES, std::string()));
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, std::string(), base::Time(),
               base::Value(1));

  std::unique_ptr<base::Value> expected_value(new base::Value(1));
  EXPECT_TRUE(expected_value->Equals(
      map.GetValue(GURL("http://www.google.com"), GURL("http://www.google.com"),
                   ContentSettingsType::COOKIES, std::string())));

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.youtube.com"),
                               ContentSettingsType::COOKIES, std::string()));

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.youtube.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::COOKIES, std::string()));

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::POPUPS, std::string()));

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::COOKIES, "resource_id"));
}

TEST(OriginIdentifierValueMapTest, SetDeleteValue) {
  content_settings::OriginIdentifierValueMap map;

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::PLUGINS, "java-plugin"));

  // Set sample values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::PLUGINS, "java-plugin", base::Time(),
               base::Value(1));

  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           ContentSettingsType::PLUGINS, "java-plugin")
                  ->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::PLUGINS, "flash-plugin"));
  // Delete non-existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsType::PLUGINS, "flash-plugin");
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::PLUGINS, "flash-plugin"));
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           ContentSettingsType::PLUGINS, "java-plugin")
                  ->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  // Delete existing value.
  map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsPattern::FromString("[*.]google.com"),
                  ContentSettingsType::PLUGINS, "java-plugin");

  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::PLUGINS, "java-plugin"));
}

TEST(OriginIdentifierValueMapTest, Clear) {
  content_settings::OriginIdentifierValueMap map;
  EXPECT_TRUE(map.empty());

  // Set two values.
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::PLUGINS, "java-plugin", base::Time(),
               base::Value(1));
  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, std::string(), base::Time(),
               base::Value(1));
  EXPECT_FALSE(map.empty());
  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           ContentSettingsType::PLUGINS, "java-plugin")
                  ->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  // Clear the map.
  map.clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(NULL, map.GetValue(GURL("http://www.google.com"),
                               GURL("http://www.google.com"),
                               ContentSettingsType::PLUGINS, "java-plugin"));
}

TEST(OriginIdentifierValueMapTest, ListEntryPrecedences) {
  content_settings::OriginIdentifierValueMap map;

  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, std::string(), base::Time(),
               base::Value(1));

  map.SetValue(ContentSettingsPattern::FromString("www.google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, std::string(), base::Time(),
               base::Value(2));

  int actual_value;
  EXPECT_TRUE(map.GetValue(GURL("http://mail.google.com"),
                           GURL("http://www.google.com"),
                           ContentSettingsType::COOKIES, std::string())
                  ->GetAsInteger(&actual_value));
  EXPECT_EQ(1, actual_value);

  EXPECT_TRUE(map.GetValue(GURL("http://www.google.com"),
                           GURL("http://www.google.com"),
                           ContentSettingsType::COOKIES, std::string())
                  ->GetAsInteger(&actual_value));
  EXPECT_EQ(2, actual_value);
}

TEST(OriginIdentifierValueMapTest, IterateEmpty) {
  content_settings::OriginIdentifierValueMap map;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, std::string(),
                          nullptr));
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
  base::Time t2 = t1 + base::TimeDelta::FromSeconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, std::string(), t1, base::Value(1));
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, std::string(), t2, base::Value(2));

  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES, std::string(), NULL));
  ASSERT_TRUE(rule_iterator->HasNext());
  content_settings::Rule rule = rule_iterator->Next();
  EXPECT_EQ(sub_pattern, rule.primary_pattern);
  EXPECT_EQ(2, content_settings::ValueToContentSetting(&rule.value));
  EXPECT_EQ(t2,
            map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                ContentSettingsType::COOKIES, std::string()));

  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(pattern, rule.primary_pattern);
  EXPECT_EQ(1, content_settings::ValueToContentSetting(&rule.value));
  EXPECT_EQ(t1,
            map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                ContentSettingsType::COOKIES, std::string()));
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
               ContentSettingsType::COOKIES, std::string(), t1, base::Value(1));
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, std::string(), t1, base::Value(2));

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator(
        map.GetRuleIterator(ContentSettingsType::COOKIES, std::string(), NULL));
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(&rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES, std::string()));
    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(1, content_settings::ValueToContentSetting(&rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES, std::string()));
    ASSERT_FALSE(rule_iterator->HasNext());
  }
  base::Time t2 = t1 + base::TimeDelta::FromSeconds(1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, std::string(), t2, base::Value(3));

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator =
        map.GetRuleIterator(ContentSettingsType::COOKIES, std::string(), NULL);
    ASSERT_TRUE(rule_iterator->HasNext());
    content_settings::Rule rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule.primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(&rule.value));
    EXPECT_EQ(t1,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES, std::string()));
    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule.primary_pattern);
    EXPECT_EQ(3, content_settings::ValueToContentSetting(&rule.value));
    EXPECT_EQ(t2,
              map.GetLastModified(rule.primary_pattern, rule.secondary_pattern,
                                  ContentSettingsType::COOKIES, std::string()));
    ASSERT_FALSE(rule_iterator->HasNext());
  }
}
