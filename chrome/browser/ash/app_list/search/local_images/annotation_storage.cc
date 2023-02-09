// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_provider.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace app_list {
namespace {

using TableColumnName = ::app_list::AnnotationStorage::TableColumnName;

constexpr char kTableName[] = "annotations";
constexpr char kColumnLabel[] = "label";
constexpr char kColumnImagePath[] = "image_path";
constexpr char kColumnLastModifiedTime[] = "last_modified_time";

constexpr const char* ColumnNameToString(TableColumnName column_name) {
  switch (column_name) {
    case TableColumnName::kLabel:
      return kColumnLabel;
    case TableColumnName::kImagePath:
      return kColumnImagePath;
    case TableColumnName::kLastModifiedTime:
      return kColumnLastModifiedTime;
  }
}

bool RunSqlQuery(sql::Database* const db,
                 const sql::StatementID& qld_from_here,
                 const std::string& query) {
  DVLOG(1) << "Query: " << query;
  DCHECK(db->IsSQLValid(query.c_str()));

  sql::Statement statement(
      db->GetCachedStatement(qld_from_here, query.c_str()));
  return statement.Run();
}

// Initializes a new annotation table, returning true on success.
// The table can be searched by label and image path.
// The map between label and image is many-to-one.
// The table cannot exist when calling this function.
bool CreateNewV1Schema(sql::Database* db) {
  DCHECK(!db->DoesTableExist(kTableName));

  if (!RunSqlQuery(db, SQL_FROM_HERE,
                   base::StrCat({
                       // clang-format off
            "CREATE TABLE ", kTableName, "(",
              kColumnLabel, " TEXT NOT NULL,",
              kColumnImagePath, " TEXT NOT NULL,",
              kColumnLastModifiedTime, " INTEGER NOT NULL)"
                       // clang-format on
                   }))) {
    return false;
  }

  if (!RunSqlQuery(db, SQL_FROM_HERE,
                   base::StrCat({
                       // clang-format off
                     "CREATE INDEX ind_annotations_label ON ",
                                 kTableName, "(", kColumnLabel, ")"
                       // clang-format on
                   }))) {
    return false;
  }

  if (!RunSqlQuery(db, SQL_FROM_HERE,
                   base::StrCat({
                       // clang-format off
                     "CREATE INDEX ind_annotations_image_path ON ",
                                 kTableName, "(", kColumnImagePath, ");"
                       // clang-format on
                   }))) {
    return false;
  }

  return true;
}

sql::StatementID GetFromHere(absl::optional<TableColumnName> column_name) {
  if (!column_name.has_value()) {
    return SQL_FROM_HERE;
  }

  switch (column_name.value()) {
    case TableColumnName::kLabel:
      return SQL_FROM_HERE;
    case TableColumnName::kImagePath:
      return SQL_FROM_HERE;
    case TableColumnName::kLastModifiedTime:
      return SQL_FROM_HERE;
  }
}

}  // namespace

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path,
    const std::string& histogram_tag,
    int current_version_number,
    int compatible_version_number,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : net::SQLitePersistentStoreBackendBase(
          path,
          histogram_tag,
          current_version_number,
          compatible_version_number,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
          nullptr),
      annotation_worker_(std::move(annotation_worker)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Construct AnnotationStorage";
}

AnnotationStorage::~AnnotationStorage() {
  annotation_worker_.reset();
}

bool AnnotationStorage::InitializeAsync() {
  return background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&AnnotationStorage::InitializeDatabase, this),
      base::BindOnce(&AnnotationStorage::OnInitializationComplete, this));
}

void AnnotationStorage::OnInitializationComplete(bool status) {
  if (!status) {
    DVLOG(1) << "Initialized with an error";
    return;
  }
  if (annotation_worker_ != nullptr) {
    annotation_worker_->Run(this);
  }
}

bool AnnotationStorage::CreateDatabaseSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db());

  if (db()->DoesTableExist(kTableName)) {
    return true;
  }

  DVLOG(1) << "Making a table";
  return CreateNewV1Schema(db());
}

bool AnnotationStorage::InsertOrReplaceAsync(ImageInfo image_info) {
  DVLOG(1) << "InsertOrReplaceAsync";
  return background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&AnnotationStorage::InsertOnBackgroundSequence),
          this, std::move(image_info)));
}

bool AnnotationStorage::InsertOnBackgroundSequence(ImageInfo image_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db()->DoesTableExist(kTableName));

  const std::string kQuery = base::StrCat({
      // clang-format off
        "INSERT OR REPLACE INTO ", kTableName, " (",
          kColumnLabel, ",", kColumnImagePath, ",",
          kColumnLastModifiedTime, ") ",
          "VALUES(?,?,?)"
      // clang-format on
  });
  DVLOG(1) << "Query: " << kQuery;
  DCHECK(db()->IsSQLValid(kQuery.c_str()));

  for (const auto& annotation : image_info.annotations) {
    sql::Statement statement(
        db()->GetCachedStatement(SQL_FROM_HERE, kQuery.c_str()));
    statement.BindString(0, annotation);
    DVLOG(1) << annotation;
    statement.BindString(1, image_info.path.value());
    statement.BindTime(2, image_info.last_modified);

    if (!statement.Run()) {
      return false;
    }
  }
  return true;
}

bool AnnotationStorage::RemoveAsync(base::FilePath image_path) {
  DVLOG(1) << "RemoveAsync";
  return background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&AnnotationStorage::RemoveOnBackgroundSequence),
          this, std::move(image_path)));
}

bool AnnotationStorage::RemoveOnBackgroundSequence(base::FilePath image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db()->DoesTableExist(kTableName));

  const std::string kQuery = base::StrCat({
      // clang-format off
        "DELETE FROM ", kTableName,
          " WHERE ", kColumnImagePath, "=?"
      // clang-format on
  });
  DVLOG(1) << "Query: " << kQuery;
  DCHECK(db()->IsSQLValid(kQuery.c_str()));

  sql::Statement statement(
      db()->GetCachedStatement(SQL_FROM_HERE, kQuery.c_str()));
  statement.BindString(0, image_path.value());

  return statement.Run();
}

bool AnnotationStorage::GetAllAnnotationsAsync(
    base::OnceCallback<void(std::vector<ImageInfo>)> callback) {
  DVLOG(1) << "GetAllAnnotationsAsync";
  return background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AnnotationStorage::FindAnnotationsOnBackgroundSequence,
                     this, absl::nullopt, absl::nullopt),
      std::move(callback));
}

bool AnnotationStorage::FindImagePathAsync(
    base::FilePath image_path,
    base::OnceCallback<void(std::vector<ImageInfo>)> callback) {
  DVLOG(1) << "FindImagePathAsync " << image_path;
  return background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AnnotationStorage::FindAnnotationsOnBackgroundSequence,
                     this, TableColumnName::kImagePath, image_path.value()),
      std::move(callback));
}

std::vector<ImageInfo> AnnotationStorage::FindAnnotationsOnBackgroundSequence(
    absl::optional<TableColumnName> column_name,
    absl::optional<std::string> value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db()->DoesTableExist(kTableName));
  DCHECK_EQ(column_name.has_value(), value.has_value());

  std::string kQuery = base::StrCat({
      // clang-format off
        "SELECT ", kColumnLabel, ",", kColumnImagePath, ",",
          kColumnLastModifiedTime,
          " FROM ", kTableName
      // clang-format on
  });
  if (column_name.has_value() && value.has_value()) {
    kQuery = base::StrCat({
        kQuery,
        // clang-format off
       " WHERE ", ColumnNameToString(column_name.value()), "=?"
        // clang-format on
    });
  }
  DVLOG(1) << kQuery;
  DCHECK(db()->IsSQLValid(kQuery.c_str()));

  sql::Statement statement(
      db()->GetCachedStatement(GetFromHere(column_name), kQuery.c_str()));
  if (column_name.has_value() && value.has_value()) {
    statement.BindString(0, value.value());
  }

  std::vector<ImageInfo> matched_paths;
  while (statement.Step()) {
    DVLOG(1) << "Select find: " << statement.ColumnString(0) << ", "
             << statement.ColumnString(1) << ", " << statement.ColumnTime(2);
    matched_paths.push_back({{statement.ColumnString(0)},
                             base::FilePath(statement.ColumnString(1)),
                             statement.ColumnTime(2),
                             /*relevance*/ 1});
  }
  return matched_paths;
}

bool AnnotationStorage::LinearSearchAnnotationsAsync(
    std::u16string query,
    base::OnceCallback<void(std::vector<ImageInfo>)> callback) {
  DVLOG(1) << "LinearSearchAnnotationsAsync";
  return background_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &AnnotationStorage::LinearSearchAnnotationsOnBackgroundSequence, this,
          std::move(query)),
      std::move(callback));
}

std::vector<ImageInfo>
AnnotationStorage::LinearSearchAnnotationsOnBackgroundSequence(
    std::u16string query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db()->DoesTableExist(kTableName));
  using TokenizedString = ash::string_matching::TokenizedString;

  const std::string kQuery = base::StrCat({
      // clang-format off
    "SELECT ", kColumnLabel, ",", kColumnImagePath, ",",
      kColumnLastModifiedTime,
      " FROM ", kTableName
      // clang-format on
  });

  DVLOG(1) << kQuery;
  DCHECK(db()->IsSQLValid(kQuery.c_str()));

  sql::Statement statement(
      db()->GetCachedStatement(SQL_FROM_HERE, kQuery.c_str()));

  std::vector<ImageInfo> matched_paths;
  TokenizedString tokenized_query(query);
  ash::string_matching::FuzzyTokenizedStringMatch fuzzy_match;
  while (statement.Step()) {
    double relevance = fuzzy_match.Relevance(
        tokenized_query,
        TokenizedString(base::UTF8ToUTF16(statement.ColumnString(0))),
        /*use_weighted_ratio=*/true);

    DVLOG(1) << "Select: " << statement.ColumnString(0) << ", "
             << statement.ColumnString(1) << ", " << statement.ColumnTime(2)
             << " rl: " << relevance;

    // TODO(b/260646344): keep only top N relevant paths.
    matched_paths.push_back({{statement.ColumnString(0)},
                             base::FilePath(statement.ColumnString(1)),
                             statement.ColumnTime(2),
                             relevance});
  }
  return matched_paths;
}

absl::optional<int> AnnotationStorage::DoMigrateDatabaseSchema() {
  return 0;
}

void AnnotationStorage::DoCommit() {}

}  // namespace app_list
