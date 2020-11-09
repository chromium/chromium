// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_converter.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/extractor.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_property_names.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media_feeds {

using mojom::MediaFeedItem;
using mojom::MediaFeedItemPtr;
using schema_org::improved::mojom::Entity;
using schema_org::improved::mojom::EntityPtr;
using schema_org::improved::mojom::Property;
using schema_org::improved::mojom::PropertyPtr;
using schema_org::improved::mojom::Values;
using schema_org::improved::mojom::ValuesPtr;

class MediaFeedsConverterTest : public testing::Test {
 public:
  MediaFeedsConverterTest()
      : extractor_(
            {schema_org::entity::kCompleteDataFeed, schema_org::entity::kMovie,
             schema_org::entity::kWatchAction, schema_org::entity::kTVEpisode,
             schema_org::entity::kImageObject,
             schema_org::entity::kPropertyValue, schema_org::entity::kPerson}) {
  }

 protected:
  Property* GetProperty(Entity* entity, const std::string& name);
  PropertyPtr CreateStringProperty(const std::string& name,
                                   const std::string& value);
  PropertyPtr CreateLongProperty(const std::string& name, long value);
  PropertyPtr CreateUrlProperty(const std::string& name, const GURL& value);
  PropertyPtr CreateTimeProperty(const std::string& name, int hours);
  PropertyPtr CreateDateTimeProperty(const std::string& name,
                                     const std::string& value);
  PropertyPtr CreateDoubleProperty(const std::string& name, double value);
  PropertyPtr CreateEntityProperty(const std::string& name, EntityPtr value);
  EntityPtr ConvertJSONToEntityPtr(const std::string& json);
  EntityPtr ValidActiveWatchAction();
  EntityPtr ValidPotentialWatchAction();
  EntityPtr ValidMediaFeed();
  EntityPtr ValidEpisode(int episode_number, EntityPtr action);
  EntityPtr ValidMediaFeedItem();
  EntityPtr ValidMediaImage();
  EntityPtr WithContentAttributes(EntityPtr image);
  EntityPtr WithAssociatedOrigins(EntityPtr feed);
  EntityPtr WithCookieNameFilter(EntityPtr feed);
  EntityPtr WithCookieNameFilterAndAssociatedOrigins(EntityPtr feed);
  EntityPtr WithImage(EntityPtr entity);
  mojom::MediaFeedItemPtr ExpectedFeedItem();
  mojom::MediaImagePtr ExpectedMediaImage();
  EntityPtr AddItemToFeed(EntityPtr feed, EntityPtr item);
  media_history::MediaHistoryKeyedService::MediaFeedFetchResult GetResult(
      const schema_org::improved::mojom::EntityPtr& schema_org_entity);
  std::vector<mojom::MediaFeedItemPtr> GetResultItems(
      const schema_org::improved::mojom::EntityPtr& schema_org_entity);

  base::test::TaskEnvironment task_environment_;
  MediaFeedsConverter converter_;

 private:
  schema_org::Extractor extractor_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

Property* MediaFeedsConverterTest::GetProperty(Entity* entity,
                                               const std::string& name) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  DCHECK(property != entity->properties.end());
  DCHECK((*property)->values);
  return property->get();
}

PropertyPtr MediaFeedsConverterTest::CreateStringProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->string_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateLongProperty(const std::string& name,
                                                        long value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->long_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateUrlProperty(const std::string& name,
                                                       const GURL& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->url_values.push_back(value);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateTimeProperty(const std::string& name,
                                                        int hours) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->time_values.push_back(base::TimeDelta::FromHours(hours));
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateDateTimeProperty(
    const std::string& name,
    const std::string& value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  base::Time time;
  bool got_time = base::Time::FromString(value.c_str(), &time);
  DCHECK(got_time);
  property->values->date_time_values.push_back(time);
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateEntityProperty(
    const std::string& name,
    EntityPtr value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->entity_values.push_back(std::move(value));
  return property;
}

PropertyPtr MediaFeedsConverterTest::CreateDoubleProperty(
    const std::string& name,
    double value) {
  PropertyPtr property = Property::New();
  property->name = name;
  property->values = Values::New();
  property->values->double_values.push_back(std::move(value));
  return property;
}

EntityPtr MediaFeedsConverterTest::ConvertJSONToEntityPtr(
    const std::string& json) {
  base::RunLoop run_loop;
  EntityPtr out;

  extractor_.Extract(json, base::BindLambdaForTesting([&](EntityPtr entity) {
                       out = std::move(entity);
                       run_loop.Quit();
                     }));

  run_loop.Run();
  return out;
}

EntityPtr MediaFeedsConverterTest::ValidActiveWatchAction() {
  return ConvertJSONToEntityPtr(
      R"END(
      {
        "@type": "WatchAction",
        "target": "https://www.example.org",
        "actionStatus": "https://schema.org/ActiveActionStatus",
        "startTime": "01:00:00"
      }
    )END");
}

EntityPtr MediaFeedsConverterTest::ValidPotentialWatchAction() {
  return ConvertJSONToEntityPtr(
      R"END(
      {
        "@type": "WatchAction",
        "target": "https://www.example.com",
        "actionStatus": "https://schema.org/PotentialActionStatus",
        "startTime": "01:00:00"
      }
    )END");
}

EntityPtr MediaFeedsConverterTest::ValidMediaFeed() {
  return ConvertJSONToEntityPtr(
      R"END(
        {
          "@type": "CompleteDataFeed",
          "provider": {
            "@type": "Organization",
            "name": "Media Site",
            "logo": {
              "@type": "ImageObject",
              "width": 1113,
              "height": 245,
              "url": "https://www.example.org/logo.jpg",
              "additionalProperty": {
                "@type": "PropertyValue",
                "name": "contentAttributes",
                "value": ["forDarkBackground", "hasTitle"]
              }
            }
          }
        }
      )END");
}

EntityPtr MediaFeedsConverterTest::ValidEpisode(int episode_number,
                                                EntityPtr action) {
  EntityPtr episode = ConvertJSONToEntityPtr(
      R"END(
        {
          "@type": "TVEpisode",
          "name": "Pilot",
          "identifier": {
            "@type": "PropertyValue",
            "propertyID": "TMS_ROOT_ID",
            "value": "1"
          },
          "duration": "PT1H"
        }
      )END");
  episode->properties.push_back(
      CreateLongProperty(schema_org::property::kEpisodeNumber, episode_number));
  episode->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, std::move(action)));
  return episode;
}

EntityPtr MediaFeedsConverterTest::ValidMediaFeedItem() {
  EntityPtr item = ConvertJSONToEntityPtr(
      R"END(
        {
          "@type": "Movie",
          "@id": "12345",
          "name": "test media feed item",
          "datePublished": "1970-01-01",
          "image": "https://www.example.com/image.jpg",
          "isFamilyFriendly": "https://schema.org/True",
          "duration": "PT30M15S"
        }
      )END");

  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, ValidActiveWatchAction()));
  return item;
}

EntityPtr MediaFeedsConverterTest::ValidMediaImage() {
  return ConvertJSONToEntityPtr(
      R"END(
        {
          "@type": "ImageObject",
          "width": 336,
          "height": 188,
          "url": "https://www.example.com/image.jpg"
        }
      )END");
}

EntityPtr MediaFeedsConverterTest::WithContentAttributes(EntityPtr image) {
  auto attributes = ConvertJSONToEntityPtr(
      R"END(
        {
          "@type": "PropertyValue",
          "name": "contentAttributes",
          "value": ["hasTitle", "iconic", "poster"]
        }
      )END");
  image->properties.push_back(CreateEntityProperty(
      schema_org::property::kAdditionalProperty, std::move(attributes)));
  return image;
}

EntityPtr MediaFeedsConverterTest::WithCookieNameFilter(EntityPtr feed) {
  auto cookie_name_filter = ConvertJSONToEntityPtr(
      R"END(
      {
        "@type": "PropertyValue",
        "name": "cookieNameFilter",
        "value": "test"
      }
    )END");

  feed->properties.push_back(
      CreateEntityProperty(schema_org::property::kAdditionalProperty,
                           std::move(cookie_name_filter)));

  return feed;
}

EntityPtr MediaFeedsConverterTest::WithCookieNameFilterAndAssociatedOrigins(
    EntityPtr feed) {
  auto origins = ConvertJSONToEntityPtr(
      R"END(
      {
        "@type": "PropertyValue",
        "name": "associatedOrigin",
        "value": ["https://www.github.com", "https://www.github.com",
                  "https://www.github1.com", "https://www.github2.com" ]
      }
    )END");
  auto cookie_name_filter = ConvertJSONToEntityPtr(
      R"END(
      {
        "@type": "PropertyValue",
        "name": "cookieNameFilter",
        "value": "test"
      }
    )END");

  auto property = CreateEntityProperty(
      schema_org::property::kAdditionalProperty, std::move(origins));
  property->values->entity_values.push_back(std::move(cookie_name_filter));
  feed->properties.push_back(std::move(property));

  return feed;
}

EntityPtr MediaFeedsConverterTest::WithImage(EntityPtr entity) {
  entity->properties.push_back(
      CreateEntityProperty(schema_org::property::kImage, ValidMediaImage()));
  return entity;
}

mojom::MediaFeedItemPtr MediaFeedsConverterTest::ExpectedFeedItem() {
  mojom::MediaFeedItemPtr expected_item = mojom::MediaFeedItem::New();
  expected_item->type = mojom::MediaFeedItemType::kMovie;
  expected_item->name = base::ASCIIToUTF16("test media feed item");

  mojom::MediaImagePtr expected_image = mojom::MediaImage::New();
  expected_image->src = GURL("https://www.example.com/image.jpg");
  expected_item->images.push_back(std::move(expected_image));

  base::Time time;
  bool got_time = base::Time::FromString("1970-01-01", &time);
  DCHECK(got_time);
  expected_item->date_published = time;

  expected_item->is_family_friendly =
      media_feeds::mojom::IsFamilyFriendly::kYes;

  expected_item->action_status = mojom::MediaFeedItemActionStatus::kActive;
  expected_item->action = mojom::Action::New();
  expected_item->action->url = GURL("https://www.example.org");
  expected_item->action->start_time = base::TimeDelta::FromHours(1);

  expected_item->duration =
      base::TimeDelta::FromMinutes(30) + base::TimeDelta::FromSeconds(15);

  return expected_item;
}

mojom::MediaImagePtr MediaFeedsConverterTest::ExpectedMediaImage() {
  mojom::MediaImagePtr expected_image = mojom::MediaImage::New();

  expected_image->src = GURL("https://www.example.com/image.jpg");
  expected_image->size = gfx::Size(336, 188);

  return expected_image;
}

EntityPtr MediaFeedsConverterTest::AddItemToFeed(EntityPtr feed,
                                                 EntityPtr item) {
  PropertyPtr data_feed_items = Property::New();
  data_feed_items->name = schema_org::property::kDataFeedElement;
  data_feed_items->values = Values::New();
  data_feed_items->values->entity_values.push_back(std::move(item));
  feed->properties.push_back(std::move(data_feed_items));
  return feed;
}

media_history::MediaHistoryKeyedService::MediaFeedFetchResult
MediaFeedsConverterTest::GetResult(
    const schema_org::improved::mojom::EntityPtr& schema_org_entity) {
  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;

  converter_.ConvertMediaFeed(std::move(schema_org_entity), &result);

  return result;
}

std::vector<mojom::MediaFeedItemPtr> MediaFeedsConverterTest::GetResultItems(
    const schema_org::improved::mojom::EntityPtr& schema_org_entity) {
  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;

  EXPECT_TRUE(
      converter_.ConvertMediaFeed(std::move(schema_org_entity), &result));
  EXPECT_FALSE(result.items.empty());

  return std::move(result.items);
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidCompleteDataFeed) {
  EntityPtr entity = ValidMediaFeed();

  mojom::MediaImagePtr expected_image = mojom::MediaImage::New();
  expected_image->src = GURL("https://www.example.org/logo.jpg");
  expected_image->size = gfx::Size(1113, 245);
  expected_image->content_attributes = {
      mojom::ContentAttribute::kForDarkBackground,
      mojom::ContentAttribute::kHasTitle};

  mojom::MediaImagePtr expected_user_image = mojom::MediaImage::New();
  expected_user_image->src = GURL("https://www.example.org/profile_pic.jpg");

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
  converter_.ConvertMediaFeed(std::move(entity), &result);

  EXPECT_TRUE(result.items.empty());
  EXPECT_EQ(1u, result.logos.size());
  EXPECT_EQ(expected_image, result.logos[0]);
  EXPECT_EQ(result.display_name, "Media Site");
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidFeedWithCookieNameFilter) {
  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
  converter_.ConvertMediaFeed(WithCookieNameFilter(ValidMediaFeed()), &result);
  EXPECT_EQ("test", result.cookie_name_filter);
}

TEST_F(MediaFeedsConverterTest,
       SucceedsOnValidFeedWithAssociatedOriginsAndCookieNameFilter) {
  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
  converter_.ConvertMediaFeed(
      WithCookieNameFilterAndAssociatedOrigins(ValidMediaFeed()), &result);

  std::set<::url::Origin> expected_origins;
  expected_origins.insert(url::Origin::Create(GURL("https://www.github.com")));
  expected_origins.insert(url::Origin::Create(GURL("https://www.github1.com")));
  expected_origins.insert(url::Origin::Create(GURL("https://www.github2.com")));

  EXPECT_EQ("test", result.cookie_name_filter);
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidCompleteDataFeedWithUser) {
  auto feed = ValidMediaFeed();
  auto user = ConvertJSONToEntityPtr(
      R"END(
              {
                "@type": "Person",
                "name": "Becca Hughes",
                "image": "https://www.example.org/profile_pic.jpg",
                "email": "beccahughes@chromium.org"
              }
            )END");

  // Add the user to the provider property inside the feed.
  feed->properties[0]->values->entity_values[0]->properties.push_back(
      CreateEntityProperty(schema_org::property::kMember, std::move(user)));

  mojom::MediaImagePtr expected_user_image = mojom::MediaImage::New();
  expected_user_image->src = GURL("https://www.example.org/profile_pic.jpg");

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
  converter_.ConvertMediaFeed(std::move(feed), &result);

  ASSERT_TRUE(result.user_identifier);
  EXPECT_EQ("Becca Hughes", result.user_identifier->name);
  EXPECT_EQ(expected_user_image, result.user_identifier->image);
  EXPECT_EQ("beccahughes@chromium.org", result.user_identifier->email);
}

TEST_F(MediaFeedsConverterTest,
       SucceedsOnValidCompleteDataFeedWithMinimalUser) {
  auto feed = ValidMediaFeed();
  auto user = ConvertJSONToEntityPtr(
      R"END(
              {
                "@type": "Person",
                "name": "Becca Hughes"
              }
            )END");

  // Add the user to the provider property inside the feed.
  feed->properties[0]->values->entity_values[0]->properties.push_back(
      CreateEntityProperty(schema_org::property::kMember, std::move(user)));

  media_history::MediaHistoryKeyedService::MediaFeedFetchResult result;
  converter_.ConvertMediaFeed(std::move(feed), &result);

  ASSERT_TRUE(result.user_identifier);
  EXPECT_EQ("Becca Hughes", result.user_identifier->name);
  EXPECT_FALSE(result.user_identifier->image);
  EXPECT_FALSE(result.user_identifier->email.has_value());
}

TEST_F(MediaFeedsConverterTest, SucceedsOnValidCompleteDataFeedWithItem) {
  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), ValidMediaFeedItem());

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(ExpectedFeedItem(), result[0]);
}

TEST_F(MediaFeedsConverterTest, FailsWrongType) {
  EntityPtr entity = Entity::New();
  // Set the entity to another type that the extractor accepts, but which isn't
  // a feed type.
  entity->type = schema_org::entity::kImageObject;

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs, testing::HasSubstr("type"));
}

TEST_F(MediaFeedsConverterTest, FailsInvalidProviderOrganizationName) {
  EntityPtr entity = ValidMediaFeed();

  Property* organization =
      GetProperty(entity.get(), schema_org::property::kProvider);
  Property* organization_name =
      GetProperty(organization->values->entity_values[0].get(),
                  schema_org::property::kName);

  organization_name->values->string_values = {""};

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs, testing::HasSubstr("provider"));
}

// Fails because the logo does not have the required title content attribute.
TEST_F(MediaFeedsConverterTest, FailsInvalidProviderOrganizationLogo) {
  EntityPtr entity = ValidMediaFeed();

  Property* organization =
      GetProperty(entity.get(), schema_org::property::kProvider);
  Property* organization_logo =
      GetProperty(organization->values->entity_values[0].get(),
                  schema_org::property::kLogo);

  organization_logo->values->entity_values.clear();
  organization_logo->values->entity_values.push_back(ValidMediaImage());

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs, testing::HasSubstr("provider"));
}

// Fails because the media feed item name is empty.
TEST_F(MediaFeedsConverterTest, FailsOnInvalidMediaFeedItemName) {
  EntityPtr item = ValidMediaFeedItem();
  auto* name = GetProperty(item.get(), schema_org::property::kName);
  name->values->string_values[0] = "";

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kName));
}

// Fails because the date published is the wrong type (string instead of
// base::Time).
TEST_F(MediaFeedsConverterTest, FailsInvalidDatePublished) {
  EntityPtr item = ValidMediaFeedItem();
  auto* date_published =
      GetProperty(item.get(), schema_org::property::kDatePublished);
  auto& dates = date_published->values->date_time_values;
  dates.erase(dates.begin());
  date_published->values->string_values.push_back("1970-01-01");

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kDatePublished));
}

// Fails because the value of isFamilyFriendly property is not a parseable
// boolean type.
TEST_F(MediaFeedsConverterTest, FailsInvalidIsFamilyFriendly) {
  EntityPtr item = ValidMediaFeedItem();
  auto* is_family_friendly =
      GetProperty(item.get(), schema_org::property::kIsFamilyFriendly);
  is_family_friendly->values->string_values = {"True"};
  is_family_friendly->values->bool_values.clear();

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kIsFamilyFriendly));
}

// Fails because an active action does not contain a start time.
TEST_F(MediaFeedsConverterTest, FailsInvalidPotentialAction) {
  EntityPtr item = ValidMediaFeedItem();
  auto* action =
      GetProperty(item.get(), schema_org::property::kPotentialAction);
  auto* start_time = GetProperty(action->values->entity_values[0].get(),
                                 schema_org::property::kStartTime);
  start_time->values->time_values = {};

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kPotentialAction));
}

// Succeeds with a valid author on a video object. For other types of media,
// this field is ignored, but it must be valid on video type.
TEST_F(MediaFeedsConverterTest, SucceedsItemWithAuthor) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kVideoObject;
  EntityPtr author = Entity::New();
  author->type = schema_org::entity::kPerson;
  author->properties.push_back(
      CreateStringProperty(schema_org::property::kName, "Becca Hughes"));
  author->properties.push_back(CreateUrlProperty(
      schema_org::property::kUrl, GURL("https://www.google.com")));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kAuthor, std::move(author)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kVideo;
  expected_item->author = mojom::Author::New();
  expected_item->author->name = "Becca Hughes";
  expected_item->author->url = GURL("https://www.google.com");

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Fails because the author's name is empty.
TEST_F(MediaFeedsConverterTest, FailsInvalidAuthor) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kVideoObject;
  EntityPtr author = Entity::New();
  author->type = schema_org::entity::kPerson;
  author->properties.push_back(
      CreateStringProperty(schema_org::property::kName, ""));
  author->properties.push_back(CreateUrlProperty(
      schema_org::property::kUrl, GURL("https://www.google.com")));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kAuthor, std::move(author)));
  item->properties.push_back(
      CreateTimeProperty(schema_org::property::kDuration, 1));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kAuthor));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithInteractionStatistic) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr interaction_statistic = Entity::New();
  interaction_statistic->type = schema_org::entity::kInteractionCounter;
  interaction_statistic->properties.push_back(
      CreateUrlProperty(schema_org::property::kInteractionType,
                        GURL("https://schema.org/WatchAction")));
  interaction_statistic->properties.push_back(
      CreateDoubleProperty(schema_org::property::kUserInteractionCount, 1.0));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kInteractionStatistic,
                           std::move(interaction_statistic)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->interaction_counters = {
      {mojom::InteractionCounterType::kWatch, 1}};

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Fails because the interaction statistic property has a duplicate of the watch
// interaction type.
TEST_F(MediaFeedsConverterTest, FailsInvalidInteractionStatistic) {
  EntityPtr item = ValidMediaFeedItem();

  PropertyPtr stats_property = Property::New();
  stats_property->values = Values::New();
  stats_property->name = schema_org::property::kInteractionStatistic;
  {
    EntityPtr interaction_statistic = Entity::New();
    interaction_statistic->type = schema_org::entity::kInteractionCounter;
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kInteractionType,
                             "https://schema.org/WatchAction"));
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kUserInteractionCount, "1"));
    stats_property->values->entity_values.push_back(
        std::move(interaction_statistic));
  }
  {
    EntityPtr interaction_statistic = Entity::New();
    interaction_statistic->type = schema_org::entity::kInteractionCounter;
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kInteractionType,
                             "https://schema.org/WatchAction"));
    interaction_statistic->properties.push_back(
        CreateStringProperty(schema_org::property::kUserInteractionCount, "3"));

    stats_property->values->entity_values.push_back(
        std::move(interaction_statistic));
  }
  item->properties.push_back(std::move(stats_property));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kInteractionStatistic));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithRating) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr rating = Entity::New();
    rating->type = schema_org::entity::kRating;
    rating->properties.push_back(
        CreateStringProperty(schema_org::property::kAuthor, "MPAA"));
    rating->properties.push_back(
        CreateStringProperty(schema_org::property::kRatingValue, "G"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContentRating, std::move(rating)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  mojom::ContentRatingPtr rating = mojom::ContentRating::New();
  rating->agency = "MPAA";
  rating->value = "G";
  expected_item->content_ratings.push_back(std::move(rating));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Fails because the rating property has a rating from an unknown agency.
TEST_F(MediaFeedsConverterTest, FailsInvalidRating) {
  EntityPtr item = ValidMediaFeedItem();

  EntityPtr rating = Entity::New();
  rating->type = schema_org::entity::kRating;
  rating->properties.push_back(
      CreateStringProperty(schema_org::property::kAuthor, "Google"));
  rating->properties.push_back(
      CreateStringProperty(schema_org::property::kRatingValue, "Googley"));
  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kContentRating, std::move(rating)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kContentRating));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithGenre) {
  EntityPtr item = ValidMediaFeedItem();

  item->properties.push_back(
      CreateStringProperty(schema_org::property::kGenre, "Action"));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->genre.push_back("Action");

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidGenre) {
  EntityPtr item = ValidMediaFeedItem();

  item->properties.push_back(
      CreateStringProperty(schema_org::property::kGenre, ""));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kGenre));
}

TEST_F(MediaFeedsConverterTest, SucceedsBroadcastEvent) {
  EntityPtr publication = Entity::New();
  publication->type = schema_org::entity::kBroadcastEvent;
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kStartDate, "2020-03-22"));
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kEndDate, "2020-03-23"));
  publication->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, ValidActiveWatchAction()));

  EntityPtr item = ValidMediaFeedItem();

  // Ignore the item's action field by changing the name. Use the action
  // on the BroadcastEvent instead.
  GetProperty(item.get(), schema_org::property::kPotentialAction)->name =
      "not an action";

  publication->properties.push_back(CreateEntityProperty(
      schema_org::property::kWorkPerformed, std::move(item)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(publication));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->live = mojom::LiveDetails::New();
  base::Time start_time, end_time;
  bool parsed_start = base::Time::FromString("2020-03-22", &start_time);
  bool parsed_end = base::Time::FromString("2020-03-23", &end_time);
  DCHECK(parsed_start && parsed_end);
  expected_item->live->start_time = start_time;
  expected_item->live->end_time = end_time;

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Fails because the end date is string type instead of date type.
TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidBroadcastEvent) {
  EntityPtr publication = Entity::New();
  publication->type = schema_org::entity::kBroadcastEvent;
  publication->properties.push_back(
      CreateDateTimeProperty(schema_org::property::kStartDate, "2020-03-22"));
  publication->properties.push_back(
      CreateStringProperty(schema_org::property::kEndDate, "2020-03-23"));
  publication->properties.push_back(CreateEntityProperty(
      schema_org::property::kWorkPerformed, ValidMediaFeedItem()));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(publication));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::entity::kBroadcastEvent));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithIdentifier) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr identifier = Entity::New();
    identifier->type = schema_org::entity::kPropertyValue;
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kPropertyID, "TMS_ROOT_ID"));
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kValue, "1"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kIdentifier, std::move(identifier)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  mojom::IdentifierPtr identifier = mojom::Identifier::New();
  identifier->type = mojom::Identifier::Type::kTMSRootId;
  identifier->value = "1";
  expected_item->identifiers.push_back(std::move(identifier));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidIdentifier) {
  EntityPtr item = ValidMediaFeedItem();

  {
    EntityPtr identifier = Entity::New();
    identifier->type = schema_org::entity::kPropertyValue;
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kPropertyID, "Unknown"));
    identifier->properties.push_back(
        CreateStringProperty(schema_org::property::kValue, "1"));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kIdentifier, std::move(identifier)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kIdentifier));
}

TEST_F(MediaFeedsConverterTest, SucceedsTVSeriesWithNoEpisode) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Successfully converts a TV episode with embedded watch action and optional
// identifiers.
TEST_F(MediaFeedsConverterTest, SucceedsItemWithTVEpisode) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  // Ignore the item's action field by changing the name. Use the action
  // embedded in the TV episode instead.
  GetProperty(item.get(), schema_org::property::kPotentialAction)->name =
      "not an action";

  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kEpisode,
                           ValidEpisode(1, ValidPotentialWatchAction())));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  expected_item->tv_episode = mojom::TVEpisode::New();
  expected_item->tv_episode->episode_number = 1;
  expected_item->tv_episode->name = "Pilot";
  expected_item->tv_episode->duration = base::TimeDelta::FromHours(1);
  expected_item->duration = base::nullopt;
  mojom::IdentifierPtr identifier = mojom::Identifier::New();
  identifier->type = mojom::Identifier::Type::kTMSRootId;
  identifier->value = "1";
  expected_item->tv_episode->identifiers.push_back(std::move(identifier));
  expected_item->action_status = mojom::MediaFeedItemActionStatus::kPotential;
  expected_item->action->start_time = base::nullopt;
  expected_item->action->url = GURL("https://www.example.com");

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

// Successfully converts a TV episode with embedded images.
TEST_F(MediaFeedsConverterTest, SucceedsItemWithTVEpisodeWithImage) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kEpisode,
      WithImage(ValidEpisode(1, ValidPotentialWatchAction()))));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]->tv_episode);
  EXPECT_EQ(1u, result[0]->tv_episode->images.size());
  EXPECT_EQ(ExpectedMediaImage(), result[0]->tv_episode->images[0]);
}

// Fails because TV episode is present, but TV episode name is empty.
TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidTVEpisode) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  EntityPtr episode = Entity::New();
  episode->type = schema_org::entity::kTVEpisode;
  episode->properties.push_back(
      CreateLongProperty(schema_org::property::kEpisodeNumber, 1));
  episode->properties.push_back(
      CreateStringProperty(schema_org::property::kName, ""));
  episode->properties.push_back(CreateEntityProperty(
      schema_org::property::kPotentialAction, ValidActiveWatchAction()));
  item->properties.push_back(
      CreateEntityProperty(schema_org::property::kEpisode, std::move(episode)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs,
              testing::HasSubstr(schema_org::property::kEpisode));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithTVSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 1));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContainsSeason, std::move(season)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, FailsItemWithInvalidTVSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
  item->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfSeasons, 6));

  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 1));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, -1));
    item->properties.push_back(CreateEntityProperty(
        schema_org::property::kContainsSeason, std::move(season)));
  }

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResult(std::move(entity));

  EXPECT_THAT(result.error_logs, testing::HasSubstr("season"));
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithPlayNextTwoSeasons) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  PropertyPtr property = Property::New();
  property->name = schema_org::property::kContainsSeason;
  property->values = Values::New();

  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 1));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
    season->properties.push_back(
        CreateEntityProperty(schema_org::property::kEpisode,
                             ValidEpisode(20, ValidActiveWatchAction())));
    property->values->entity_values.push_back(std::move(season));
  }
  {
    EntityPtr season = Entity::New();
    season->type = schema_org::entity::kTVSeason;
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kSeasonNumber, 2));
    season->properties.push_back(
        CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));
    season->properties.push_back(
        CreateEntityProperty(schema_org::property::kEpisode,
                             ValidEpisode(1, ValidPotentialWatchAction())));
    property->values->entity_values.push_back(std::move(season));
  }
  item->properties.push_back(std::move(property));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  {
    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->episode_number = 20;
    expected_item->tv_episode->season_number = 1;
    expected_item->tv_episode->name = "Pilot";
    expected_item->tv_episode->duration = base::TimeDelta::FromHours(1);
    expected_item->duration = base::nullopt;
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->tv_episode->identifiers.push_back(std::move(identifier));
  }
  {
    expected_item->play_next_candidate = mojom::PlayNextCandidate::New();
    expected_item->play_next_candidate->episode_number = 1;
    expected_item->play_next_candidate->season_number = 2;
    expected_item->play_next_candidate->name = "Pilot";
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->play_next_candidate->identifiers.push_back(
        std::move(identifier));
    expected_item->play_next_candidate->duration =
        base::TimeDelta::FromHours(1);
    expected_item->play_next_candidate->action = mojom::Action::New();
    expected_item->play_next_candidate->action->url =
        GURL("https://www.example.com");
  }

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithPlayNextSameSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  EntityPtr season = Entity::New();
  season->type = schema_org::entity::kTVSeason;
  season->properties.push_back(
      CreateLongProperty(schema_org::property::kSeasonNumber, 1));
  season->properties.push_back(
      CreateLongProperty(schema_org::property::kNumberOfEpisodes, 20));

  PropertyPtr property = Property::New();
  property->name = schema_org::property::kEpisode;
  property->values = Values::New();
  property->values->entity_values.push_back(
      ValidEpisode(15, ValidActiveWatchAction()));
  property->values->entity_values.push_back(
      ValidEpisode(16, ValidPotentialWatchAction()));
  season->properties.push_back(std::move(property));

  item->properties.push_back(CreateEntityProperty(
      schema_org::property::kContainsSeason, std::move(season)));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  {
    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->episode_number = 15;
    expected_item->tv_episode->season_number = 1;
    expected_item->tv_episode->name = "Pilot";
    expected_item->tv_episode->duration = base::TimeDelta::FromHours(1);
    expected_item->duration = base::nullopt;
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->tv_episode->identifiers.push_back(std::move(identifier));
  }
  {
    expected_item->play_next_candidate = mojom::PlayNextCandidate::New();
    expected_item->play_next_candidate->episode_number = 16;
    expected_item->play_next_candidate->season_number = 1;
    expected_item->play_next_candidate->name = "Pilot";
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->play_next_candidate->identifiers.push_back(
        std::move(identifier));
    expected_item->play_next_candidate->duration =
        base::TimeDelta::FromHours(1);
    expected_item->play_next_candidate->action = mojom::Action::New();
    expected_item->play_next_candidate->action->url =
        GURL("https://www.example.com");
  }

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->tv_episode, expected_item->tv_episode);
  EXPECT_EQ(result[0]->play_next_candidate, expected_item->play_next_candidate);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithPlayNextNoSeason) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  PropertyPtr property = Property::New();
  property->name = schema_org::property::kEpisode;
  property->values = Values::New();
  property->values->entity_values.push_back(
      ValidEpisode(15, ValidActiveWatchAction()));
  property->values->entity_values.push_back(
      ValidEpisode(16, ValidPotentialWatchAction()));
  item->properties.push_back(std::move(property));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  mojom::MediaFeedItemPtr expected_item = ExpectedFeedItem();
  expected_item->type = mojom::MediaFeedItemType::kTVSeries;
  {
    expected_item->tv_episode = mojom::TVEpisode::New();
    expected_item->tv_episode->episode_number = 15;
    expected_item->tv_episode->season_number = 0;
    expected_item->tv_episode->name = "Pilot";
    expected_item->tv_episode->duration = base::TimeDelta::FromHours(1);
    expected_item->duration = base::nullopt;
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->tv_episode->identifiers.push_back(std::move(identifier));
  }
  {
    expected_item->play_next_candidate = mojom::PlayNextCandidate::New();
    expected_item->play_next_candidate->episode_number = 16;
    expected_item->play_next_candidate->season_number = 0;
    expected_item->play_next_candidate->name = "Pilot";
    mojom::IdentifierPtr identifier = mojom::Identifier::New();
    identifier->type = mojom::Identifier::Type::kTMSRootId;
    identifier->value = "1";
    expected_item->play_next_candidate->identifiers.push_back(
        std::move(identifier));
    expected_item->play_next_candidate->duration =
        base::TimeDelta::FromHours(1);
    expected_item->play_next_candidate->action = mojom::Action::New();
    expected_item->play_next_candidate->action->url =
        GURL("https://www.example.com");
  }

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->tv_episode, expected_item->tv_episode);
  EXPECT_EQ(result[0]->play_next_candidate, expected_item->play_next_candidate);
  EXPECT_EQ(expected_item, result[0]);
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithPlayNextAndEpisodeImages) {
  EntityPtr item = ValidMediaFeedItem();
  item->type = schema_org::entity::kTVSeries;

  PropertyPtr property = Property::New();
  property->name = schema_org::property::kEpisode;
  property->values = Values::New();
  property->values->entity_values.push_back(
      WithImage(ValidEpisode(15, ValidActiveWatchAction())));
  property->values->entity_values.push_back(
      WithImage(ValidEpisode(16, ValidPotentialWatchAction())));
  item->properties.push_back(std::move(property));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]->tv_episode);
  ASSERT_TRUE(result[0]->play_next_candidate);
  ASSERT_EQ(1u, result[0]->tv_episode->images.size());
  ASSERT_EQ(1u, result[0]->play_next_candidate->images.size());
  EXPECT_EQ(ExpectedMediaImage(), result[0]->tv_episode->images[0]);
  EXPECT_EQ(ExpectedMediaImage(), result[0]->play_next_candidate->images[0]);
}

TEST_F(MediaFeedsConverterTest, SucceedsItemWithImageObject) {
  EntityPtr item = ValidMediaFeedItem();

  auto* image = GetProperty(item.get(), schema_org::property::kImage);
  image->values->url_values = {};
  image->values->entity_values.push_back(ValidMediaImage());

  auto expected = ExpectedFeedItem();
  expected->images.clear();
  expected->images.push_back(ExpectedMediaImage());

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected, result[0]);
}

TEST_F(MediaFeedsConverterTest,
       SucceedsItemWithImageObjectAndContentAttributes) {
  EntityPtr item = ValidMediaFeedItem();

  auto* image = GetProperty(item.get(), schema_org::property::kImage);
  image->values->url_values = {};
  image->values->entity_values.push_back(
      WithContentAttributes(ValidMediaImage()));

  auto expected = ExpectedFeedItem();
  expected->images.clear();
  auto expected_image = ExpectedMediaImage();
  expected_image->content_attributes = {mojom::ContentAttribute::kHasTitle,
                                        mojom::ContentAttribute::kIconic,
                                        mojom::ContentAttribute::kPoster};
  expected->images.push_back(std::move(expected_image));

  EntityPtr entity = AddItemToFeed(ValidMediaFeed(), std::move(item));

  auto result = GetResultItems(std::move(entity));

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(expected, result[0]);
}

}  // namespace media_feeds
