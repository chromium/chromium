// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/feeds/media_feeds_converter.h"

#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-shared.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/autofill/core/browser/validation.h"
#include "components/schema_org/common/improved_metadata.mojom-forward.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "components/schema_org/schema_org_enums.h"
#include "components/schema_org/schema_org_property_names.h"
#include "url/url_constants.h"

namespace media_feeds {

using schema_org::improved::mojom::Entity;
using schema_org::improved::mojom::EntityPtr;
using schema_org::improved::mojom::Property;
using schema_org::improved::mojom::PropertyPtr;
using schema_org::improved::mojom::ValuesPtr;

static int constexpr kMaxRatings = 5;
static int constexpr kMaxGenres = 3;
static int constexpr kMaxInteractionStatistics = 3;
static int constexpr kMaxImages = 5;

// Gets the property of entity with corresponding name. May be null if not found
// or if the property has no values.
Property* MediaFeedsConverter::GetProperty(Entity* entity,
                                           const std::string& name) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  if (property == entity->properties.end())
    return nullptr;
  if (!(*property)->values)
    return nullptr;
  return property->get();
}

// Converts a property named property_name and store the result in the
// converted_item struct using the convert_property callback. Returns true only
// is the conversion was successful. If is_required is set, the property must be
// found and be valid. If is_required is not set, returns false only if the
// property is found and is invalid.
template <typename T>
bool MediaFeedsConverter::ConvertProperty(
    Entity* entity,
    T* converted_item,
    const std::string& property_name,
    bool is_required,
    base::OnceCallback<bool(const Property& property, T*)> convert_property) {
  auto* property = GetProperty(entity, property_name);
  if (!property) {
    if (is_required)
      Log("Missing " + property_name + ".");

    return !is_required;
  }

  bool was_converted =
      std::move(convert_property).Run(*property, converted_item);

  if (!was_converted)
    Log("Invalid " + property_name + ".");

  return was_converted;
}

// Validates a property identified by name using the provided callback. Returns
// true only if the property is valid.
bool MediaFeedsConverter::ValidateProperty(
    Entity* entity,
    const std::string& name,
    base::OnceCallback<bool(const Property& property)> property_is_valid) {
  auto property = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [&name](const PropertyPtr& property) { return property->name == name; });
  if (property == entity->properties.end())
    return false;
  if (!(*property)->values)
    return false;
  return std::move(property_is_valid).Run(**property);
}

// Checks that the property contains at least one URL and that all URLs it
// contains are valid.
bool MediaFeedsConverter::IsUrl(const Property& property) {
  return !property.values->url_values.empty() &&
         std::accumulate(property.values->url_values.begin(),
                         property.values->url_values.end(), true,
                         [](auto& accumulation, auto& url_value) {
                           return accumulation && url_value.is_valid();
                         });
}

// Checks that the property is a positive integer.
bool MediaFeedsConverter::IsPositiveInteger(const Property& property) {
  if (!property.values->long_values.empty())
    return property.values->long_values[0] > 0;
  if (!property.values->double_values.empty())
    return property.values->double_values[0] > 0;
  return false;
}

// Checks that the property contains at least one non-empty string and that all
// strings it contains are non-empty.
bool MediaFeedsConverter::IsNonEmptyString(const Property& property) {
  return (!property.values->string_values.empty() &&
          std::accumulate(property.values->string_values.begin(),
                          property.values->string_values.end(), true,
                          [](auto& accumulation, auto& string_value) {
                            return accumulation && !string_value.empty();
                          }));
}

// Returns the non-empty string in the first position. If there is more than
// one string value or it is empty then returns nullopt.
base::Optional<std::string> MediaFeedsConverter::GetFirstNonEmptyString(
    const Property& prop) {
  if (prop.values->string_values.size() != 1)
    return base::nullopt;

  auto& string = prop.values->string_values[0];
  if (string.empty())
    return base::nullopt;

  return string;
}

// Checks that the property contains at least one valid email address.
bool MediaFeedsConverter::IsEmail(const Property& email) {
  if (email.values->string_values.empty())
    return false;

  return autofill::IsValidEmailAddress(
      base::ASCIIToUTF16(email.values->string_values[0]));
}

// Checks whether the media item type is supported.
bool MediaFeedsConverter::IsMediaItemType(const std::string& type) {
  static const base::NoDestructor<base::flat_set<base::StringPiece>>
      kSupportedTypes(base::flat_set<base::StringPiece>(
          {schema_org::entity::kVideoObject, schema_org::entity::kMovie,
           schema_org::entity::kTVSeries}));
  return kSupportedTypes->find(type) != kSupportedTypes->end();
}

// Checks that the property contains at least one valid date / date-time.
bool MediaFeedsConverter::IsDateOrDateTime(const Property& property) {
  return !property.values->date_time_values.empty();
}

// Gets a positive integer from the property which may be stored as a long or
// double.
base::Optional<uint64_t> MediaFeedsConverter::GetPositiveIntegerFromProperty(
    Entity* entity,
    const std::string& property_name) {
  auto* property = GetProperty(entity, property_name);
  if (!property)
    return base::nullopt;

  if (!property->values->long_values.empty() &&
      property->values->long_values[0] > 0) {
    return property->values->long_values[0];
  }

  if (!property->values->double_values.empty() &&
      property->values->double_values[0] > 0) {
    return lround(property->values->double_values[0]);
  }

  return base::nullopt;
}

// Gets the duration from the property and store the result in item. Returns
// true if the duration was valid.
template <typename T>
bool MediaFeedsConverter::GetDuration(const Property& property, T* item) {
  if (property.values->time_values.empty())
    return false;

  item->duration = property.values->time_values[0];
  return true;
}

// Converts a string to a mojo ContentAttribute.
base::Optional<mojom::ContentAttribute>
MediaFeedsConverter::GetContentAttribute(const std::string& value) {
  if (value == "iconic")
    return mojom::ContentAttribute::kIconic;

  if (value == "sceneStill")
    return mojom::ContentAttribute::kSceneStill;

  if (value == "poster")
    return mojom::ContentAttribute::kPoster;

  if (value == "background")
    return mojom::ContentAttribute::kBackground;

  if (value == "forDarkBackground")
    return mojom::ContentAttribute::kForDarkBackground;

  if (value == "forLightBackground")
    return mojom::ContentAttribute::kForLightBackground;

  if (value == "hasTitle")
    return mojom::ContentAttribute::kHasTitle;

  if (value == "noTitle")
    return mojom::ContentAttribute::kNoTitle;

  if (value == "transparentBackground")
    return mojom::ContentAttribute::kHasTransparentBackground;

  if (value == "centered")
    return mojom::ContentAttribute::kCentered;

  if (value == "rightCentered")
    return mojom::ContentAttribute::kRightCentered;

  if (value == "leftCentered")
    return mojom::ContentAttribute::kLeftCentered;

  return base::nullopt;
}

// Gets the content attributes from the image. This should be a list of
// string attributes in the "additionalProperty" property. This property is
// optional according to the spec. Returns nullopt if the property is invalid,
// empty vector if the property is not present or present but empty.
base::Optional<std::vector<mojom::ContentAttribute>>
MediaFeedsConverter::GetContentAttributes(const EntityPtr& image) {
  auto* property =
      GetProperty(image.get(), schema_org::property::kAdditionalProperty);

  if (!property || property->values->entity_values.empty())
    return std::vector<mojom::ContentAttribute>();

  std::vector<mojom::ContentAttribute> content_attributes;
  const auto& attribute = property->values->entity_values[0];

  if (attribute->type != schema_org::entity::kPropertyValue)
    return base::nullopt;

  auto* name = GetProperty(attribute.get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name) ||
      name->values->string_values[0] != "contentAttributes") {
    return base::nullopt;
  }

  auto* values = GetProperty(attribute.get(), schema_org::property::kValue);
  if (!values || !IsNonEmptyString(*values))
    return base::nullopt;

  for (const auto& value : values->values->string_values) {
    auto attr = GetContentAttribute(value);
    if (attr.has_value())
      content_attributes.push_back(attr.value());
  }

  return content_attributes;
}

// Gets a list of media images from the property. The property should have at
// least one media image and no more than kMaxImages. A media image is either a
// valid URL string or an ImageObject entity containing a width, height, and
// URL.
base::Optional<std::vector<mojom::MediaImagePtr>>
MediaFeedsConverter::GetMediaImage(const Property& property) {
  if (property.values->url_values.empty() &&
      property.values->entity_values.empty()) {
    return base::nullopt;
  }

  std::vector<mojom::MediaImagePtr> images;

  for (const auto& url : property.values->url_values) {
    if (!url.is_valid())
      continue;

    auto image = mojom::MediaImage::New();
    image->src = url;

    images.push_back(std::move(image));
    if (images.size() == kMaxImages)
      return images;
  }

  for (const auto& image_object : property.values->entity_values) {
    if (image_object->type != schema_org::entity::kImageObject)
      continue;

    auto image = mojom::MediaImage::New();

    auto* width = GetProperty(image_object.get(), schema_org::property::kWidth);
    if (!width || !IsPositiveInteger(*width))
      continue;

    auto* height =
        GetProperty(image_object.get(), schema_org::property::kHeight);
    if (!height || !IsPositiveInteger(*height))
      continue;

    image->size = gfx::Size(width->values->long_values[0],
                            height->values->long_values[0]);

    auto* url = GetProperty(image_object.get(), schema_org::property::kUrl);
    if (!url)
      url = GetProperty(image_object.get(), schema_org::property::kEmbedUrl);
    if (!IsUrl(*url))
      continue;

    image->src = url->values->url_values[0];

    auto content_attributes = GetContentAttributes(image_object);
    if (content_attributes && !content_attributes.value().empty()) {
      image->content_attributes = std::move(content_attributes.value());
    }

    images.push_back(std::move(image));
    if (images.size() == kMaxImages)
      return images;
  }
  return images;
}

// Returns true if the image has at least one of the provided attributes.
bool MediaFeedsConverter::ImageHasOneOfAttributes(
    const mojom::MediaImagePtr& image,
    std::vector<mojom::ContentAttribute> attributes) {
  for (auto attribute : attributes) {
    if (base::Contains(image->content_attributes, attribute))
      return true;
  }
  return false;
}

// Gets logos from a property. Logos have additional requirements beyond those
// for media images.
base::Optional<std::vector<mojom::MediaImagePtr>>
MediaFeedsConverter::GetLogoImages(const Property& property) {
  auto images = GetMediaImage(property);
  if (!images.has_value())
    return base::nullopt;

  std::vector<mojom::MediaImagePtr> logos;
  for (auto& image : images.value()) {
    if (ImageHasOneOfAttributes(image, {mojom::ContentAttribute::kHasTitle,
                                        mojom::ContentAttribute::kNoTitle}) &&
        ImageHasOneOfAttributes(
            image, {mojom::ContentAttribute::kForDarkBackground,
                    mojom::ContentAttribute::kForLightBackground})) {
      logos.push_back(std::move(image));
    }
  }
  return logos;
}

bool MediaFeedsConverter::GetCurrentlyLoggedInUser(
    const Property& member,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result) {
  if (member.values->entity_values.empty())
    return false;

  auto user_identifier = mojom::UserIdentifier::New();

  auto& person = member.values->entity_values[0];
  if (person->type != schema_org::entity::kPerson)
    return false;

  auto* name = GetProperty(person.get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;

  user_identifier->name = name->values->string_values[0];

  auto* image = GetProperty(person.get(), schema_org::property::kImage);
  if (image) {
    auto maybe_images = GetMediaImage(*image);
    if (!maybe_images.has_value() || maybe_images.value().empty())
      return false;

    user_identifier->image = std::move(maybe_images.value()[0]);
  }

  auto* email = GetProperty(person.get(), schema_org::property::kEmail);
  if (email) {
    if (!IsEmail(*email))
      return false;

    user_identifier->email = email->values->string_values[0];
  }

  result->user_identifier = std::move(user_identifier);

  return true;
}

// Validates the provider property of an entity. Outputs the name and images
// properties.
bool MediaFeedsConverter::ValidateProvider(
    const Property& provider,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result) {
  if (provider.values->entity_values.empty())
    return false;

  auto& organization = provider.values->entity_values[0];

  if (organization->type != schema_org::entity::kOrganization)
    return false;

  auto* name = GetProperty(organization.get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;

  auto* logo = GetProperty(organization.get(), schema_org::property::kLogo);
  if (!logo)
    return false;

  auto maybe_images = GetLogoImages(*logo);
  if (!maybe_images.has_value() || maybe_images.value().empty())
    return false;

  if (!ConvertProperty(
          organization.get(), result, schema_org::property::kMember,
          /*is_required=*/false,
          base::BindOnce(&MediaFeedsConverter::GetCurrentlyLoggedInUser,
                         base::Unretained(this)))) {
    return false;
  }

  result->display_name = name->values->string_values[0];
  result->logos = std::move(maybe_images.value());

  return true;
}

// Gets the feed additional properties.
bool MediaFeedsConverter::GetFeedAdditionalProperties(
    const Property& property,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result) {
  if (property.values->entity_values.empty())
    return false;

  for (const auto& entity : property.values->entity_values) {
    if (!entity || entity->type != schema_org::entity::kPropertyValue)
      continue;

    auto* name_prop = GetProperty(entity.get(), schema_org::property::kName);
    if (!name_prop)
      continue;

    auto name = GetFirstNonEmptyString(*name_prop);
    if (!name)
      continue;

    // If the property is "cookieNameFilter" then the value should be a single
    // non-empty string.
    if (name == "cookieNameFilter") {
      auto* value = GetProperty(entity.get(), schema_org::property::kValue);
      if (!value)
        continue;

      result->cookie_name_filter =
          GetFirstNonEmptyString(*value).value_or(std::string());
    }
  }

  return true;
}

// Gets the author property and stores the result in item. Returns true if the
// author was valid.
bool MediaFeedsConverter::GetMediaItemAuthor(const Property& author,
                                             mojom::MediaFeedItem* item) {
  item->author = mojom::Author::New();

  if (IsNonEmptyString(author)) {
    item->author->name = author.values->string_values[0];
    return true;
  }

  if (author.values->entity_values.empty())
    return false;

  auto person = std::find_if(
      author.values->entity_values.begin(), author.values->entity_values.end(),
      [](const EntityPtr& value) {
        return value->type == schema_org::entity::kPerson;
      });

  auto* name = GetProperty(person->get(), schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  item->author->name = name->values->string_values[0];

  auto* url = GetProperty(person->get(), schema_org::property::kUrl);
  if (url) {
    if (!IsUrl(*url))
      return false;
    item->author->url = url->values->url_values[0];
  }

  return true;
}

// Gets the ratings property and stores the result in item. Returns true if the
// ratings were valid.
bool MediaFeedsConverter::GetContentRatings(const Property& property,
                                            mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty() ||
      property.values->entity_values.size() > kMaxRatings)
    return false;

  for (const auto& rating : property.values->entity_values) {
    mojom::ContentRatingPtr converted_rating = mojom::ContentRating::New();

    if (rating->type != schema_org::entity::kRating)
      return false;

    auto* author = GetProperty(rating.get(), schema_org::property::kAuthor);
    if (!author || !IsNonEmptyString(*author))
      return false;

    static const base::NoDestructor<base::flat_set<base::StringPiece>>
        kRatingAgencies(base::flat_set<base::StringPiece>(
            {"TVPG", "MPAA", "BBFC", "CSA", "AGCOM", "FSK", "SETSI", "ICAA",
             "NA", "EIRIN", "KMRB", "CLASSIND", "MKRF", "CBFC", "KPI", "LSF",
             "RTC"}));
    if (!kRatingAgencies->contains(author->values->string_values[0]))
      return false;
    converted_rating->agency = author->values->string_values[0];

    auto* rating_value =
        GetProperty(rating.get(), schema_org::property::kRatingValue);
    if (!rating_value || !IsNonEmptyString(*rating_value))
      return false;
    converted_rating->value = rating_value->values->string_values[0];

    item->content_ratings.push_back(std::move(converted_rating));
  }
  return true;
}

// Gets the identifiers property and stores the result in item. Item should be a
// struct with an identifiers field. Returns true if the identifiers were valid.
template <typename T>
bool MediaFeedsConverter::GetIdentifiers(const Property& property, T* item) {
  if (property.values->entity_values.empty())
    return false;

  std::vector<mojom::IdentifierPtr> identifiers;

  for (const auto& identifier : property.values->entity_values) {
    mojom::IdentifierPtr converted_identifier = mojom::Identifier::New();

    if (identifier->type != schema_org::entity::kPropertyValue)
      return false;

    auto* property_id =
        GetProperty(identifier.get(), schema_org::property::kPropertyID);
    if (!property_id || !IsNonEmptyString(*property_id))
      return false;
    std::string property_id_str = property_id->values->string_values[0];
    if (property_id_str == "TMS_ROOT_ID") {
      converted_identifier->type = mojom::Identifier::Type::kTMSRootId;
    } else if (property_id_str == "TMS_ID") {
      converted_identifier->type = mojom::Identifier::Type::kTMSId;
    } else if (property_id_str == "_PARTNER_ID_") {
      converted_identifier->type = mojom::Identifier::Type::kPartnerId;
    } else {
      return false;
    }

    auto* value = GetProperty(identifier.get(), schema_org::property::kValue);
    if (!value)
      return false;

    // The value must be a type we can unambiguously store as string.
    if (!value->values->string_values.empty()) {
      converted_identifier->value = value->values->string_values[0];
    } else if (!value->values->long_values.empty()) {
      converted_identifier->value =
          base::NumberToString(value->values->long_values[0]);
    } else {
      return false;
    }

    item->identifiers.push_back(std::move(converted_identifier));
  }
  return true;
}

// Gets the interaction type from a property containing an interaction type
// string.
base::Optional<mojom::InteractionCounterType>
MediaFeedsConverter::GetInteractionType(const Property& property) {
  if (property.values->url_values.empty())
    return base::nullopt;
  GURL type = property.values->url_values[0];
  if (!type.SchemeIsHTTPOrHTTPS() || type.host() != "schema.org")
    return base::nullopt;

  std::string type_path = type.path().substr(1);
  if (type_path == schema_org::entity::kWatchAction) {
    return mojom::InteractionCounterType::kWatch;
  } else if (type_path == schema_org::entity::kLikeAction) {
    return mojom::InteractionCounterType::kLike;
  } else if (type_path == schema_org::entity::kDislikeAction) {
    return mojom::InteractionCounterType::kDislike;
  }
  return base::nullopt;
}

// Gets the interaction statistics property and stores the result in item.
// Returns true if the statistics were valid.
bool MediaFeedsConverter::GetInteractionStatistics(const Property& property,
                                                   mojom::MediaFeedItem* item) {
  if (property.values->entity_values.empty() ||
      property.values->entity_values.size() > kMaxInteractionStatistics) {
    return false;
  }
  for (const auto& stat : property.values->entity_values) {
    if (stat->type != schema_org::entity::kInteractionCounter)
      return false;

    auto* interaction_type =
        GetProperty(stat.get(), schema_org::property::kInteractionType);
    if (!interaction_type)
      return false;
    auto type = GetInteractionType(*interaction_type);
    if (!type.has_value() || item->interaction_counters.count(type.value()) > 0)
      return false;

    base::Optional<uint64_t> count = GetPositiveIntegerFromProperty(
        stat.get(), schema_org::property::kUserInteractionCount);
    if (!count.has_value())
      return false;
    item->interaction_counters.insert(
        std::pair<mojom::InteractionCounterType, uint64_t>(type.value(),
                                                           count.value()));
  }
  if (item->interaction_counters.empty())
    return false;

  return true;
}

base::Optional<mojom::MediaFeedItemType> MediaFeedsConverter::GetMediaItemType(
    const std::string& schema_org_type) {
  if (schema_org_type == schema_org::entity::kVideoObject) {
    return mojom::MediaFeedItemType::kVideo;
  } else if (schema_org_type == schema_org::entity::kMovie) {
    return mojom::MediaFeedItemType::kMovie;
  } else if (schema_org_type == schema_org::entity::kTVSeries) {
    return mojom::MediaFeedItemType::kTVSeries;
  }
  return base::nullopt;
}

// Gets the isFamilyFriendly property and stores the result in item.
bool MediaFeedsConverter::GetIsFamilyFriendly(const Property& property,
                                              mojom::MediaFeedItem* item) {
  if (property.values->bool_values.empty()) {
    return false;
  }

  item->is_family_friendly = property.values->bool_values[0]
                                 ? media_feeds::mojom::IsFamilyFriendly::kYes
                                 : media_feeds::mojom::IsFamilyFriendly::kNo;
  return true;
}

// Gets the action status from embedded in the action property of the entity.
base::Optional<mojom::MediaFeedItemActionStatus>
MediaFeedsConverter::GetActionStatus(Entity* entity) {
  auto* action = GetProperty(entity, schema_org::property::kPotentialAction);
  if (!action || action->values->entity_values.empty())
    return base::nullopt;

  auto* action_status = GetProperty(action->values->entity_values[0].get(),
                                    schema_org::property::kActionStatus);
  if (!action_status)
    return base::nullopt;
  if (!IsUrl(*action_status))
    return base::nullopt;

  auto status = schema_org::enums::CheckValidEnumString(
      "http://schema.org/ActionStatusType",
      action_status->values->url_values[0]);
  switch (status.value()) {
    case static_cast<int>(
        schema_org::enums::ActionStatusType::kActiveActionStatus):
      return mojom::MediaFeedItemActionStatus::kActive;
    case static_cast<int>(
        schema_org::enums::ActionStatusType::kPotentialActionStatus):

      return mojom::MediaFeedItemActionStatus::kPotential;
    case static_cast<int>(
        schema_org::enums::ActionStatusType::kCompletedActionStatus):
      return mojom::MediaFeedItemActionStatus::kCompleted;
  }

  return base::nullopt;
}

// Gets the watchAction properties from an embedded entity and stores the result
// in item. Returns true if the action was valid.
template <typename T>
bool MediaFeedsConverter::GetAction(
    mojom::MediaFeedItemActionStatus action_status,
    const Property& property,
    T* item) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& action = property.values->entity_values[0];
  if (action->type != schema_org::entity::kWatchAction)
    return false;

  item->action = mojom::Action::New();

  auto* target = GetProperty(action.get(), schema_org::property::kTarget);
  if (!target || !IsUrl(*target))
    return false;
  item->action->url = target->values->url_values[0];

  if (action_status == mojom::MediaFeedItemActionStatus::kActive) {
    auto* start_time =
        GetProperty(action.get(), schema_org::property::kStartTime);
    if (!start_time || start_time->values->time_values.empty())
      return false;
    item->action->start_time = start_time->values->time_values[0];
  }

  return true;
}

// Gets the TV episode stored in an embedded entity and stores the result in
// item. Returns true if the TV episode was valid.
bool MediaFeedsConverter::GetEpisode(
    const MediaFeedsConverter::EpisodeCandidate& candidate,
    mojom::MediaFeedItem* item) {
  if (!item->tv_episode)
    item->tv_episode = mojom::TVEpisode::New();

  item->tv_episode->episode_number = candidate.episode_number;
  item->tv_episode->season_number = candidate.season_number;
  item->action_status = candidate.action_status;

  auto* name = GetProperty(candidate.entity, schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  item->tv_episode->name = name->values->string_values[0];

  if (!ConvertProperty<mojom::TVEpisode>(
          candidate.entity, item->tv_episode.get(),
          schema_org::property::kIdentifier, false,
          base::BindOnce(&MediaFeedsConverter::GetIdentifiers<mojom::TVEpisode>,
                         base::Unretained(this)))) {
    return false;
  }

  auto* image = GetProperty(candidate.entity, schema_org::property::kImage);
  if (image) {
    auto converted_images = GetMediaImage(*image);
    if (!converted_images.has_value())
      return false;

    item->tv_episode->images = std::move(converted_images.value());
  }

  if (!ConvertProperty<mojom::MediaFeedItem>(
          candidate.entity, item, schema_org::property::kPotentialAction, true,
          base::BindOnce(&MediaFeedsConverter::GetAction<mojom::MediaFeedItem>,
                         base::Unretained(this), candidate.action_status))) {
    return false;
  }

  if (!ConvertProperty<mojom::TVEpisode>(
          candidate.entity, item->tv_episode.get(),
          schema_org::property::kDuration, true,
          base::BindOnce(&MediaFeedsConverter::GetDuration<mojom::TVEpisode>,
                         base::Unretained(this)))) {
    return false;
  }

  return true;
}

// Gets the PlayNextCandidate stored in an embedded entity and stores the result
// in item. Returns true if the PlayNextCandidate was valid. See the spec for
// this feature: https://wicg.github.io/media-feeds/#play-next-tv-episodes
bool MediaFeedsConverter::GetPlayNextCandidate(
    const MediaFeedsConverter::EpisodeCandidate& candidate,
    mojom::MediaFeedItem* item) {
  if (!item->play_next_candidate)
    item->play_next_candidate = mojom::PlayNextCandidate::New();

  item->play_next_candidate->episode_number = candidate.episode_number;
  item->play_next_candidate->season_number = candidate.season_number;

  auto* name = GetProperty(candidate.entity, schema_org::property::kName);
  if (!name || !IsNonEmptyString(*name))
    return false;
  item->play_next_candidate->name = name->values->string_values[0];

  if (!ConvertProperty<mojom::PlayNextCandidate>(
          candidate.entity, item->play_next_candidate.get(),
          schema_org::property::kIdentifier, false,
          base::BindOnce(
              &MediaFeedsConverter::GetIdentifiers<mojom::PlayNextCandidate>,
              base::Unretained(this)))) {
    return false;
  }

  auto* image = GetProperty(candidate.entity, schema_org::property::kImage);
  if (image) {
    auto converted_images = GetMediaImage(*image);
    if (!converted_images.has_value())
      return false;

    item->play_next_candidate->images = std::move(converted_images.value());
  }

  if (!ConvertProperty<mojom::PlayNextCandidate>(
          candidate.entity, item->play_next_candidate.get(),
          schema_org::property::kPotentialAction, true,
          base::BindOnce(
              &MediaFeedsConverter::GetAction<mojom::PlayNextCandidate>,
              base::Unretained(this), candidate.action_status))) {
    return false;
  }

  if (!ConvertProperty<mojom::PlayNextCandidate>(
          candidate.entity, item->play_next_candidate.get(),
          schema_org::property::kDuration, true,
          base::BindOnce(
              &MediaFeedsConverter::GetDuration<mojom::PlayNextCandidate>,
              base::Unretained(this)))) {
    return false;
  }

  return true;
}

// Converts the entity to an EpisodeCandidate.
base::Optional<MediaFeedsConverter::EpisodeCandidate>
MediaFeedsConverter::GetEpisodeCandidate(const EntityPtr& entity) {
  if (entity->type != schema_org::entity::kTVEpisode)
    return base::nullopt;

  EpisodeCandidate candidate;
  candidate.entity = entity.get();

  auto action_status = GetActionStatus(entity.get());
  candidate.action_status =
      action_status.value_or(mojom::MediaFeedItemActionStatus::kUnknown);

  auto episode_number = GetPositiveIntegerFromProperty(
      entity.get(), schema_org::property::kEpisodeNumber);
  if (!episode_number.has_value())
    return base::nullopt;
  candidate.episode_number = episode_number.value();

  return candidate;
}

// Converts all the entity values in the property to episode candidates. Returns
// base::nullopt if any of the entities are not valid episode candidates.
base::Optional<std::vector<MediaFeedsConverter::EpisodeCandidate>>
MediaFeedsConverter::GetEpisodeCandidatesFromProperty(Property* property,
                                                      int season_number) {
  std::vector<EpisodeCandidate> episodes;
  if (!property) {
    return episodes;
  }
  for (const EntityPtr& episode : property->values->entity_values) {
    auto candidate = GetEpisodeCandidate(episode);
    if (!candidate.has_value())
      return base::nullopt;
    candidate.value().season_number = season_number;
    episodes.push_back(std::move(candidate.value()));
  }
  return episodes;
}

// Gets a list of EpisodeCandidates from the entity. These can be embedded
// either in the season or episode properties. Returns base::nullopt if any
// candidates were invalid.
base::Optional<std::vector<MediaFeedsConverter::EpisodeCandidate>>
MediaFeedsConverter::GetEpisodeCandidates(Entity* entity) {
  std::vector<EpisodeCandidate> candidates;
  auto* seasons = GetProperty(entity, schema_org::property::kContainsSeason);
  if (seasons) {
    for (const auto& season : seasons->values->entity_values) {
      auto season_number = GetPositiveIntegerFromProperty(
          season.get(), schema_org::property::kSeasonNumber);
      if (!season_number.has_value())
        return base::nullopt;
      auto season_episodes = GetEpisodeCandidatesFromProperty(
          GetProperty(season.get(), schema_org::property::kEpisode),
          season_number.value());
      if (!season_episodes.has_value())
        return base::nullopt;
      candidates.insert(candidates.end(), season_episodes.value().begin(),
                        season_episodes.value().end());
    }
  }

  auto embedded_episodes = GetEpisodeCandidatesFromProperty(
      GetProperty(entity, schema_org::property::kEpisode), 0);

  if (!embedded_episodes.has_value())
    return base::nullopt;

  candidates.insert(candidates.end(), embedded_episodes.value().begin(),
                    embedded_episodes.value().end());
  return candidates;
}

// Picks the main episode for the item from a list of candidates. Returns
// base::nullopt if there is no main episode.
base::Optional<MediaFeedsConverter::EpisodeCandidate>
MediaFeedsConverter::PickMainEpisode(
    std::vector<MediaFeedsConverter::EpisodeCandidate> candidates) {
  if (candidates.empty())
    return base::nullopt;

  if (candidates.size() == 1)
    return candidates[0];

  std::vector<EpisodeCandidate> main_candidates;
  std::copy_if(
      candidates.begin(), candidates.end(), std::back_inserter(main_candidates),
      [](const EpisodeCandidate& e) {
        return e.action_status == mojom::MediaFeedItemActionStatus::kActive ||
               e.action_status == mojom::MediaFeedItemActionStatus::kCompleted;
      });

  if (main_candidates.empty())
    return base::nullopt;

  return main_candidates[0];
}

// Picks the play next candidate for the item from a list of candidates. Returns
// base::nullopt if there is no matching candidate.
base::Optional<MediaFeedsConverter::EpisodeCandidate>
MediaFeedsConverter::PickPlayNextCandidate(
    std::vector<MediaFeedsConverter::EpisodeCandidate> candidates,
    const base::Optional<MediaFeedsConverter::EpisodeCandidate>& main_episode,
    const std::map<int, int>& number_of_episodes) {
  if (!main_episode.has_value())
    return base::nullopt;

  // Try to find the number of episodes for the main episode's season so we know
  // whether to look in the next season for the next episode. If we don't find
  // it, just look for the next episode in the main episode's season.
  auto find_num_episodes =
      number_of_episodes.find(main_episode.value().season_number);
  int next_episode = main_episode.value().episode_number + 1;
  int next_season = main_episode.value().season_number;
  if (find_num_episodes != number_of_episodes.end() &&
      next_episode > find_num_episodes->second) {
    next_episode = 1;
    next_season++;
  }

  std::vector<EpisodeCandidate> next_candidates;
  std::copy_if(
      candidates.begin(), candidates.end(), std::back_inserter(next_candidates),
      [](const EpisodeCandidate& e) {
        return e.action_status == mojom::MediaFeedItemActionStatus::kPotential;
      });
  auto it = std::find_if(next_candidates.begin(), next_candidates.end(),
                         [&](const EpisodeCandidate& e) {
                           return e.episode_number == next_episode &&
                                  e.season_number == next_season;
                         });
  if (it == next_candidates.end())
    return base::nullopt;
  return *it;
}

// Gets the TV season stored in an embedded entity and updates a map of (season
// number)->(number of episodes). Returns true if the TV season was valid.
// Embedded episodes are handled separately and not checked here.
bool MediaFeedsConverter::GetSeason(const Property& property,
                                    std::map<int, int>* number_of_episodes) {
  if (property.values->entity_values.empty())
    return false;

  EntityPtr& season = property.values->entity_values[0];
  if (season->type != schema_org::entity::kTVSeason)
    return false;

  auto season_number = GetPositiveIntegerFromProperty(
      season.get(), schema_org::property::kSeasonNumber);
  if (!season_number.has_value())
    return false;

  auto number_episodes = GetPositiveIntegerFromProperty(
      season.get(), schema_org::property::kNumberOfEpisodes);
  if (!number_episodes.has_value())
    return false;

  number_of_episodes->insert(
      std::make_pair(season_number.value(), number_episodes.value()));

  return true;
}

// Gets the broadcastEvent entity from the entity and store the result in item
// as LiveDetails. Returns true if the broadcastEvent was valid.
bool MediaFeedsConverter::GetLiveDetails(const EntityPtr& broadcastEvent,
                                         mojom::MediaFeedItem* converted_item) {
  converted_item->live = mojom::LiveDetails::New();

  auto* start_date =
      GetProperty(broadcastEvent.get(), schema_org::property::kStartDate);
  if (!start_date || !IsDateOrDateTime(*start_date))
    return false;
  converted_item->live->start_time = start_date->values->date_time_values[0];

  auto* end_date =
      GetProperty(broadcastEvent.get(), schema_org::property::kEndDate);
  if (end_date) {
    if (!IsDateOrDateTime(*end_date))
      return false;
    converted_item->live->end_time = end_date->values->date_time_values[0];
  }

  return true;
}

bool MediaFeedsConverter::GetMediaFeedItem(
    const EntityPtr& item,
    mojom::MediaFeedItem* converted_item,
    base::flat_set<std::string>* item_ids,
    bool is_embedded_item) {
  auto convert_property = base::BindRepeating(
      &MediaFeedsConverter::ConvertProperty<mojom::MediaFeedItem>,
      base::Unretained(this), item.get(), converted_item);

  auto type = GetMediaItemType(item->type);
  if (!type.has_value()) {
    Log("Missing item type.");
    return false;
  }
  converted_item->type = type.value();

  // Check that the id is present and unique. This does not get converted.
  if (item->id == "" || item_ids->find(item->id) != item_ids->end()) {
    Log("Invalid item ID.");
    return false;
  }
  item_ids->insert(item->id);

  auto* name = GetProperty(item.get(), schema_org::property::kName);
  if (name && IsNonEmptyString(*name)) {
    const auto value = name->values->string_values[0];
    if (!base::UTF8ToUTF16(value.c_str(), value.size(),
                           &converted_item->name)) {
      Log("Invalid name.");
      return false;
    }
  } else {
    Log("Invalid name.");
    return false;
  }

  auto* date_published =
      GetProperty(item.get(), schema_org::property::kDatePublished);
  if (date_published && !date_published->values->date_time_values.empty()) {
    converted_item->date_published =
        date_published->values->date_time_values[0];
  } else {
    Log("Invalid datePublished.");
    return false;
  }

  if (!convert_property.Run(
          schema_org::property::kIsFamilyFriendly, false,
          base::BindOnce(&MediaFeedsConverter::GetIsFamilyFriendly,
                         base::Unretained(this)))) {
    return false;
  }

  auto* image = GetProperty(item.get(), schema_org::property::kImage);
  if (!image)
    return false;
  auto converted_images = GetMediaImage(*image);
  if (!converted_images.has_value())
    return false;
  converted_item->images = std::move(converted_images.value());

  if (!convert_property.Run(
          schema_org::property::kInteractionStatistic, false,
          base::BindOnce(&MediaFeedsConverter::GetInteractionStatistics,
                         base::Unretained(this)))) {
    return false;
  }

  if (!convert_property.Run(
          schema_org::property::kContentRating, false,
          base::BindOnce(&MediaFeedsConverter::GetContentRatings,
                         base::Unretained(this)))) {
    return false;
  }

  auto* genre = GetProperty(item.get(), schema_org::property::kGenre);
  if (genre) {
    if (!IsNonEmptyString(*genre)) {
      Log("Empty genre.");
      return false;
    }
    for (const auto& genre_value : genre->values->string_values) {
      converted_item->genre.push_back(genre_value);
      if (converted_item->genre.size() >= kMaxGenres) {
        Log("Exceeded maximum genres.");
        return false;
      }
    }
  }

  if (!convert_property.Run(
          schema_org::property::kIdentifier, false,
          base::BindOnce(
              &MediaFeedsConverter::GetIdentifiers<mojom::MediaFeedItem>,
              base::Unretained(this)))) {
    return false;
  }

  if (converted_item->type == mojom::MediaFeedItemType::kVideo) {
    if (!convert_property.Run(
            schema_org::property::kAuthor, true,
            base::BindOnce(&MediaFeedsConverter::GetMediaItemAuthor,
                           base::Unretained(this)))) {
      return false;
    }
    if (!convert_property.Run(
            schema_org::property::kDuration, !converted_item->live,
            base::BindOnce(
                &MediaFeedsConverter::GetDuration<mojom::MediaFeedItem>,
                base::Unretained(this)))) {
      return false;
    }
  } else if (converted_item->type == mojom::MediaFeedItemType::kMovie) {
    if (!convert_property.Run(
            schema_org::property::kDuration, /*is_required=*/true,
            base::BindOnce(
                &MediaFeedsConverter::GetDuration<mojom::MediaFeedItem>,
                base::Unretained(this)))) {
      return false;
    }
  } else if (converted_item->type == mojom::MediaFeedItemType::kTVSeries) {
    std::map<int, int> number_of_episodes;
    auto* season =
        GetProperty(item.get(), schema_org::property::kContainsSeason);

    if (season && !GetSeason(*season, &number_of_episodes)) {
      Log("Invalid season.");
      return false;
    }

    auto episodes = GetEpisodeCandidates(item.get());
    if (!episodes.has_value()) {
      Log("Invalid episode.");
      return false;
    }

    auto main_episode = PickMainEpisode(episodes.value());
    if (main_episode.has_value() &&
        !GetEpisode(main_episode.value(), converted_item)) {
      Log("Invalid main episode.");
      return false;
    }

    auto next_episode = PickPlayNextCandidate(episodes.value(), main_episode,
                                              number_of_episodes);
    if (next_episode.has_value() &&
        !GetPlayNextCandidate(next_episode.value(), converted_item)) {
      Log("Invalid PlayNextCandidate.");
      return false;
    }

    // Attempt to set the duration of the item if there is no embedded duration
    // in an episode.
    if (!converted_item->tv_episode &&
        !convert_property.Run(
            schema_org::property::kDuration, /*is_required=*/false,
            base::BindOnce(
                &MediaFeedsConverter::GetDuration<mojom::MediaFeedItem>,
                base::Unretained(this)))) {
      return false;
    }
  }

  bool has_embedded_action =
      item->type == schema_org::entity::kTVSeries && converted_item->action;
  if (!has_embedded_action && !is_embedded_item) {
    auto action_status = GetActionStatus(item.get());
    converted_item->action_status =
        action_status.value_or(mojom::MediaFeedItemActionStatus::kUnknown);
    if (!convert_property.Run(
            schema_org::property::kPotentialAction, !has_embedded_action,
            base::BindOnce(
                &MediaFeedsConverter::GetAction<mojom::MediaFeedItem>,
                base::Unretained(this), converted_item->action_status))) {
      return false;
    }
  }

  return true;
}

// Given the schema.org data_feed_items, iterate through and convert all feed
// items into MediaFeedItemPtr types. Store the converted items in
// converted_feed_items. Skips invalid feed items.
void MediaFeedsConverter::GetDataFeedItems(
    const PropertyPtr& data_feed_items,
    std::vector<mojom::MediaFeedItemPtr>* converted_feed_items) {
  if (data_feed_items->values->entity_values.empty())
    return;

  base::flat_set<std::string> item_ids;

  for (const auto& item : data_feed_items->values->entity_values) {
    mojom::MediaFeedItemPtr converted_item = mojom::MediaFeedItem::New();

    if (item->type == schema_org::entity::kBroadcastEvent) {
      auto* embedded_item =
          GetProperty(item.get(), schema_org::property::kWorkPerformed);
      if (!embedded_item || embedded_item->values->entity_values.empty()) {
        Log("BroadcastEvent with no embedded item.");
        continue;
      }

      if (!GetMediaFeedItem(embedded_item->values->entity_values[0],
                            converted_item.get(), &item_ids,
                            /*is_embedded_item=*/true)) {
        Log("Item was invalid");
        continue;
      }

      if (!GetLiveDetails(item, converted_item.get())) {
        Log("Invalid live details for BroadcastEvent item.");
      }

      bool has_embedded_action =
          item->type == schema_org::entity::kTVSeries && converted_item->action;
      if (!has_embedded_action) {
        auto action_status = GetActionStatus(item.get());
        converted_item->action_status =
            action_status.value_or(mojom::MediaFeedItemActionStatus::kUnknown);
        auto* action =
            GetProperty(item.get(), schema_org::property::kPotentialAction);
        if (!action ||
            !GetAction<mojom::MediaFeedItem>(converted_item->action_status,
                                             *action, converted_item.get())) {
          Log("Invalid action for BroadcastEvent item.");
          continue;
        }
      }
    } else {
      if (!GetMediaFeedItem(item, converted_item.get(), &item_ids,
                            /*is_embedded_item=*/false)) {
        Log("Item was invalid");
        continue;
      }
    }

    converted_feed_items->push_back(std::move(converted_item));
  }
}

bool MediaFeedsConverter::ConvertMediaFeedImpl(
    const schema_org::improved::mojom::EntityPtr& entity,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result) {
  if (!entity) {
    Log("No feed entity. There may have been a problem parsing the schema.org "
        "data.");
    return false;
  }

  if (entity->type != schema_org::entity::kCompleteDataFeed) {
    Log("Entity type was not CompleteDataFeed");
    return false;
  }

  auto* provider = GetProperty(entity.get(), schema_org::property::kProvider);
  if (!provider || !ValidateProvider(*provider, result)) {
    Log("Invalid provider.");
    return false;
  }

  if (!ConvertProperty(
          entity.get(), result, schema_org::property::kAdditionalProperty,
          false,
          base::BindOnce(&MediaFeedsConverter::GetFeedAdditionalProperties,
                         base::Unretained(this)))) {
    return false;
  }

  auto data_feed_items = std::find_if(
      entity->properties.begin(), entity->properties.end(),
      [](const PropertyPtr& property) {
        return property->name == schema_org::property::kDataFeedElement;
      });
  if (data_feed_items != entity->properties.end() &&
      (*data_feed_items)->values) {
    GetDataFeedItems(*data_feed_items, &result->items);
  }

  return true;
}

bool MediaFeedsConverter::ConvertMediaFeed(
    const schema_org::improved::mojom::EntityPtr& entity,
    media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result) {
  DCHECK(result);

  bool was_successful = ConvertMediaFeedImpl(entity, result);

  result->error_logs = log_;
  log_.clear();

  return was_successful;
}

// Append a string to the log message.
void MediaFeedsConverter::Log(const std::string& message) {
  log_.append("\n");
  log_.append(message);
}

}  // namespace media_feeds
