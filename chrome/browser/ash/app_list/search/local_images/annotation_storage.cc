// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_images/sql_database.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace app_list {
namespace {

using TableColumnName = ::app_list::AnnotationStorage::TableColumnName;

constexpr double kRelevanceThreshold = 0.6;
constexpr int kVersionNumber = 2;

// Initializes a new annotation table, returning a schema version number
// on success. The table can be searched by label and image path.
// The map between label and image is many-to-one.
// The table cannot exist when calling this function.
int CreateNewSchema(SqlDatabase* db) {
  DVLOG(1) << "Making a table";

  static constexpr char kQuery[] =
      // clang-format off
      "CREATE TABLE annotations("
          "label TEXT NOT NULL,"
          "image_path TEXT NOT NULL,"
          "last_modified_time INTEGER NOT NULL)";
  // clang-format on
  sql::Statement statement = db->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement.Run()) {
    return 0;
  }

  static constexpr char kQuery1[] =
      "CREATE INDEX ind_annotations_label ON annotations(label)";

  sql::Statement statement1 = db->GetStatementForQuery(SQL_FROM_HERE, kQuery1);
  if (!statement1.Run()) {
    return 0;
  }

  static constexpr char kQuery2[] =
      "CREATE INDEX ind_annotations_image_path ON annotations(image_path)";

  sql::Statement statement2 = db->GetStatementForQuery(SQL_FROM_HERE, kQuery2);
  if (!statement2.Run()) {
    return 0;
  }

  return kVersionNumber;
}

int MigrateSchema(SqlDatabase* db, int current_version_number) {
  return current_version_number;
}

}  // namespace

ImageInfo::ImageInfo(const std::set<std::string>& annotations,
                     const base::FilePath& path,
                     const base::Time& last_modified)
    : annotations(annotations), path(path), last_modified(last_modified) {}

ImageInfo::~ImageInfo() = default;
ImageInfo::ImageInfo(const ImageInfo&) = default;

FileSearchResult::FileSearchResult(const base::FilePath& path,
                                   const base::Time& last_modified,
                                   double relevance)
    : path(path), last_modified(last_modified), relevance(relevance) {}

FileSearchResult::~FileSearchResult() = default;
FileSearchResult::FileSearchResult(const FileSearchResult&) = default;

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path_to_db,
    const std::string& histogram_tag,
    int current_version_number,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : annotation_worker_(std::move(annotation_worker)),
      sql_database_(
          std::make_unique<SqlDatabase>(path_to_db,
                                        histogram_tag,
                                        current_version_number,
                                        base::BindRepeating(CreateNewSchema),
                                        base::BindRepeating(MigrateSchema))) {
  DVLOG(1) << "Construct AnnotationStorage";
}

AnnotationStorage::~AnnotationStorage() = default;

void AnnotationStorage::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sql_database_->Initialize()) {
    LOG(ERROR) << "Failed to initialize the db.";
    return;
  }
  if (annotation_worker_ != nullptr) {
    // Owns `annotation_worker_`.
    annotation_worker_->Initialize(this);
  }
}

void AnnotationStorage::Insert(const ImageInfo& image_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Insert";

  static constexpr char kQuery[] =
      // clang-format off
      "INSERT INTO annotations(label,image_path,last_modified_time) "
          "VALUES(?,?,?)";
  // clang-format on

  for (const auto& annotation : image_info.annotations) {
    sql::Statement statement =
        sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
    DVLOG(1) << annotation;
    statement.BindString(0, annotation);
    statement.BindString(1, image_info.path.value());
    statement.BindTime(2, image_info.last_modified);

    if (!statement.Run()) {
      // TODO(b/260646344): log to UMA instead.
      return;
    }
  }
  return;
}

void AnnotationStorage::Remove(const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Remove";

  static constexpr char kQuery[] = "DELETE FROM annotations WHERE image_path=?";

  sql::Statement statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  statement.BindString(0, image_path.value());

  statement.Run();
}

std::vector<ImageInfo> AnnotationStorage::GetAllAnnotations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "GetAllAnnotations";

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time "
          "FROM annotations "
          "ORDER BY label";
  // clang-format on

  sql::Statement statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);

  std::vector<ImageInfo> matched_paths;
  while (statement.Step()) {
    const base::FilePath path = base::FilePath(statement.ColumnString(1));
    const base::Time time = statement.ColumnTime(2);
    DVLOG(1) << "Select find: " << statement.ColumnString(0) << ", " << path
             << ", " << time;
    matched_paths.push_back(
        {{statement.ColumnString(0)}, std::move(path), std::move(time)});
  }

  return matched_paths;
}

std::vector<ImageInfo> AnnotationStorage::FindImagePath(
    const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image_path.empty());
  DVLOG(1) << "FindImagePath " << image_path;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time "
          "FROM annotations "
          "WHERE image_path=? "
          "ORDER BY label";
  // clang-format on

  sql::Statement statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  statement.BindString(0, image_path.value());

  std::vector<ImageInfo> matched_paths;
  while (statement.Step()) {
    const base::FilePath path = base::FilePath(statement.ColumnString(1));
    const base::Time time = statement.ColumnTime(2);
    DVLOG(1) << "Select find: " << statement.ColumnString(0) << ", " << path
             << ", " << time;
    matched_paths.push_back(
        {{statement.ColumnString(0)}, std::move(path), std::move(time)});
  }

  return matched_paths;
}

std::vector<FileSearchResult> AnnotationStorage::LinearSearchAnnotations(
    const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "LinearSearchAnnotationsAsync";
  using TokenizedString = ash::string_matching::TokenizedString;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT label,image_path,last_modified_time "
          "FROM annotations "
          "ORDER BY label";
  // clang-format on

  sql::Statement statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);

  std::vector<FileSearchResult> matched_paths;
  TokenizedString tokenized_query(query);
  ash::string_matching::FuzzyTokenizedStringMatch fuzzy_match;
  while (statement.Step()) {
    double relevance = fuzzy_match.Relevance(
        tokenized_query,
        TokenizedString(base::UTF8ToUTF16(statement.ColumnString(0))),
        /*use_weighted_ratio=*/true);
    if (relevance < kRelevanceThreshold) {
      continue;
    }

    const base::FilePath path = base::FilePath(statement.ColumnString(1));
    const base::Time time = statement.ColumnTime(2);
    DVLOG(1) << "Select: " << statement.ColumnString(0) << ", " << path << ", "
             << time << " rl: " << relevance;

    // TODO(b/260646344): keep only top N relevant paths.
    matched_paths.emplace_back(std::move(path), std::move(time), relevance);
  }
  return matched_paths;
}

}  // namespace app_list
