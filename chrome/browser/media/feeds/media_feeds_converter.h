// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
#define CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_

#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/history/media_history_keyed_service.h"
#include "components/schema_org/common/improved_metadata.mojom.h"

namespace media_feeds {

class MediaFeedsConverter {
 public:
  // Given a schema_org entity of type CompleteDataFeed, converts all items
  // contained in the feed to MediaFeedItemPtr type and places them in the
  // outputs nested items vector.
  //
  // The feed should be valid according to https://wicg.github.io/media-feeds/.
  // If not, ConvertMediaFeed does not populate any fields and returns false. If
  // the feed is valid, but some of its feed items are not, ConvertMediaFeed
  // excludes the invalid feed items from the result.
  bool ConvertMediaFeed(
      const schema_org::improved::mojom::EntityPtr& schema_org_entity,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

 private:
  bool ConvertMediaFeedImpl(
      const schema_org::improved::mojom::EntityPtr& schema_org_entity,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

  // Represents a candidate for use as the item's main episode or play next
  // candidate.
  struct EpisodeCandidate {
    schema_org::improved::mojom::Entity* entity;
    mojom::MediaFeedItemActionStatus action_status;
    int season_number;
    int episode_number;
  };

  schema_org::improved::mojom::Property* GetProperty(
      schema_org::improved::mojom::Entity* entity,
      const std::string& name);

  template <typename T>
  bool ConvertProperty(
      schema_org::improved::mojom::Entity* entity,
      T* converted_item,
      const std::string& property_name,
      bool is_required,
      base::OnceCallback<
          bool(const schema_org::improved::mojom::Property& property, T*)>
          convert_property);

  bool ValidateProperty(
      schema_org::improved::mojom::Entity* entity,
      const std::string& name,
      base::OnceCallback<bool(const schema_org::improved::mojom::Property&
                                  property)> property_is_valid);

  bool IsUrl(const schema_org::improved::mojom::Property& property);

  bool IsPositiveInteger(const schema_org::improved::mojom::Property& property);

  bool IsNonEmptyString(const schema_org::improved::mojom::Property& property);

  base::Optional<std::string> GetFirstNonEmptyString(
      const schema_org::improved::mojom::Property& prop);

  bool IsEmail(const schema_org::improved::mojom::Property& email);

  bool IsMediaItemType(const std::string& type);

  bool IsDateOrDateTime(const schema_org::improved::mojom::Property& property);

  base::Optional<uint64_t> GetPositiveIntegerFromProperty(
      schema_org::improved::mojom::Entity* entity,
      const std::string& property_name);

  template <typename T>
  bool GetDuration(const schema_org::improved::mojom::Property& property,
                   T* item);

  base::Optional<mojom::ContentAttribute> GetContentAttribute(
      const std::string& value);

  base::Optional<std::vector<mojom::ContentAttribute>> GetContentAttributes(
      const schema_org::improved::mojom::EntityPtr& image);

  base::Optional<std::vector<mojom::MediaImagePtr>> GetMediaImage(
      const schema_org::improved::mojom::Property& property);

  bool ImageHasOneOfAttributes(const mojom::MediaImagePtr& image,
                               std::vector<mojom::ContentAttribute> attributes);

  base::Optional<std::vector<mojom::MediaImagePtr>> GetLogoImages(
      const schema_org::improved::mojom::Property& property);

  bool GetCurrentlyLoggedInUser(
      const schema_org::improved::mojom::Property& member,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

  bool ValidateProvider(
      const schema_org::improved::mojom::Property& provider,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

  bool GetAssociatedOriginURLs(
      const schema_org::improved::mojom::Property& property,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

  bool GetFeedAdditionalProperties(
      const schema_org::improved::mojom::Property& property,
      media_history::MediaHistoryKeyedService::MediaFeedFetchResult* result);

  bool GetMediaItemAuthor(const schema_org::improved::mojom::Property& author,
                          mojom::MediaFeedItem* item);

  bool GetContentRatings(const schema_org::improved::mojom::Property& property,
                         mojom::MediaFeedItem* item);

  template <typename T>
  bool GetIdentifiers(const schema_org::improved::mojom::Property& property,
                      T* item);

  base::Optional<mojom::InteractionCounterType> GetInteractionType(
      const schema_org::improved::mojom::Property& property);

  bool GetInteractionStatistics(
      const schema_org::improved::mojom::Property& property,
      mojom::MediaFeedItem* item);

  base::Optional<mojom::MediaFeedItemType> GetMediaItemType(
      const std::string& schema_org_type);

  bool GetIsFamilyFriendly(
      const schema_org::improved::mojom::Property& property,
      mojom::MediaFeedItem* item);

  base::Optional<mojom::MediaFeedItemActionStatus> GetActionStatus(
      schema_org::improved::mojom::Entity* entity);

  template <typename T>
  bool GetAction(mojom::MediaFeedItemActionStatus action_status,
                 const schema_org::improved::mojom::Property& property,
                 T* item);

  bool GetEpisode(const EpisodeCandidate& candidate,
                  mojom::MediaFeedItem* item);

  bool GetPlayNextCandidate(const EpisodeCandidate& candidate,
                            mojom::MediaFeedItem* item);

  base::Optional<EpisodeCandidate> GetEpisodeCandidate(
      const schema_org::improved::mojom::EntityPtr& entity);

  base::Optional<std::vector<EpisodeCandidate>>
  GetEpisodeCandidatesFromProperty(
      schema_org::improved::mojom::Property* property,
      int season_number);

  base::Optional<std::vector<EpisodeCandidate>> GetEpisodeCandidates(
      schema_org::improved::mojom::Entity* entity);

  base::Optional<EpisodeCandidate> PickMainEpisode(
      std::vector<EpisodeCandidate> candidates);

  base::Optional<EpisodeCandidate> PickPlayNextCandidate(
      std::vector<EpisodeCandidate> candidates,
      const base::Optional<EpisodeCandidate>& main_episode,
      const std::map<int, int>& number_of_episodes);

  bool GetSeason(const schema_org::improved::mojom::Property& property,
                 std::map<int, int>* number_of_episodes);

  bool GetLiveDetails(
      const schema_org::improved::mojom::EntityPtr& broadcastEvent,
      mojom::MediaFeedItem* converted_item);

  bool GetMediaFeedItem(const schema_org::improved::mojom::EntityPtr& item,
                        mojom::MediaFeedItem* converted_item,
                        base::flat_set<std::string>* item_ids,
                        bool is_embedded_item);

  void GetDataFeedItems(
      const schema_org::improved::mojom::PropertyPtr& data_feed_items,
      std::vector<mojom::MediaFeedItemPtr>* converted_feed_items);

  // Append a string to the log message.
  void Log(const std::string& message);

  std::string log_;
};

}  // namespace media_feeds

#endif  // CHROME_BROWSER_MEDIA_FEEDS_MEDIA_FEEDS_CONVERTER_H_
