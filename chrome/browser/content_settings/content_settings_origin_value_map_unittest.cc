// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_origin_value_map.h"

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
std::vector<ContentSettingsPattern> ToPrimaryPatternVector(
    content_settings::RuleIterator& iterator) {
  std::vector<ContentSettingsPattern> v;
  while (iterator.HasNext()) {
    v.push_back(iterator.Next()->primary_pattern);
  }
  return v;
}

}  // namespace

class OriginValueMapTest : public testing::Test {
 public:
  OriginValueMapTest() = default;

 private:
};

TEST_F(OriginValueMapTest, SetGetValue) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());

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

TEST_F(OriginValueMapTest, GetRule) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());

  EXPECT_EQ(nullptr, map.GetRule(GURL("http://www.google.com"),
                                 GURL("http://www.google.com"),
                                 ContentSettingsType::COOKIES));

  map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsPattern::FromString("[*.]google.com"),
               ContentSettingsType::COOKIES, base::Value(1), {});

  auto rule =
      map.GetRule(GURL("http://www.google.com"), GURL("http://www.google.com"),
                  ContentSettingsType::COOKIES);
  ASSERT_TRUE(rule);
  EXPECT_EQ(base::Value(1), rule->value);

  EXPECT_EQ(nullptr, map.GetRule(GURL("http://www.google.com"),
                                 GURL("http://www.youtube.com"),
                                 ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetRule(GURL("http://www.youtube.com"),
                                 GURL("http://www.google.com"),
                                 ContentSettingsType::COOKIES));

  EXPECT_EQ(nullptr, map.GetRule(GURL("http://www.google.com"),
                                 GURL("http://www.google.com"),
                                 ContentSettingsType::POPUPS));
}

TEST_F(OriginValueMapTest, GetRuleIterator) {
  content_settings::OriginValueMap map;
  auto pattern = ContentSettingsPattern::FromString;
  auto kWildcard = pattern("*");

  {
    base::AutoLock lock(map.GetLock());
    EXPECT_TRUE(map.SetValue(pattern("maps.google.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("photos.google.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("[*.]maps.google.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("[*.]photos.google.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("zoogle.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("[*.]zoogle.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
    EXPECT_TRUE(map.SetValue(pattern("[*.]google.com"), kWildcard,
                             ContentSettingsType::COOKIES, base::Value(1), {}));
  }

  EXPECT_THAT(ToPrimaryPatternVector(
                  *map.GetRuleIterator(ContentSettingsType::COOKIES)),
              testing::ElementsAre(
                  pattern("maps.google.com"), pattern("[*.]maps.google.com"),
                  pattern("photos.google.com"),
                  pattern("[*.]photos.google.com"), pattern("[*.]google.com"),
                  pattern("zoogle.com"), pattern("[*.]zoogle.com")));
}

TEST_F(OriginValueMapTest, SetValueReturnsChanges) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());

  // Initial call return true.
  EXPECT_TRUE(map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsType::COOKIES, base::Value(1), {}));

  // An identical call return false.
  EXPECT_FALSE(
      map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
                   ContentSettingsPattern::FromString("[*.]google.com"),
                   ContentSettingsType::COOKIES, base::Value(1), {}));

  // A change in value returns true.
  EXPECT_TRUE(map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsType::COOKIES, base::Value(2), {}));

  // A change in metadata returns true.
  content_settings::RuleMetaData metadata;
  metadata.set_session_model(content_settings::mojom::SessionModel::ONE_TIME);
  EXPECT_TRUE(map.SetValue(ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsPattern::FromString("[*.]google.com"),
                           ContentSettingsType::COOKIES, base::Value(2),
                           metadata));
}

TEST_F(OriginValueMapTest, SetDeleteValue) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());

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
  EXPECT_FALSE(
      map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                      ContentSettingsPattern::FromString("[*.]google.com"),
                      ContentSettingsType::NOTIFICATIONS));
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
  EXPECT_TRUE(
      map.DeleteValue(ContentSettingsPattern::FromString("[*.]google.com"),
                      ContentSettingsPattern::FromString("[*.]google.com"),
                      ContentSettingsType::GEOLOCATION));

  EXPECT_EQ(nullptr, map.GetValue(GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  ContentSettingsType::GEOLOCATION));
}

TEST_F(OriginValueMapTest, Clear) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());
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

TEST_F(OriginValueMapTest, ListEntryPrecedences) {
  content_settings::OriginValueMap map;
  base::AutoLock lock(map.GetLock());

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

TEST_F(OriginValueMapTest, IterateEmpty) {
  content_settings::OriginValueMap map;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES));
  EXPECT_FALSE(rule_iterator);
}

TEST_F(OriginValueMapTest, IterateNonempty) {
  // Verify the precedence order.
  content_settings::OriginValueMap map;

  map.GetLock().Acquire();
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.google.com");
  base::Time t1 = base::Time::Now();
  base::Time t2 = t1 + base::Seconds(1);
  content_settings::RuleMetaData metadata;
  metadata.set_last_modified(t1);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(1), metadata);
  metadata.set_last_modified(t2);
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(2), metadata);

  map.GetLock().Release();
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      map.GetRuleIterator(ContentSettingsType::COOKIES));
  ASSERT_TRUE(rule_iterator->HasNext());
  std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
  EXPECT_EQ(sub_pattern, rule->primary_pattern);
  EXPECT_EQ(2, content_settings::ValueToContentSetting(rule->value));
  EXPECT_EQ(t2, rule->metadata.last_modified());

  ASSERT_TRUE(rule_iterator->HasNext());
  rule = rule_iterator->Next();
  EXPECT_EQ(pattern, rule->primary_pattern);
  EXPECT_EQ(1, content_settings::ValueToContentSetting(rule->value));
  EXPECT_EQ(t1, rule->metadata.last_modified());
}

TEST_F(OriginValueMapTest, UpdateLastModified) {
  // Verify that the last_modified timestamp is updated.
  content_settings::OriginValueMap map;
  map.GetLock().Acquire();
  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromString("[*.]google.com");
  ContentSettingsPattern sub_pattern =
      ContentSettingsPattern::FromString("sub.google.com");

  base::Time t1 = base::Time::Now();
  content_settings::RuleMetaData metadata;
  metadata.set_last_modified(t1);
  metadata.set_session_model(content_settings::mojom::SessionModel::DURABLE);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(1), metadata);
  metadata.SetExpirationAndLifetime(base::Time::Now() + base::Seconds(100),
                                    base::Seconds(100));
  metadata.set_session_model(
      content_settings::mojom::SessionModel::USER_SESSION);
  map.SetValue(sub_pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(2), metadata);
  map.GetLock().Release();

  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator(
        map.GetRuleIterator(ContentSettingsType::COOKIES));
    ASSERT_TRUE(rule_iterator->HasNext());
    std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule->primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule->value));
    EXPECT_EQ(t1, rule->metadata.last_modified());
    ASSERT_FALSE(rule->metadata.expiration().is_null());
    EXPECT_GT(rule->metadata.expiration(), base::Time::Now());
    EXPECT_EQ(rule->metadata.session_model(),
              content_settings::mojom::SessionModel::USER_SESSION);

    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule->primary_pattern);
    EXPECT_EQ(1, content_settings::ValueToContentSetting(rule->value));
    EXPECT_EQ(t1, rule->metadata.last_modified());
    ASSERT_TRUE(rule->metadata.expiration().is_null());
    EXPECT_EQ(rule->metadata.session_model(),
              content_settings::mojom::SessionModel::DURABLE);
    ASSERT_FALSE(rule_iterator->HasNext());
  }
  map.GetLock().Acquire();
  base::Time t2 = t1 + base::Seconds(1);
  metadata.set_last_modified(t2);
  map.SetValue(pattern, ContentSettingsPattern::Wildcard(),
               ContentSettingsType::COOKIES, base::Value(3), metadata);
  map.GetLock().Release();
  {
    std::unique_ptr<content_settings::RuleIterator> rule_iterator =
        map.GetRuleIterator(ContentSettingsType::COOKIES);
    ASSERT_TRUE(rule_iterator->HasNext());
    std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
    EXPECT_EQ(sub_pattern, rule->primary_pattern);
    EXPECT_EQ(2, content_settings::ValueToContentSetting(rule->value));
    EXPECT_EQ(t1, rule->metadata.last_modified());
    rule = rule_iterator->Next();
    EXPECT_EQ(pattern, rule->primary_pattern);
    EXPECT_EQ(3, content_settings::ValueToContentSetting(rule->value));
    EXPECT_EQ(t2, rule->metadata.last_modified());
    ASSERT_FALSE(rule_iterator->HasNext());
  }
}
