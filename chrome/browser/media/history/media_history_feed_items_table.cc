// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feed_items_table.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom-forward.h"
#include "chrome/browser/media/feeds/media_feeds_utils.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace media_history {

namespace {

// The maximum number of images to allow.
constexpr int kMaxImageCount = 5;
// The maximum number of genres to allow.
constexpr int kMaxGenres = 3;

media_feeds::InteractionCounter_Type Convert(
    const media_feeds::mojom::InteractionCounterType& type) {
  switch (type) {
    case media_feeds::mojom::InteractionCounterType::kLike:
      return media_feeds::InteractionCounter_Type_LIKE;
    case media_feeds::mojom::InteractionCounterType::kDislike:
      return media_feeds::InteractionCounter_Type_DISLIKE;
    case media_feeds::mojom::InteractionCounterType::kWatch:
      return media_feeds::InteractionCounter_Type_WATCH;
  }
}

base::Optional<media_feeds::mojom::InteractionCounterType> Convert(
    const media_feeds::InteractionCounter_Type& type) {
  switch (type) {
    case media_feeds::InteractionCounter_Type_LIKE:
      return media_feeds::mojom::InteractionCounterType::kLike;
    case media_feeds::InteractionCounter_Type_DISLIKE:
      return media_feeds::mojom::InteractionCounterType::kDislike;
    case media_feeds::InteractionCounter_Type_WATCH:
      return media_feeds::mojom::InteractionCounterType::kWatch;
    case media_feeds::
        InteractionCounter_Type_InteractionCounter_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
    case media_feeds::
        InteractionCounter_Type_InteractionCounter_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return base::nullopt;
  }
}

media_feeds::mojom::IdentifierPtr Convert(
    const media_feeds::Identifier& identifier) {
  auto out = media_feeds::mojom::Identifier::New();

  switch (identifier.type()) {
    case media_feeds::Identifier_Type_TMS_ROOT_ID:
      out->type = media_feeds::mojom::Identifier::Type::kTMSRootId;
      break;
    case media_feeds::Identifier_Type_TMS_ID:
      out->type = media_feeds::mojom::Identifier::Type::kTMSId;
      break;
    case media_feeds::Identifier_Type_PARTNER_ID:
      out->type = media_feeds::mojom::Identifier::Type::kPartnerId;
      break;
    case media_feeds::
        Identifier_Type_Identifier_Type_INT_MIN_SENTINEL_DO_NOT_USE_:
    case media_feeds::
        Identifier_Type_Identifier_Type_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      break;
  }

  out->value = identifier.value();
  return out;
}

media_feeds::mojom::ActionPtr Convert(const media_feeds::Action& action) {
  auto out = media_feeds::mojom::Action::New();
  out->url = GURL(action.url());

  if (action.start_time_secs()) {
    out->start_time = base::TimeDelta::FromSeconds(action.start_time_secs());
  }

  return out;
}

media_feeds::mojom::LiveDetailsPtr Convert(
    const media_feeds::LiveDetails& live_details) {
  auto out = media_feeds::mojom::LiveDetails::New();

  if (live_details.start_time_secs()) {
    out->start_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(live_details.start_time_secs()));
  }

  if (live_details.end_time_secs()) {
    out->end_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(live_details.end_time_secs()));
  }

  return out;
}

media_feeds::mojom::MediaImagePtr Convert(const media_feeds::Image& image) {
  return media_feeds::ProtoToMediaImage(image);
}

void FillIdentifier(const media_feeds::mojom::IdentifierPtr& identifier,
                    media_feeds::Identifier* proto) {
  switch (identifier->type) {
    case media_feeds::mojom::Identifier::Type::kTMSRootId:
      proto->set_type(media_feeds::Identifier_Type_TMS_ROOT_ID);
      break;
    case media_feeds::mojom::Identifier::Type::kTMSId:
      proto->set_type(media_feeds::Identifier_Type_TMS_ID);
      break;
    case media_feeds::mojom::Identifier::Type::kPartnerId:
      proto->set_type(media_feeds::Identifier_Type_PARTNER_ID);
      break;
  }

  proto->set_value(identifier->value);
}

void FillAction(const media_feeds::mojom::ActionPtr& action,
                media_feeds::Action* proto) {
  proto->set_url(action->url.spec());

  if (action->start_time.has_value())
    proto->set_start_time_secs(action->start_time->InSeconds());
}

void FillLiveDetails(const media_feeds::mojom::LiveDetailsPtr& live_details,
                     media_feeds::LiveDetails* proto) {
  proto->set_start_time_secs(
      live_details->start_time.ToDeltaSinceWindowsEpoch().InSeconds());

  if (live_details->end_time.has_value())
    proto->set_end_time_secs(
        live_details->end_time->ToDeltaSinceWindowsEpoch().InSeconds());
}

void AssignStatement(sql::Statement* statement,
                     sql::Database* db,
                     const sql::StatementID& id,
                     const std::vector<std::string>& sql) {
  statement->Assign(
      db->GetCachedStatement(id, base::JoinString(sql, " ").c_str()));
}

}  // namespace

const char MediaHistoryFeedItemsTable::kTableName[] = "mediaFeedItem";

const char MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName[] =
    "Media.Feeds.FeedItem.ReadResult";

MediaHistoryFeedItemsTable::MediaHistoryFeedItemsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedItemsTable::~MediaHistoryFeedItemsTable() = default;

sql::InitStatus MediaHistoryFeedItemsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      "CREATE TABLE IF NOT EXISTS mediaFeedItem("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "feed_id INTEGER NOT NULL,"
      "type INTEGER NOT NULL,"
      "name TEXT, "
      "author BLOB, "
      "date_published_s INTEGER,"
      "is_family_friendly INTEGER,"
      "action_status INTEGER NOT NULL,"
      "action BLOB, "
      "interaction_counters BLOB, "
      "content_rating BLOB, "
      "genre BLOB,"
      "duration_s INTEGER,"
      "is_live INTEGER,"
      "live_start_time_s INTEGER,"
      "live_end_time_s INTEGER,"
      "tv_episode BLOB, "
      "play_next_candidate BLOB, "
      "identifiers BLOB, "
      "shown_count INTEGER DEFAULT 0,"
      "clicked INTEGER DEFAULT 0, "
      "images BLOB, "
      "safe_search_result INTEGER DEFAULT 0, "
      "CONSTRAINT fk_feed "
      "FOREIGN KEY (feed_id) "
      "REFERENCES mediaFeed(id) "
      "ON DELETE CASCADE"
      ")");

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeedItem_feed_id_index ON "
        "mediaFeedItem (feed_id)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS "
        "mediaFeedItem_safe_search_result_feed_id_index ON "
        "mediaFeedItem (safe_search_result, feed_id)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS "
        "mediaFeedItem_continue_watching_index ON "
        "mediaFeedItem (action_status, play_next_candidate, "
        "safe_search_result)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS "
        "mediaFeedItem_continue_watching_with_type_index ON "
        "mediaFeedItem (action_status, play_next_candidate, type, "
        "safe_search_result)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS "
        "mediaFeedItem_for_feed_index ON "
        "mediaFeedItem (feed_id, action_status, play_next_candidate, "
        "safe_search_result)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history feed items table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedItemsTable::SaveItem(
    const int64_t feed_id,
    const media_feeds::mojom::MediaFeedItemPtr& item) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO mediaFeedItem "
      "(feed_id, type, name, date_published_s, is_family_friendly, "
      "action_status, genre, duration_s, is_live, live_start_time_s, "
      "live_end_time_s, shown_count, clicked, author, action, "
      "interaction_counters, content_rating, identifiers, tv_episode, "
      "play_next_candidate, images, safe_search_result) VALUES "
      "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, feed_id);
  statement.BindInt64(1, static_cast<int>(item->type));
  statement.BindString16(2, item->name);
  statement.BindInt64(
      3, item->date_published.ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(4, static_cast<int>(item->is_family_friendly));
  statement.BindInt64(5, static_cast<int>(item->action_status));

  if (!item->genre.empty()) {
    media_feeds::GenreSet genres;

    for (auto& entry : item->genre) {
      genres.add_genre(entry);

      if (genres.genre_size() >= kMaxGenres)
        break;
    }
    BindProto(statement, 6, genres);
  } else {
    statement.BindNull(6);
  }

  if (item->duration.has_value()) {
    statement.BindInt64(7, item->duration->InSeconds());
  } else {
    statement.BindNull(7);
  }

  statement.BindBool(8, !item->live.is_null());

  if (!item->live.is_null()) {
    statement.BindInt64(
        9, item->live->start_time.ToDeltaSinceWindowsEpoch().InSeconds());

    if (item->live->end_time.has_value()) {
      statement.BindInt64(
          10, item->live->end_time->ToDeltaSinceWindowsEpoch().InSeconds());
    } else {
      statement.BindNull(10);
    }
  } else {
    statement.BindNull(9);
    statement.BindNull(10);
  }

  statement.BindInt64(11, item->shown_count);
  statement.BindBool(12, item->clicked);

  if (item->author) {
    media_feeds::Author author;
    author.set_name(item->author->name);
    author.set_url(item->author->url.spec());
    BindProto(statement, 13, author);
  } else {
    statement.BindNull(13);
  }

  if (item->action) {
    media_feeds::Action action;
    FillAction(item->action, &action);
    BindProto(statement, 14, action);
  } else {
    statement.BindNull(14);
  }

  if (!item->interaction_counters.empty()) {
    media_feeds::InteractionCounterSet counters;

    for (auto& entry : item->interaction_counters) {
      auto* counter = counters.add_counter();
      counter->set_type(Convert(entry.first));
      counter->set_count(entry.second);
    }

    BindProto(statement, 15, counters);
  } else {
    statement.BindNull(15);
  }

  if (!item->content_ratings.empty()) {
    media_feeds::ContentRatingSet ratings;

    for (auto& entry : item->content_ratings) {
      auto* rating = ratings.add_rating();
      rating->set_agency(entry->agency);
      rating->set_value(entry->value);
    }

    BindProto(statement, 16, ratings);
  } else {
    statement.BindNull(16);
  }

  if (!item->identifiers.empty()) {
    media_feeds::IdentifierSet identifiers;

    for (auto& identifier : item->identifiers)
      FillIdentifier(identifier, identifiers.add_identifier());

    BindProto(statement, 17, identifiers);
  } else {
    statement.BindNull(17);
  }

  if (item->tv_episode) {
    media_feeds::TVEpisode tv_episode;
    tv_episode.set_name(item->tv_episode->name);
    tv_episode.set_episode_number(item->tv_episode->episode_number);
    tv_episode.set_season_number(item->tv_episode->season_number);
    tv_episode.set_duration_secs(item->tv_episode->duration.InSeconds());

    for (auto& identifier : item->tv_episode->identifiers)
      FillIdentifier(identifier, tv_episode.add_identifier());

    for (auto& image : item->tv_episode->images)
      media_feeds::MediaImageToProto(tv_episode.add_image(), image);

    if (!item->tv_episode->live.is_null()) {
      FillLiveDetails(item->tv_episode->live,
                      tv_episode.mutable_live_details());
    }

    BindProto(statement, 18, tv_episode);
  } else {
    statement.BindNull(18);
  }

  if (item->play_next_candidate) {
    media_feeds::PlayNextCandidate play_next_candidate;
    play_next_candidate.set_name(item->play_next_candidate->name);
    play_next_candidate.set_episode_number(
        item->play_next_candidate->episode_number);
    play_next_candidate.set_season_number(
        item->play_next_candidate->season_number);
    play_next_candidate.set_duration_secs(
        item->play_next_candidate->duration.InSeconds());
    FillAction(item->play_next_candidate->action,
               play_next_candidate.mutable_action());

    for (auto& identifier : item->play_next_candidate->identifiers)
      FillIdentifier(identifier, play_next_candidate.add_identifier());

    for (auto& image : item->play_next_candidate->images)
      media_feeds::MediaImageToProto(play_next_candidate.add_image(), image);

    BindProto(statement, 19, play_next_candidate);
  } else {
    statement.BindNull(19);
  }

  if (!item->images.empty()) {
    BindProto(statement, 20,
              media_feeds::MediaImagesToProto(item->images, kMaxImageCount));
  } else {
    statement.BindNull(20);
  }

  statement.BindInt64(21, static_cast<int>(item->safe_search_result));

  return statement.Run();
}

bool MediaHistoryFeedItemsTable::DeleteItems(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM mediaFeedItem WHERE feed_id = ?"));

  statement.BindInt64(0, feed_id);
  return statement.Run();
}

std::vector<media_feeds::mojom::MediaFeedItemPtr>
MediaHistoryFeedItemsTable::GetItems(
    const MediaHistoryKeyedService::GetMediaFeedItemsRequest& request) {
  std::vector<media_feeds::mojom::MediaFeedItemPtr> items;
  if (!CanAccessDatabase())
    return items;

  std::vector<std::string> sql;
  sql.push_back(
      "SELECT type, name, date_published_s, is_family_friendly, "
      "action_status, genre, duration_s, is_live, live_start_time_s, "
      "live_end_time_s, shown_count, clicked, author, action, "
      "interaction_counters, content_rating, identifiers, tv_episode, "
      "play_next_candidate, images, safe_search_result, id FROM mediaFeedItem");

  sql::Statement statement;

  if (request.type ==
      MediaHistoryKeyedService::GetMediaFeedItemsRequest::Type::kDebugAll) {
    // Debug request should just return all feed items for a single feed.
    sql.push_back("WHERE feed_id = ?");

    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::JoinString(sql, " ").c_str()));

    statement.BindInt64(0, *request.feed_id);
  } else if (request.type ==
             MediaHistoryKeyedService::GetMediaFeedItemsRequest::Type::
                 kItemsForFeed) {
    // kItemsForFeed should return items for a feed. Ordered by clicked and
    // shown count so items that have been clicked and shown a lot will be at
    // the end. Items must not be continue watching items.
    sql.push_back(
        "WHERE feed_id = ? AND action_status != ? AND play_next_candidate IS "
        "NULL");

    if (request.fetched_items_should_be_safe)
      sql.push_back("AND safe_search_result = ?");

    if (request.filter_by_type.has_value())
      sql.push_back("AND type = ?");

    sql.push_back("LIMIT ?");

    // For each different query combination we should have an assign statement
    // call that will generate a unique SQL_FROM_HERE value.
    if (request.fetched_items_should_be_safe && request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.fetched_items_should_be_safe) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    }

    // Now bind all the parameters to the query.
    int bind_index = 0;
    statement.BindInt64(bind_index++, *request.feed_id);
    statement.BindInt64(
        bind_index++,
        static_cast<int>(
            media_feeds::mojom::MediaFeedItemActionStatus::kActive));

    if (request.fetched_items_should_be_safe) {
      statement.BindInt64(
          bind_index++,
          static_cast<int>(media_feeds::mojom::SafeSearchResult::kSafe));
    }

    if (request.filter_by_type.has_value()) {
      statement.BindInt64(bind_index++,
                          static_cast<int>(*request.filter_by_type));
    }

    statement.BindInt64(bind_index++, *request.limit);
  } else if (request.type ==
             MediaHistoryKeyedService::GetMediaFeedItemsRequest::Type::
                 kContinueWatching) {
    // kContinueWatching should return items across all feeds that either have
    // an active action status or a play next candidate. Ordered by most recent
    // first.
    sql.push_back(
        "WHERE (action_status = ? OR play_next_candidate IS NOT NULL)");

    if (request.fetched_items_should_be_safe)
      sql.push_back("AND safe_search_result = ?");

    if (request.filter_by_type.has_value())
      sql.push_back("AND type = ?");

    sql.push_back("ORDER BY id DESC LIMIT ?");

    // For each different query combination we should have an assign statement
    // call that will generate a unique SQL_FROM_HERE value.
    if (request.fetched_items_should_be_safe && request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.fetched_items_should_be_safe) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    }

    // Now bind all the parameters to the query.
    int bind_index = 0;
    statement.BindInt64(
        bind_index++,
        static_cast<int>(
            media_feeds::mojom::MediaFeedItemActionStatus::kActive));

    if (request.fetched_items_should_be_safe) {
      statement.BindInt64(
          bind_index++,
          static_cast<int>(media_feeds::mojom::SafeSearchResult::kSafe));
    }

    if (request.filter_by_type.has_value()) {
      statement.BindInt64(bind_index++,
                          static_cast<int>(*request.filter_by_type));
    }

    statement.BindInt64(bind_index++, *request.limit);
  }

  DCHECK(statement.is_valid());

  while (statement.Step()) {
    auto item = media_feeds::mojom::MediaFeedItem::New();

    item->type = static_cast<media_feeds::mojom::MediaFeedItemType>(
        statement.ColumnInt64(0));
    item->is_family_friendly =
        static_cast<media_feeds::mojom::IsFamilyFriendly>(
            statement.ColumnInt64(3));
    item->action_status =
        static_cast<media_feeds::mojom::MediaFeedItemActionStatus>(
            statement.ColumnInt64(4));
    item->safe_search_result =
        static_cast<media_feeds::mojom::SafeSearchResult>(
            statement.ColumnInt64(20));

    if (!IsKnownEnumValue(item->type)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadType);
      continue;
    }

    if (!IsKnownEnumValue(item->action_status)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadActionStatus);
      continue;
    }

    if (!IsKnownEnumValue(item->safe_search_result)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadSafeSearchResult);
      continue;
    }

    if (!IsKnownEnumValue(item->is_family_friendly)) {
      base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                    FeedItemReadResult::kBadIsFamilyFriendly);
      continue;
    }

    if (statement.GetColumnType(12) == sql::ColumnType::kBlob) {
      media_feeds::Author author;
      if (!GetProto(statement, 12, author)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadAuthor);

        continue;
      }

      item->author = media_feeds::mojom::Author::New();
      item->author->name = author.name();
      item->author->url = GURL(author.url());
    }

    if (statement.GetColumnType(13) == sql::ColumnType::kBlob) {
      media_feeds::Action action;
      if (!GetProto(statement, 13, action)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadAction);

        continue;
      }

      item->action = Convert(action);
    }

    if (statement.GetColumnType(14) == sql::ColumnType::kBlob) {
      media_feeds::InteractionCounterSet counters;
      if (!GetProto(statement, 14, counters)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadInteractionCounters);

        continue;
      }

      for (auto& counter : counters.counter()) {
        item->interaction_counters.emplace(*Convert(counter.type()),
                                           counter.count());
      }
    }

    if (statement.GetColumnType(15) == sql::ColumnType::kBlob) {
      media_feeds::ContentRatingSet ratings;
      if (!GetProto(statement, 15, ratings)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadContentRatings);

        continue;
      }

      for (auto& rating : ratings.rating()) {
        auto mojo_rating = media_feeds::mojom::ContentRating::New();
        mojo_rating->agency = rating.agency();
        mojo_rating->value = rating.value();
        item->content_ratings.push_back(std::move(mojo_rating));
      }
    }

    if (statement.GetColumnType(16) == sql::ColumnType::kBlob) {
      media_feeds::IdentifierSet identifiers;
      if (!GetProto(statement, 16, identifiers)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadIdentifiers);

        continue;
      }

      for (auto& identifier : identifiers.identifier())
        item->identifiers.push_back(Convert(identifier));
    }

    if (statement.GetColumnType(17) == sql::ColumnType::kBlob) {
      media_feeds::TVEpisode tv_episode;
      if (!GetProto(statement, 17, tv_episode)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadTVEpisode);

        continue;
      }

      item->tv_episode = media_feeds::mojom::TVEpisode::New();
      item->tv_episode->name = tv_episode.name();
      item->tv_episode->episode_number = tv_episode.episode_number();
      item->tv_episode->season_number = tv_episode.season_number();
      item->tv_episode->duration =
          base::TimeDelta::FromSeconds(tv_episode.duration_secs());

      if (tv_episode.has_live_details())
        item->tv_episode->live = Convert(tv_episode.live_details());

      for (auto& identifier : tv_episode.identifier())
        item->tv_episode->identifiers.push_back(Convert(identifier));

      for (auto& image : tv_episode.image())
        item->tv_episode->images.push_back(Convert(image));
    }

    if (statement.GetColumnType(18) == sql::ColumnType::kBlob) {
      media_feeds::PlayNextCandidate play_next_candidate;
      if (!GetProto(statement, 18, play_next_candidate)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadPlayNextCandidate);

        continue;
      }

      item->play_next_candidate = media_feeds::mojom::PlayNextCandidate::New();
      item->play_next_candidate->name = play_next_candidate.name();
      item->play_next_candidate->episode_number =
          play_next_candidate.episode_number();
      item->play_next_candidate->season_number =
          play_next_candidate.season_number();
      item->play_next_candidate->duration =
          base::TimeDelta::FromSeconds(play_next_candidate.duration_secs());
      item->play_next_candidate->action = Convert(play_next_candidate.action());

      for (auto& identifier : play_next_candidate.identifier())
        item->play_next_candidate->identifiers.push_back(Convert(identifier));

      for (auto& image : play_next_candidate.image())
        item->play_next_candidate->images.push_back(Convert(image));
    }

    if (statement.GetColumnType(19) == sql::ColumnType::kBlob) {
      media_feeds::ImageSet image_set;
      if (!GetProto(statement, 19, image_set)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadImages);

        continue;
      }

      item->images = media_feeds::ProtoToMediaImages(image_set, kMaxImageCount);
    }

    base::UmaHistogramEnumeration(kFeedItemReadResultHistogramName,
                                  FeedItemReadResult::kSuccess);

    item->name = statement.ColumnString16(1);
    item->date_published = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2)));

    if (statement.GetColumnType(5) == sql::ColumnType::kBlob) {
      media_feeds::GenreSet genre_set;
      if (!GetProto(statement, 5, genre_set)) {
        base::UmaHistogramEnumeration(
            MediaHistoryFeedItemsTable::kFeedItemReadResultHistogramName,
            FeedItemReadResult::kBadGenres);

        continue;
      }

      for (auto& genre : genre_set.genre()) {
        item->genre.push_back(genre);

        if (item->genre.size() >= kMaxGenres)
          break;
      }
    }

    if (statement.GetColumnType(6) == sql::ColumnType::kInteger)
      item->duration = base::TimeDelta::FromSeconds(statement.ColumnInt64(6));

    if (statement.ColumnBool(7)) {
      item->live = media_feeds::mojom::LiveDetails::New();

      if (statement.GetColumnType(8) == sql::ColumnType::kInteger) {
        item->live->start_time = base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromSeconds(statement.ColumnInt64(8)));
      }

      if (statement.GetColumnType(9) == sql::ColumnType::kInteger) {
        item->live->end_time = base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromSeconds(statement.ColumnInt64(9)));
      }
    }

    item->shown_count = statement.ColumnInt64(10);
    item->clicked = statement.ColumnBool(11);
    item->id = statement.ColumnInt64(21);

    items.push_back(std::move(item));
  }

  DCHECK(statement.Succeeded());
  return items;
}

MediaHistoryKeyedService::PendingSafeSearchCheckList
MediaHistoryFeedItemsTable::GetPendingSafeSearchCheckItems() {
  MediaHistoryKeyedService::PendingSafeSearchCheckList items;

  if (!CanAccessDatabase())
    return items;

  sql::Statement statement(
      DB()->GetUniqueStatement("SELECT id, action, play_next_candidate FROM "
                               "mediaFeedItem WHERE safe_search_result = ?"));

  statement.BindInt64(
      0, static_cast<int>(media_feeds::mojom::SafeSearchResult::kUnknown));

  DCHECK(statement.is_valid());

  while (statement.Step()) {
    auto check =
        std::make_unique<MediaHistoryKeyedService::PendingSafeSearchCheck>(
            MediaHistoryKeyedService::SafeSearchCheckedType::kFeedItem,
            statement.ColumnInt64(0));

    if (statement.GetColumnType(1) == sql::ColumnType::kBlob) {
      media_feeds::Action action;
      if (!GetProto(statement, 1, action))
        continue;

      GURL url(action.url());
      if (url.is_valid())
        check->urls.insert(url);
    }

    if (statement.GetColumnType(2) == sql::ColumnType::kBlob) {
      media_feeds::PlayNextCandidate play_next_candidate;
      if (!GetProto(statement, 2, play_next_candidate))
        continue;

      GURL url(play_next_candidate.action().url());
      if (url.is_valid())
        check->urls.insert(url);
    }

    if (!check->urls.empty())
      items.push_back(std::move(check));
  }

  return items;
}

base::Optional<int64_t> MediaHistoryFeedItemsTable::StoreSafeSearchResult(
    int64_t feed_item_id,
    media_feeds::mojom::SafeSearchResult result) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeedItem SET safe_search_result = ? WHERE id = ?"));
  statement.BindInt64(0, static_cast<int>(result));
  statement.BindInt64(1, feed_item_id);

  if (!statement.Run())
    return base::nullopt;

  {
    // Get the feed that was affected.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE, "SELECT feed_id FROM mediaFeedItem WHERE id = ?"));
    statement.BindInt64(0, feed_item_id);

    while (statement.Step())
      return statement.ColumnInt64(0);
  }

  return base::nullopt;
}

bool MediaHistoryFeedItemsTable::IncrementShownCount(
    const int64_t feed_item_id) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeedItem SET shown_count = shown_count + 1 WHERE id = ?"));
  statement.BindInt64(0, feed_item_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedItemsTable::MarkAsClicked(const int64_t feed_item_id) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE mediaFeedItem SET clicked = 1 WHERE id = ?"));
  statement.BindInt64(0, feed_item_id);
  return statement.Run();
}

}  // namespace media_history
