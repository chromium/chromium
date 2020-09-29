// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_feeds_table.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "base/updateable_sequenced_task_runner.h"
#include "chrome/browser/media/feeds/media_feeds.pb.h"
#include "chrome/browser/media/feeds/media_feeds_utils.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_store.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace media_history {

namespace {

// The maximum number of logos to allow.
const int kMaxLogoCount = 5;

base::UnguessableToken ProtoToUnguessableToken(
    const media_feeds::FeedResetToken& proto) {
  return base::UnguessableToken::Deserialize(proto.high(), proto.low());
}

void AssignStatement(sql::Statement* statement,
                     sql::Database* db,
                     const sql::StatementID& id,
                     const std::vector<std::string>& sql) {
  statement->Assign(
      db->GetCachedStatement(id, base::JoinString(sql, " ").c_str()));
}

}  // namespace

const char MediaHistoryFeedsTable::kTableName[] = "mediaFeed";

const char MediaHistoryFeedsTable::kFeedReadResultHistogramName[] =
    "Media.Feeds.Feed.ReadResult";

MediaHistoryFeedsTable::MediaHistoryFeedsTable(
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : MediaHistoryTableBase(std::move(db_task_runner)) {}

MediaHistoryFeedsTable::~MediaHistoryFeedsTable() = default;

sql::InitStatus MediaHistoryFeedsTable::CreateTableIfNonExistent() {
  if (!CanAccessDatabase())
    return sql::INIT_FAILURE;

  bool success = DB()->Execute(
      base::StringPrintf("CREATE TABLE IF NOT EXISTS %s("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         "origin_id INTEGER NOT NULL UNIQUE,"
                         "url TEXT NOT NULL, "
                         "last_discovery_time_s INTEGER, "
                         "last_fetch_time_s INTEGER, "
                         "user_status INTEGER DEFAULT 0, "
                         "last_fetch_result INTEGER DEFAULT 0, "
                         "fetch_failed_count INTEGER, "
                         "last_fetch_time_not_cache_hit_s INTEGER, "
                         "last_fetch_item_count INTEGER, "
                         "last_fetch_safe_item_count INTEGER, "
                         "last_fetch_play_next_count INTEGER, "
                         "last_fetch_content_types INTEGER, "
                         "logo BLOB, "
                         "display_name TEXT, "
                         "user_identifier BLOB, "
                         "last_display_time_s INTEGER, "
                         "reset_reason INTEGER DEFAULT 0, "
                         "reset_token BLOB, "
                         "cookie_name_filter TEXT, "
                         "safe_search_result INTEGER DEFAULT 0, "
                         "CONSTRAINT fk_origin "
                         "FOREIGN KEY (origin_id) "
                         "REFERENCES origin(id) "
                         "ON DELETE CASCADE"
                         ")",
                         kTableName)
          .c_str());

  if (success) {
    success = DB()->Execute(
        base::StringPrintf(
            "CREATE INDEX IF NOT EXISTS mediaFeed_origin_id_index ON "
            "%s (origin_id)",
            kTableName)
            .c_str());
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeed_fetch_time_index ON "
        "mediaFeed (last_fetch_time_s)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeed_safe_search_result ON "
        "mediaFeed (safe_search_result)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeed_last_fetch_content_types_index "
        "ON "
        "mediaFeed (last_fetch_content_types)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeed_top_feeds_index ON "
        "mediaFeed (last_fetch_content_types, safe_search_result)");
  }

  if (success) {
    success = DB()->Execute(
        "CREATE INDEX IF NOT EXISTS mediaFeed_user_status_index ON "
        "mediaFeed (user_status)");
  }

  if (!success) {
    ResetDB();
    LOG(ERROR) << "Failed to create media history feeds table.";
    return sql::INIT_FAILURE;
  }

  return sql::INIT_OK;
}

bool MediaHistoryFeedsTable::DiscoverFeed(const GURL& url) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  const auto origin =
      MediaHistoryOriginTable::GetOriginForStorage(url::Origin::Create(url));
  const auto now = base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();

  base::Optional<GURL> feed_url;
  base::Optional<int64_t> feed_id;

  {
    // Check if we already have a feed for the current origin;
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT id, url FROM mediaFeed WHERE origin_id = (SELECT id FROM "
        "origin WHERE origin = ?)"));
    statement.BindString(0, origin);

    while (statement.Step()) {
      DCHECK(!feed_id);
      DCHECK(!feed_url);

      feed_id = statement.ColumnInt64(0);
      feed_url = GURL(statement.ColumnString(1));
    }
  }

  if (!feed_url || url != feed_url) {
    // If the feed does not exist or exists and has a different URL then we
    // should replace the feed.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO mediaFeed "
        "(origin_id, url, last_discovery_time_s) VALUES "
        "((SELECT id FROM origin WHERE origin = ?), ?, ?)"));
    statement.BindString(0, origin);
    statement.BindString(1, url.spec());
    statement.BindInt64(2, now);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  } else {
    // If the feed already exists in the database with the same URL we should
    // just update the last discovery time so we don't delete the old entry.
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_discovery_time_s = ? WHERE id = ?"));
    statement.BindInt64(0, now);
    statement.BindInt64(1, *feed_id);
    return statement.Run() && DB()->GetLastChangeCount() == 1;
  }
}

std::vector<media_feeds::mojom::MediaFeedPtr> MediaHistoryFeedsTable::GetRows(
    const MediaHistoryKeyedService::GetMediaFeedsRequest& request) {
  std::vector<media_feeds::mojom::MediaFeedPtr> feeds;
  if (!CanAccessDatabase())
    return feeds;

  base::Optional<double> origin_count;
  double rank = 0;

  const bool top_feeds =
      request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::Type::
                          kTopFeedsForFetch ||
      request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::Type::
                          kTopFeedsForDisplay;

  std::vector<std::string> sql;
  sql.push_back(
      "SELECT "
      "mediaFeed.id, "
      "mediaFeed.url, "
      "mediaFeed.last_discovery_time_s, "
      "mediaFeed.last_fetch_time_s, "
      "mediaFeed.user_status, "
      "mediaFeed.last_fetch_result, "
      "mediaFeed.fetch_failed_count, "
      "mediaFeed.last_fetch_time_not_cache_hit_s, "
      "mediaFeed.last_fetch_item_count, "
      "mediaFeed.last_fetch_safe_item_count, "
      "mediaFeed.last_fetch_play_next_count, "
      "mediaFeed.last_fetch_content_types, "
      "mediaFeed.logo, "
      "mediaFeed.display_name, "
      "mediaFeed.last_display_time_s, "
      "mediaFeed.reset_reason, "
      "mediaFeed.user_identifier, "
      "mediaFeed.cookie_name_filter, "
      "mediaFeed.safe_search_result, "
      "mediaFeed.reset_token ");

  sql::Statement statement;

  if (top_feeds) {
    // Check the request has the right parameters.
    DCHECK(request.limit.has_value());

    if (request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::Type::
                            kTopFeedsForDisplay) {
      DCHECK(request.fetched_items_min.has_value());
    } else if (request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::
                                   Type::kTopFeedsForFetch) {
      DCHECK(request.audio_video_watchtime_min.has_value());
    }

    // If we need the top feeds we should select rows from the origin table and
    // LEFT JOIN mediaFeed. This means there should be a row for each origin
    // and if there is a media feed that will be included.
    sql.push_back(
        ",origin.aggregate_watchtime_audio_video_s "
        "FROM origin "
        "LEFT JOIN mediaFeed "
        "ON origin.id = mediaFeed.origin_id");

    // If we have an audio/video watchtime requirement we should add that.
    if (request.audio_video_watchtime_min.has_value())
      sql.push_back("WHERE origin.aggregate_watchtime_audio_video_s >= ?");

    // If we have a content type filter then we should add that.
    if (request.filter_by_type.has_value())
      sql.push_back("WHERE mediaFeed.last_fetch_content_types & ?");

    // Finally, order the results by watchtime.
    sql.push_back("ORDER BY origin.aggregate_watchtime_audio_video_s DESC");

    // Get the total count of the origins so we can calculate a percentile.
    sql::Statement origin_statement(DB()->GetCachedStatement(
        SQL_FROM_HERE, "SELECT COUNT(id) FROM origin"));

    while (origin_statement.Step()) {
      origin_count = origin_statement.ColumnDouble(0);
      rank = *origin_count;
    }

    DCHECK(origin_count.has_value());

    // For each different query combination we should have an assign statement
    // call that will generate a unique SQL_FROM_HERE value.
    if (request.audio_video_watchtime_min.has_value() &&
        request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.audio_video_watchtime_min.has_value()) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else if (request.filter_by_type) {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    } else {
      AssignStatement(&statement, DB(), SQL_FROM_HERE, sql);
    }

    // Now bind all the parameters to the query.
    int bind_index = 0;

    if (request.audio_video_watchtime_min.has_value()) {
      statement.BindInt64(bind_index++,
                          request.audio_video_watchtime_min->InSeconds());
    }

    if (request.filter_by_type.has_value()) {
      statement.BindInt64(bind_index++,
                          static_cast<int>(*request.filter_by_type));
    }
  } else if (request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::
                                 Type::kSelectedFeedsForFetch) {
    sql.push_back("FROM mediaFeed WHERE user_status = ?");

    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::JoinString(sql, " ").c_str()));

    statement.BindInt64(
        0, static_cast<int>(media_feeds::mojom::FeedUserStatus::kEnabled));
  } else {
    sql.push_back("FROM mediaFeed");

    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE, base::JoinString(sql, " ").c_str()));
  }

  while (statement.Step()) {
    rank--;

    // If there is no mediaFeed data then skip this.
    if (statement.GetColumnType(0) == sql::ColumnType::kNull)
      continue;

    auto feed = media_feeds::mojom::MediaFeed::New();
    feed->last_fetch_item_count = statement.ColumnInt64(8);
    feed->last_fetch_safe_item_count = statement.ColumnInt64(9);

    // If we are getting the top feeds for display then we should filter by
    // the number of fetched items or fetched safe search items.
    if (request.type == MediaHistoryKeyedService::GetMediaFeedsRequest::Type::
                            kTopFeedsForDisplay) {
      if (request.fetched_items_min_should_be_safe) {
        if (feed->last_fetch_safe_item_count < *request.fetched_items_min)
          continue;
      } else {
        if (feed->last_fetch_item_count < *request.fetched_items_min)
          continue;
      }
    }

    feed->user_status = static_cast<media_feeds::mojom::FeedUserStatus>(
        statement.ColumnInt64(4));
    feed->last_fetch_result =
        static_cast<media_feeds::mojom::FetchResult>(statement.ColumnInt64(5));
    feed->reset_reason =
        static_cast<media_feeds::mojom::ResetReason>(statement.ColumnInt64(15));
    feed->safe_search_result =
        static_cast<media_feeds::mojom::SafeSearchResult>(
            statement.ColumnInt64(18));

    if (!IsKnownEnumValue(feed->user_status)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadUserStatus);
      continue;
    }

    if (!IsKnownEnumValue(feed->last_fetch_result)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadFetchResult);
      continue;
    }

    if (!IsKnownEnumValue(feed->reset_reason)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadResetReason);
      continue;
    }

    if (!IsKnownEnumValue(feed->safe_search_result)) {
      base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                    FeedReadResult::kBadSafeSearchResult);
      continue;
    }

    if (statement.GetColumnType(12) == sql::ColumnType::kBlob) {
      media_feeds::ImageSet image_set;
      if (!GetProto(statement, 12, image_set)) {
        base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                      FeedReadResult::kBadLogo);

        continue;
      }

      feed->logos = media_feeds::ProtoToMediaImages(image_set, kMaxLogoCount);
    }

    base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                  FeedReadResult::kSuccess);

    feed->id = statement.ColumnInt64(0);
    feed->url = GURL(statement.ColumnString(1));
    feed->last_discovery_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromSeconds(statement.ColumnInt64(2)));

    if (statement.GetColumnType(3) == sql::ColumnType::kInteger) {
      feed->last_fetch_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSeconds(statement.ColumnInt64(3)));
    }

    feed->fetch_failed_count = statement.ColumnInt64(6);

    if (statement.GetColumnType(7) == sql::ColumnType::kInteger) {
      feed->last_fetch_time_not_cache_hit =
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromSeconds(statement.ColumnInt64(7)));
    }

    feed->last_fetch_play_next_count = statement.ColumnInt64(10);
    feed->last_fetch_content_types = statement.ColumnInt64(11);
    feed->display_name = statement.ColumnString(13);

    if (statement.GetColumnType(14) == sql::ColumnType::kInteger) {
      feed->last_display_time = base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromSeconds(statement.ColumnInt64(14)));
    }

    if (top_feeds && origin_count > 1) {
      feed->origin_audio_video_watchtime_percentile =
          (rank / (*origin_count - 1)) * 100;
    } else if (top_feeds) {
      DCHECK_EQ(1, *origin_count);
      feed->origin_audio_video_watchtime_percentile = 100;
    }

    if (statement.GetColumnType(16) == sql::ColumnType::kBlob) {
      media_feeds::UserIdentifier identifier;
      if (!GetProto(statement, 16, identifier)) {
        base::UmaHistogramEnumeration(kFeedReadResultHistogramName,
                                      FeedReadResult::kBadUserIdentifier);

        continue;
      }

      feed->user_identifier = media_feeds::mojom::UserIdentifier::New();
      feed->user_identifier->name = identifier.name();
      feed->user_identifier->email = identifier.email();

      auto image_url = GURL(identifier.image().url());

      if (image_url.is_valid())
        feed->user_identifier->image = ProtoToMediaImage(identifier.image());
    }

    if (statement.GetColumnType(17) == sql::ColumnType::kText)
      feed->cookie_name_filter = statement.ColumnString(17);

    if (statement.GetColumnType(19) == sql::ColumnType::kBlob) {
      media_feeds::FeedResetToken token;
      if (GetProto(statement, 19, token))
        feed->reset_token = ProtoToUnguessableToken(token);
    }

    if (top_feeds) {
      feed->aggregate_watchtime =
          base::TimeDelta::FromSeconds(statement.ColumnInt64(20));
    }

    feeds.push_back(std::move(feed));

    // If we are returning top feeds then we should apply a limit here.
    if (top_feeds && feeds.size() >= *request.limit)
      break;
  }

  DCHECK(statement.Succeeded());
  return feeds;
}

bool MediaHistoryFeedsTable::UpdateFeedFromFetch(
    const int64_t feed_id,
    const media_feeds::mojom::FetchResult result,
    const bool was_fetched_from_cache,
    const int item_count,
    const int item_play_next_count,
    const int item_content_types,
    const std::vector<media_feeds::mojom::MediaImagePtr>& logos,
    const media_feeds::mojom::UserIdentifier* user_identifier,
    const std::string& display_name,
    const int item_safe_count,
    const std::string& cookie_name_filter) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  int fetch_failed_count = 0;

  {
    if (result != media_feeds::mojom::FetchResult::kSuccess) {
      // See how many times we have failed to fetch the feed.
      sql::Statement statement(DB()->GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT fetch_failed_count FROM mediaFeed WHERE id = ?"));
      statement.BindInt64(0, feed_id);

      while (statement.Step()) {
        DCHECK(!fetch_failed_count);
        fetch_failed_count = statement.ColumnInt64(0) + 1;
      }
    }
  }

  sql::Statement statement;
  if (was_fetched_from_cache) {
    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_fetch_time_s = ?, last_fetch_result = ?, "
        "fetch_failed_count = ?, last_fetch_item_count = ?, "
        "last_fetch_play_next_count = ?, last_fetch_content_types = ?, "
        "logo = ?, display_name = ?, last_fetch_safe_item_count = ?, "
        "user_identifier = ?, cookie_name_filter = ? WHERE id = ?"));
  } else {
    statement.Assign(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE mediaFeed SET last_fetch_time_s = ?, last_fetch_result = ?, "
        "fetch_failed_count = ?, last_fetch_item_count = ?, "
        "last_fetch_play_next_count = ?, last_fetch_content_types = ?, "
        "logo = ?, display_name = ?, last_fetch_safe_item_count = ?, "
        "user_identifier = ?, cookie_name_filter = ?, "
        "last_fetch_time_not_cache_hit_s = ? "
        "WHERE id = ?"));
  }

  statement.BindInt64(0,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(1, static_cast<int>(result));
  statement.BindInt64(2, fetch_failed_count);
  statement.BindInt64(3, item_count);
  statement.BindInt64(4, item_play_next_count);
  statement.BindInt64(5, item_content_types);

  if (!logos.empty()) {
    BindProto(statement, 6,
              media_feeds::MediaImagesToProto(logos, kMaxLogoCount));
  } else {
    statement.BindNull(6);
  }

  statement.BindString(7, display_name);
  statement.BindInt64(8, item_safe_count);

  if (user_identifier) {
    media_feeds::UserIdentifier proto_id;
    proto_id.set_name(user_identifier->name);
    if (user_identifier->email.has_value())
      proto_id.set_email(user_identifier->email.value());

    media_feeds::MediaImageToProto(proto_id.mutable_image(),
                                   user_identifier->image);

    BindProto(statement, 9, proto_id);
  } else {
    statement.BindNull(9);
  }

  if (!cookie_name_filter.empty()) {
    statement.BindString(10, cookie_name_filter);
  } else {
    statement.BindNull(10);
  }

  if (was_fetched_from_cache) {
    statement.BindInt64(11, feed_id);
  } else {
    statement.BindInt64(
        11, base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
    statement.BindInt64(12, feed_id);
  }

  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedsTable::UpdateDisplayTime(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET last_display_time_s = ? WHERE id = ?"));

  statement.BindInt64(0,
                      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());
  statement.BindInt64(1, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedsTable::RecalculateSafeSearchItemCount(
    const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET last_fetch_safe_item_count = (SELECT COUNT(id) "
      "FROM mediaFeedItem WHERE safe_search_result = ? AND feed_id = ?) WHERE "
      "id = ?"));
  statement.BindInt64(
      0, static_cast<int>(media_feeds::mojom::SafeSearchResult::kSafe));
  statement.BindInt64(1, feed_id);
  statement.BindInt64(2, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

bool MediaHistoryFeedsTable::Reset(
    const int64_t feed_id,
    const media_feeds::mojom::ResetReason reason) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return false;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET last_fetch_time_s = NULL, last_fetch_result = 0, "
      "fetch_failed_count = 0, last_fetch_time_not_cache_hit_s = NULL, "
      "last_fetch_item_count = 0, last_fetch_safe_item_count = 0, "
      "last_fetch_play_next_count = 0, last_fetch_content_types = 0, "
      "logo = NULL, display_name = NULL, user_identifier = NULL, "
      "reset_reason = ?, reset_token = ? WHERE id = ?"));

  statement.BindInt64(0, static_cast<int>(reason));

  // Store a new feed reset token to invalidate any fetches.
  auto token = base::UnguessableToken::Create();
  media_feeds::FeedResetToken proto_token;
  proto_token.set_high(token.GetHighForSerialization());
  proto_token.set_low(token.GetLowForSerialization());
  BindProto(statement, 1, proto_token);

  statement.BindInt64(2, feed_id);

  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

base::Optional<MediaHistoryKeyedService::MediaFeedFetchDetails>
MediaHistoryFeedsTable::GetFetchDetails(const int64_t feed_id) {
  if (!CanAccessDatabase())
    return base::nullopt;

  sql::Statement statement(
      DB()->GetCachedStatement(SQL_FROM_HERE,
                               "SELECT url, last_fetch_result, reset_token "
                               "FROM mediaFeed WHERE id = ?"));
  statement.BindInt64(0, feed_id);

  while (statement.Step()) {
    MediaHistoryKeyedService::MediaFeedFetchDetails details;
    details.url = GURL(statement.ColumnString(0));

    if (!details.url.is_valid())
      return base::nullopt;

    details.last_fetch_result =
        static_cast<media_feeds::mojom::FetchResult>(statement.ColumnInt64(1));
    if (!IsKnownEnumValue(details.last_fetch_result))
      return base::nullopt;

    if (statement.GetColumnType(2) == sql::ColumnType::kBlob) {
      media_feeds::FeedResetToken token;
      if (!GetProto(statement, 2, token))
        return base::nullopt;
      details.reset_token = ProtoToUnguessableToken(token);
    }

    return details;
  }

  return base::nullopt;
}

bool MediaHistoryFeedsTable::Delete(const int64_t feed_id) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM mediaFeed WHERE id = ?"));
  statement.BindInt64(0, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() >= 1;
}

bool MediaHistoryFeedsTable::ClearResetReason(const int64_t feed_id) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE mediaFeed SET reset_reason = ? WHERE id = ?"));
  statement.BindInt64(0,
                      static_cast<int>(media_feeds::mojom::ResetReason::kNone));
  statement.BindInt64(1, feed_id);
  return statement.Run() && DB()->GetLastChangeCount() == 1;
}

std::string MediaHistoryFeedsTable::GetCookieNameFilter(const int64_t feed_id) {
  DCHECK_LT(0, DB()->transaction_nesting());
  if (!CanAccessDatabase())
    return std::string();

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "SELECT cookie_name_filter FROM mediaFeed WHERE id = ?"));
  statement.BindInt64(0, feed_id);

  while (statement.Step())
    return statement.ColumnString(0);

  return std::string();
}

std::set<int64_t> MediaHistoryFeedsTable::GetFeedsForOriginSubdomain(
    const url::Origin& origin) {
  std::set<int64_t> feeds;
  if (!CanAccessDatabase())
    return feeds;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, url FROM mediaFeed WHERE url LIKE ? AND (last_fetch_result > "
      "0 OR reset_reason > 0)"));

  std::vector<std::string> wildcard_parts = base::SplitString(
      MediaHistoryOriginTable::GetOriginForStorage(origin),
      url::kStandardSchemeSeparator, base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  if (wildcard_parts.size() != 2)
    return feeds;

  statement.BindString(
      0, base::StrCat({wildcard_parts[0], url::kStandardSchemeSeparator, "%.",
                       wildcard_parts[1], "/%"}));

  while (statement.Step()) {
    // This shouldn't happen but is a backup so we don't accidentally reset
    // feeds that we should not.
    auto url = GURL(statement.ColumnString(1));
    if (!url.DomainIs(origin.host()))
      continue;

    feeds.insert(statement.ColumnInt64(0));
  }

  return feeds;
}

base::Optional<int64_t> MediaHistoryFeedsTable::GetFeedForOrigin(
    const url::Origin& origin) {
  if (!CanAccessDatabase())
    return base::nullopt;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT mediaFeed.id FROM origin LEFT JOIN mediaFeed ON "
      "mediaFeed.origin_id = origin.id WHERE origin.origin = ? AND "
      "(mediaFeed.last_fetch_result > 0 OR mediaFeed.reset_reason > 0)"));
  statement.BindString(0, MediaHistoryOriginTable::GetOriginForStorage(origin));

  while (statement.Step())
    return statement.ColumnInt64(0);

  return base::nullopt;
}

MediaHistoryKeyedService::PendingSafeSearchCheckList
MediaHistoryFeedsTable::GetPendingSafeSearchCheckItems() {
  MediaHistoryKeyedService::PendingSafeSearchCheckList items;

  if (!CanAccessDatabase())
    return items;

  sql::Statement statement(DB()->GetUniqueStatement(
      "SELECT id, url FROM mediaFeed WHERE safe_search_result = ?"));
  statement.BindInt64(
      0, static_cast<int>(media_feeds::mojom::SafeSearchResult::kUnknown));

  DCHECK(statement.is_valid());

  while (statement.Step()) {
    auto check =
        std::make_unique<MediaHistoryKeyedService::PendingSafeSearchCheck>(
            MediaHistoryKeyedService::SafeSearchCheckedType::kFeed,
            statement.ColumnInt64(0));

    GURL url(statement.ColumnString(1));
    if (url.is_valid())
      check->urls.insert(url);

    if (!check->urls.empty())
      items.push_back(std::move(check));
  }

  return items;
}

bool MediaHistoryFeedsTable::StoreSafeSearchResult(
    int64_t feed_id,
    media_feeds::mojom::SafeSearchResult result) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE mediaFeed SET safe_search_result = ? WHERE id = ?"));
  statement.BindInt64(0, static_cast<int>(result));
  statement.BindInt64(1, feed_id);
  return statement.Run();
}

bool MediaHistoryFeedsTable::UpdateFeedUserStatus(
    const int64_t feed_id,
    media_feeds::mojom::FeedUserStatus status) {
  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "UPDATE mediaFeed SET user_status = ? WHERE id = ?"));
  statement.BindInt64(0, static_cast<int>(status));
  statement.BindInt64(1, feed_id);
  return statement.Run();
}

}  // namespace media_history
