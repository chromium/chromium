// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_store.h"

#include <tuple>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media/history/media_history_images_table.h"
#include "chrome/browser/media/history/media_history_origin_table.h"
#include "chrome/browser/media/history/media_history_playback_table.h"
#include "chrome/browser/media/history/media_history_session_images_table.h"
#include "chrome/browser/media/history/media_history_session_table.h"
#include "content/public/browser/media_player_watch_time.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_position.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace {

constexpr int kCurrentVersionNumber = 6;
constexpr int kCompatibleVersionNumber = 1;

constexpr base::FilePath::CharType kMediaHistoryDatabaseName[] =
    FILE_PATH_LITERAL("Media History");

void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

base::FilePath GetDBPath(Profile* profile) {
  // If this is a testing profile then we should use an in-memory database.
  if (profile->AsTestingProfile())
    return base::FilePath();
  return profile->GetPath().Append(kMediaHistoryDatabaseName);
}

int MigrateFrom1To2(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 2 adds a new column to mediaFeed. However, a later version removes
  // the mediaFeed table, so just bump the version number and return 2 to
  // indicate success.
  const int kTargetVersion = 2;
  meta_table->SetVersionNumber(kTargetVersion);
  return kTargetVersion;
}

int MigrateFrom2To3(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 3 drops the mediaFeedAssociatedOrigin table.
  const int kTargetVersion = 3;

  static const char k2To3Sql[] =
      "DROP TABLE IF EXISTS mediaFeedAssociatedOrigin;";
  sql::Transaction transaction(db);
  if (transaction.Begin() && db->Execute(k2To3Sql) && transaction.Commit()) {
    meta_table->SetVersionNumber(kTargetVersion);
    return kTargetVersion;
  }
  return 2;
}

int MigrateFrom3To4(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 4 adds a new column to mediaFeed. However, a later version removes
  // the mediaFeed table, so just bump the version number and return 4 to
  // indicate success.
  const int kTargetVersion = 4;
  meta_table->SetVersionNumber(kTargetVersion);
  return kTargetVersion;
}

int MigrateFrom4To5(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 5 adds a new column to mediaFeed. However, a later version removes
  // the mediaFeed table, so just bump the version number and return 5 to
  // indicate success.
  const int kTargetVersion = 5;
  meta_table->SetVersionNumber(kTargetVersion);
  return kTargetVersion;
}

int MigrateFrom5To6(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 6 drops the mediaFeed and mediaFeedItem tables.
  const int kTargetVersion = 6;

  sql::Transaction transaction(db);
  if (!transaction.Begin() || !db->Execute("DROP TABLE IF EXISTS mediaFeed") ||
      !db->Execute("DROP TABLE IF EXISTS mediaFeedItem") ||
      !transaction.Commit()) {
    return 5;
  }

  meta_table->SetVersionNumber(kTargetVersion);
  return kTargetVersion;
}

}  // namespace

int GetCurrentVersion() {
  return kCurrentVersionNumber;
}

namespace media_history {

MediaHistoryStore::MediaHistoryStore(
    Profile* profile,
    scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner)
    : db_task_runner_(db_task_runner),
      db_path_(GetDBPath(profile)),
      db_(std::make_unique<sql::Database>(
          sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 500})),
      meta_table_(std::make_unique<sql::MetaTable>()),
      origin_table_(new MediaHistoryOriginTable(db_task_runner_)),
      playback_table_(new MediaHistoryPlaybackTable(db_task_runner_)),
      session_table_(new MediaHistorySessionTable(db_task_runner_)),
      session_images_table_(
          new MediaHistorySessionImagesTable(db_task_runner_)),
      images_table_(new MediaHistoryImagesTable(db_task_runner_)),
      initialization_successful_(false) {
  db_->set_histogram_tag("MediaHistory");

  // To recover from corruption.
  db_->set_error_callback(
      base::BindRepeating(&DatabaseErrorCallback, db_.get(), db_path_));
}

MediaHistoryStore::~MediaHistoryStore() {
  // The connection pointer needs to be deleted on the DB sequence since there
  // might be a task in progress on the DB sequence which uses this connection.
  if (meta_table_)
    db_task_runner_->DeleteSoon(FROM_HERE, meta_table_.release());
  if (db_)
    db_task_runner_->DeleteSoon(FROM_HERE, db_.release());
}

sql::Database* MediaHistoryStore::DB() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return db_.get();
}

void MediaHistoryStore::SavePlayback(
    std::unique_ptr<content::MediaPlayerWatchTime> watch_time) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  // TODO(https://crbug.com/1052436): Remove the separate origin.
  auto origin = url::Origin::Create(watch_time->origin);
  if (origin != url::Origin::Create(watch_time->url)) {
    return;
  }

  if (!CreateOriginId(origin)) {
    return;
  }

  if (!playback_table_->SavePlayback(*watch_time)) {
    return;
  }

  if (watch_time->has_audio && watch_time->has_video) {
    if (!origin_table_->IncrementAggregateAudioVideoWatchTime(
            origin, watch_time->cumulative_watch_time)) {
      return;
    }
  }

  transaction.Commit();
}

void MediaHistoryStore::Initialize(const bool should_reset) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (should_reset) {
    if (!sql::Database::Delete(db_path_)) {
      LOG(ERROR) << "Failed to delete the old database.";
      return;
    }
  }

  if (IsCancelled())
    return;

  bool result = InitializeInternal();

  if (IsCancelled()) {
    meta_table_.reset();
    db_.reset();
    return;
  }

  // In some edge cases the DB might be corrupted and unrecoverable so we should
  // delete the database and recreate it.
  if (!result) {
    db_ = std::make_unique<sql::Database>();
    meta_table_ = std::make_unique<sql::MetaTable>();

    sql::Database::Delete(db_path_);
  }
}

bool MediaHistoryStore::InitializeInternal() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (db_path_.empty()) {
    if (IsCancelled() || !db_ || !db_->OpenInMemory()) {
      LOG(ERROR) << "Failed to open the in-memory database.";

      return false;
    }
  } else {
    base::File::Error err;
    if (IsCancelled() ||
        !base::CreateDirectoryAndGetError(db_path_.DirName(), &err)) {
      LOG(ERROR) << "Failed to create the directory.";

      return false;
    }

    if (IsCancelled() || !db_ || !db_->Open(db_path_)) {
      LOG(ERROR) << "Failed to open the database.";

      return false;
    }
  }

  if (IsCancelled() || !db_ || !db_->Execute("PRAGMA foreign_keys=1")) {
    LOG(ERROR) << "Failed to enable foreign keys on the media history store.";

    return false;
  }

  if (IsCancelled() || !db_ || !meta_table_ ||
      !meta_table_->Init(db_.get(), GetCurrentVersion(),
                         kCompatibleVersionNumber)) {
    LOG(ERROR) << "Failed to create the meta table.";

    return false;
  }

  if (IsCancelled() || !db_) {
    LOG(ERROR) << "Failed to begin the transaction.";

    return false;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin the transaction.";

    return false;
  }

  sql::InitStatus status = CreateOrUpgradeIfNeeded();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to create or update the media history store.";

    return false;
  }

  status = InitializeTables();
  if (status != sql::INIT_OK) {
    LOG(ERROR) << "Failed to initialize the media history store tables.";

    return false;
  }

  if (IsCancelled() || !db_ || !transaction.Commit()) {
    LOG(ERROR) << "Failed to commit transaction.";

    return false;
  }

  initialization_successful_ = true;

  return true;
}

sql::InitStatus MediaHistoryStore::CreateOrUpgradeIfNeeded() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (IsCancelled() || !meta_table_)
    return sql::INIT_FAILURE;

  int cur_version = meta_table_->GetVersionNumber();
  if (meta_table_->GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Media history database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // Versions 0 and below are unexpected.
  if (cur_version <= 0)
    return sql::INIT_FAILURE;

  // NOTE: Insert schema upgrade scripts here when required.
  if (cur_version == 1)
    cur_version = MigrateFrom1To2(db_.get(), meta_table_.get());
  if (cur_version == 2)
    cur_version = MigrateFrom2To3(db_.get(), meta_table_.get());
  if (cur_version == 3)
    cur_version = MigrateFrom3To4(db_.get(), meta_table_.get());
  if (cur_version == 4)
    cur_version = MigrateFrom4To5(db_.get(), meta_table_.get());
  if (cur_version == 5)
    cur_version = MigrateFrom5To6(db_.get(), meta_table_.get());

  if (cur_version == kCurrentVersionNumber)
    return sql::INIT_OK;
  return sql::INIT_FAILURE;
}

sql::InitStatus MediaHistoryStore::InitializeTables() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (IsCancelled() || !db_)
    return sql::INIT_FAILURE;

  sql::InitStatus status = origin_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = playback_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = session_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = session_images_table_->Initialize(db_.get());
  if (status == sql::INIT_OK)
    status = images_table_->Initialize(db_.get());

  return status;
}

bool MediaHistoryStore::CreateOriginId(const url::Origin& origin) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return false;

  return origin_table_->CreateOriginId(origin);
}

mojom::MediaHistoryStatsPtr MediaHistoryStore::GetMediaHistoryStats() {
  mojom::MediaHistoryStatsPtr stats(mojom::MediaHistoryStats::New());

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return stats;

  sql::Statement statement(DB()->GetUniqueStatement(
      "SELECT name FROM sqlite_schema WHERE type='table' "
      "AND name NOT LIKE 'sqlite_%';"));

  std::vector<std::string> table_names;
  while (statement.Step()) {
    auto table_name = statement.ColumnString(0);
    stats->table_row_counts.emplace(table_name, GetTableRowCount(table_name));
  }

  DCHECK(statement.Succeeded());
  return stats;
}

std::vector<mojom::MediaHistoryOriginRowPtr>
MediaHistoryStore::GetOriginRowsForDebug() {
  std::vector<mojom::MediaHistoryOriginRowPtr> origins;
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return origins;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf(
          "SELECT O.origin, O.last_updated_time_s, "
          "O.aggregate_watchtime_audio_video_s,  "
          "(SELECT SUM(watch_time_s) FROM %s WHERE origin_id = O.id AND "
          "has_video = 1 AND has_audio = 1) AS accurate_watchtime "
          "FROM %s O",
          MediaHistoryPlaybackTable::kTableName,
          MediaHistoryOriginTable::kTableName)
          .c_str()));

  std::vector<std::string> table_names;
  while (statement.Step()) {
    mojom::MediaHistoryOriginRowPtr origin(mojom::MediaHistoryOriginRow::New());

    origin->origin = url::Origin::Create(GURL(statement.ColumnString(0)));
    origin->last_updated_time = base::Time::FromDeltaSinceWindowsEpoch(
                                    base::Seconds(statement.ColumnInt64(1)))
                                    .ToJsTime();
    origin->cached_audio_video_watchtime =
        base::Seconds(statement.ColumnInt64(2));
    origin->actual_audio_video_watchtime =
        base::Seconds(statement.ColumnInt64(3));

    origins.push_back(std::move(origin));
  }

  DCHECK(statement.Succeeded());
  return origins;
}

std::vector<url::Origin> MediaHistoryStore::GetHighWatchTimeOrigins(
    const base::TimeDelta& audio_video_watchtime_min) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  return origin_table_->GetHighWatchTimeOrigins(audio_video_watchtime_min);
}

std::vector<mojom::MediaHistoryPlaybackRowPtr>
MediaHistoryStore::GetMediaHistoryPlaybackRowsForDebug() {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return std::vector<mojom::MediaHistoryPlaybackRowPtr>();

  return playback_table_->GetPlaybackRows();
}

int MediaHistoryStore::GetTableRowCount(const std::string& table_name) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return -1;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT count(*) from %s", table_name.c_str())
          .c_str()));

  while (statement.Step()) {
    return statement.ColumnInt(0);
  }

  return -1;
}

void MediaHistoryStore::SavePlaybackSession(
    const GURL& url,
    const media_session::MediaMetadata& metadata,
    const absl::optional<media_session::MediaPosition>& position,
    const std::vector<media_session::MediaImage>& artwork) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  auto origin = url::Origin::Create(url);
  if (!CreateOriginId(origin)) {
    return;
  }

  auto session_id =
      session_table_->SavePlaybackSession(url, origin, metadata, position);
  if (!session_id) {
    return;
  }

  for (auto& image : artwork) {
    auto image_id =
        images_table_->SaveOrGetImage(image.src, origin, image.type);
    if (!image_id) {
      return;
    }

    // If we do not have any sizes associated with the image we should save a
    // link with a null size. Otherwise, we should save a link for each size.
    if (image.sizes.empty()) {
      session_images_table_->LinkImage(*session_id, *image_id, absl::nullopt);
    } else {
      for (auto& size : image.sizes) {
        session_images_table_->LinkImage(*session_id, *image_id, size);
      }
    }
  }

  transaction.Commit();
}

std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>
MediaHistoryStore::GetPlaybackSessions(
    absl::optional<unsigned int> num_sessions,
    absl::optional<MediaHistoryStore::GetPlaybackSessionsFilter> filter) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());

  if (!CanAccessDatabase())
    return std::vector<mojom::MediaHistoryPlaybackSessionRowPtr>();

  auto sessions =
      session_table_->GetPlaybackSessions(num_sessions, std::move(filter));

  for (auto& session : sessions) {
    session->artwork = session_images_table_->GetImagesForSession(session->id);
  }

  return sessions;
}

void MediaHistoryStore::DeleteAllOriginData(
    const std::set<url::Origin>& origins) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  for (auto& origin : origins) {
    if (!origin_table_->Delete(origin))
      return;
  }

  transaction.Commit();
}

void MediaHistoryStore::DeleteAllURLData(const std::set<GURL>& urls) {
  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin()) {
    LOG(ERROR) << "Failed to begin the transaction.";
    return;
  }

  MediaHistoryTableBase* tables[] = {
      playback_table_.get(),
      session_table_.get(),
  };

  std::set<url::Origin> origins_with_deletions;
  for (auto& url : urls) {
    origins_with_deletions.insert(url::Origin::Create(url));

    for (auto* table : tables) {
      if (!table->DeleteURL(url))
        return;
    }
  }

  for (auto& origin : origins_with_deletions) {
    if (!origin_table_->RecalculateAggregateAudioVideoWatchTime(origin))
      return;
  }

  // The mediaImages table will not be automatically cleared when we remove
  // single sessions so we should remove them manually.
  sql::Statement statement(DB()->GetUniqueStatement(
      "DELETE FROM mediaImage WHERE id IN ("
      "  SELECT id FROM mediaImage LEFT JOIN sessionImage"
      "  ON sessionImage.image_id = mediaImage.id"
      "  WHERE sessionImage.session_id IS NULL)"));

  if (statement.Run())
    transaction.Commit();
}

std::set<GURL> MediaHistoryStore::GetURLsInTableForTest(
    const std::string& table) {
  std::set<GURL> urls;

  DCHECK(db_task_runner_->RunsTasksInCurrentSequence());
  if (!CanAccessDatabase())
    return urls;

  sql::Statement statement(DB()->GetUniqueStatement(
      base::StringPrintf("SELECT url from %s", table.c_str()).c_str()));

  while (statement.Step()) {
    urls.insert(GURL(statement.ColumnString(0)));
  }

  DCHECK(statement.Succeeded());
  return urls;
}

void MediaHistoryStore::SetCancelled() {
  DCHECK(!db_task_runner_->RunsTasksInCurrentSequence());

  cancelled_.Set();

  MediaHistoryTableBase* tables[] = {
      origin_table_.get(),         playback_table_.get(), session_table_.get(),
      session_images_table_.get(), images_table_.get(),
  };

  for (auto* table : tables) {
    if (table)
      table->SetCancelled();
  }
}

bool MediaHistoryStore::CanAccessDatabase() const {
  return !IsCancelled() && initialization_successful_ && db_ && db_->is_open();
}

bool MediaHistoryStore::IsCancelled() const {
  return cancelled_.IsSet();
}

}  // namespace media_history
