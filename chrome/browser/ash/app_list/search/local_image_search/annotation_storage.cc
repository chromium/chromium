// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotations_table.h"
#include "chrome/browser/ash/app_list/search/local_image_search/documents_table.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"
#include "chrome/browser/ash/app_list/search/local_image_search/inverted_index_table.h"
#include "chrome/browser/ash/app_list/search/local_image_search/search_utils.h"
#include "chrome/browser/ash/app_list/search/local_image_search/sql_database.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "sql/statement.h"

namespace app_list {
namespace {

using FuzzyTokenizedStringMatch =
    ::ash::string_matching::FuzzyTokenizedStringMatch;
using TokenizedString = ::ash::string_matching::TokenizedString;
using Mode = ::ash::string_matching::TokenizedString::Mode;

constexpr double kRelevanceThreshold = 0.79;
// The default score of annotation if it's null it the database. The default
// score is used for ocr annotations, which do not have a score by default.
constexpr double kDefaultScore = 0.7;
// The weight of fuzzy match relevance, in range [0,1].
constexpr double kRelevanceWeight = 0.9;
constexpr int kVersionNumber = 6;

constexpr char kSqlDatabaseUmaTag[] =
    "Apps.AppList.AnnotationStorage.SqlDatabase.Status";

// These values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class ErrorStatus {
  kOk = 0,
  kFailedToCreateNewSchema = 1,
  kFailedToMigrateSchema = 2,
  kFailedToInitializeDb = 3,
  kFailedToInsertInDb = 4,
  kFailedToRemoveFromDb = 5,
  kFailedToGetAllFiles = 6,
  kFailedToSearchByDirectory = 7,
  kFailedToFindImagePath = 8,
  kFailedToPrefixSearch = 9,
  kMaxValue = kFailedToPrefixSearch,
};

double GetRelevanceThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      search_features::kLauncherLocalImageSearchRelevance,
      "relevance_threshold", kRelevanceThreshold);
}

void LogErrorUma(ErrorStatus status) {
  base::UmaHistogramEnumeration("Apps.AppList.AnnotationStorage.Status",
                                status);
}

// Initializes a new annotation table, returning a schema version number
// on success. The database implements inverted index.
// The table cannot exist when calling this function.
int CreateNewSchema(SqlDatabase* db) {
  DVLOG(1) << "Making a table";

  if (!db || !AnnotationsTable::Create(db) || !DocumentsTable::Create(db) ||
      !InvertedIndexTable::Create(db)) {
    LOG(ERROR) << "Failed to create schema.";
    LogErrorUma(ErrorStatus::kFailedToCreateNewSchema);
    return 0;
  }

  return kVersionNumber;
}

int MigrateSchema(SqlDatabase* db, int current_version_number) {
  if (current_version_number == kVersionNumber) {
    return current_version_number;
  }

  if (!db || !AnnotationsTable::Drop(db) || !DocumentsTable::Drop(db) ||
      !InvertedIndexTable::Drop(db)) {
    LOG(ERROR) << "Failed to drop schema.";
    LogErrorUma(ErrorStatus::kFailedToMigrateSchema);
    return 0;
  }

  return CreateNewSchema(db);
}

}  // namespace

ImageInfo::ImageInfo(const std::set<std::string>& annotations,
                     const base::FilePath& path,
                     const base::Time& last_modified,
                     int64_t file_size)
    : annotations(annotations),
      path(path),
      last_modified(last_modified),
      file_size(file_size) {}

ImageInfo::~ImageInfo() = default;
ImageInfo::ImageInfo(const ImageInfo&) = default;

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path_to_db,
    int current_version_number,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : annotation_worker_(std::move(annotation_worker)),
      sql_database_(
          std::make_unique<SqlDatabase>(path_to_db,
                                        kSqlDatabaseUmaTag,
                                        current_version_number,
                                        base::BindRepeating(CreateNewSchema),
                                        base::BindRepeating(MigrateSchema))) {
  DVLOG(1) << "Construct AnnotationStorage";
}

AnnotationStorage::AnnotationStorage(
    const base::FilePath& path_to_db,
    std::unique_ptr<ImageAnnotationWorker> annotation_worker)
    : AnnotationStorage(path_to_db,
                        kVersionNumber,
                        std::move(annotation_worker)) {}

AnnotationStorage::~AnnotationStorage() = default;

void AnnotationStorage::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sql_database_->Initialize()) {
    LOG(ERROR) << "Failed to initialize the db.";
    LogErrorUma(ErrorStatus::kFailedToInitializeDb);
    return;
  }
  if (annotation_worker_ != nullptr) {
    // Owns `annotation_worker_`.
    annotation_worker_->Initialize(this);
  }
  LogErrorUma(ErrorStatus::kOk);

  auto file_info = base::File::Info();
  if (base::GetFileInfo(sql_database_->GetPathToDb(), &file_info)) {
    base::UmaHistogramMemoryMB("Apps.AppList.AnnotationStorage.DatabaseSize",
                               file_info.size / 1024 / 1024);
  }
}

void AnnotationStorage::Insert(const ImageInfo& image_info,
                               IndexingSource indexing_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Insert " << image_info.path;

  int64_t document_id;
  if (!DocumentsTable::InsertOrIgnore(sql_database_.get(), image_info.path,
                                      image_info.last_modified,
                                      image_info.file_size) ||
      !DocumentsTable::GetDocumentId(sql_database_.get(), image_info.path,
                                     document_id)) {
    LOG(ERROR) << "Failed to insert into the db.";
    LogErrorUma(ErrorStatus::kFailedToInsertInDb);
    return;
  }

  // Inserts the annotations to the database and finally updates the document
  // table to indicate that the annotations from ICA/OCR have been saved to db.
  bool execution_succeed;
  switch (indexing_source) {
    case IndexingSource::kOcr:
      for (const auto& annotation : image_info.annotations) {
        DVLOG(1) << annotation;
        int64_t annotation_id;
        if (!AnnotationsTable::InsertOrIgnore(sql_database_.get(),
                                              annotation) ||
            !AnnotationsTable::GetTermId(sql_database_.get(), annotation,
                                         annotation_id) ||
            !InvertedIndexTable::Insert(sql_database_.get(), annotation_id,
                                        document_id, indexing_source)) {
          LOG(ERROR) << "Failed to insert into the db from OCR.";
          LogErrorUma(ErrorStatus::kFailedToInsertInDb);
          return;
        }
      }
      execution_succeed =
          DocumentsTable::UpdateOCRStatus(sql_database_.get(), document_id);
      break;
    case IndexingSource::kIca:
      for (const auto& [annotation, annotation_info] :
           image_info.annotation_map) {
        DVLOG(1) << annotation;
        int64_t annotation_id;
        if (!AnnotationsTable::InsertOrIgnore(sql_database_.get(),
                                              annotation) ||
            !AnnotationsTable::GetTermId(sql_database_.get(), annotation,
                                         annotation_id) ||
            !InvertedIndexTable::Insert(
                sql_database_.get(), annotation_id, document_id,
                indexing_source, annotation_info.score, annotation_info.x,
                annotation_info.y, annotation_info.area)) {
          LOG(ERROR) << "Failed to insert into the db from ICA.";
          LogErrorUma(ErrorStatus::kFailedToInsertInDb);
          return;
        }
      }
      execution_succeed =
          DocumentsTable::UpdateICAStatus(sql_database_.get(), document_id);
      break;
  }

  if (!execution_succeed) {
    LOG(ERROR) << "Failed to update the document table.";
    LogErrorUma(ErrorStatus::kFailedToInsertInDb);
  }
}

void AnnotationStorage::Remove(const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Remove " << image_path;

  if (!InvertedIndexTable::Remove(sql_database_.get(), image_path) ||
      !DocumentsTable::Remove(sql_database_.get(), image_path) ||
      !AnnotationsTable::Prune(sql_database_.get())) {
    LOG(ERROR) << "Failed to remove from the db.";
    LogErrorUma(ErrorStatus::kFailedToRemoveFromDb);
  }
}

std::vector<ImageInfo> AnnotationStorage::GetAllAnnotationsForTest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "GetAllAnnotations";

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT a.term, d.directory_path, d.file_name,"
          "d.last_modified_time, d.file_size, "
          "ii.source, ii.score, ii.x, ii.y, ii.area "
          "FROM annotations AS a "
          "JOIN inverted_index AS ii ON a.term_id = ii.term_id "
          "JOIN documents AS d ON ii.document_id = d.document_id "
          "ORDER BY a.term, d.directory_path, d.file_name";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    return {};
  }

  std::vector<ImageInfo> matched_paths;
  while (statement->Step()) {
    const std::string annotation = statement->ColumnString(0);
    base::FilePath file_path(statement->ColumnString(1));
    file_path = file_path.Append(statement->ColumnString(2));
    const base::Time time = statement->ColumnTime(3);
    const int64_t file_size = statement->ColumnInt64(4);
    DVLOG(1) << "Select find: " << annotation << ", " << file_path << ", "
             << time << ", " << file_size;
    ImageInfo image_info(
        {{}, std::move(file_path), std::move(time), file_size});

    const int source = statement->ColumnInt(5);
    if (source == 0) {  // OCR annotation.
      image_info.annotations.insert(annotation);
    } else if (source == 1) {  // ICA annotation.
      AnnotationInfo annotation_info;
      annotation_info.score = statement->ColumnDouble(6);
      if (statement->GetColumnType(7) != sql::ColumnType::kNull) {
        annotation_info.x = statement->ColumnDouble(7);
      }
      if (statement->GetColumnType(8) != sql::ColumnType::kNull) {
        annotation_info.y = statement->ColumnDouble(8);
      }
      if (statement->GetColumnType(9) != sql::ColumnType::kNull) {
        annotation_info.area = statement->ColumnDouble(9);
      }
      image_info.annotation_map[annotation] = annotation_info;
    }
    matched_paths.push_back(image_info);
  }

  return matched_paths;
}

std::vector<base::FilePath> AnnotationStorage::GetAllFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "GetAllFiles";

  std::vector<base::FilePath> documents;
  if (!DocumentsTable::GetAllFiles(sql_database_.get(), documents)) {
    LOG(ERROR) << "Failed to get file paths from the db.";
    LogErrorUma(ErrorStatus::kFailedToGetAllFiles);
    return {};
  }

  return documents;
}

std::vector<base::FilePath> AnnotationStorage::SearchByDirectory(
    const base::FilePath& directory) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "SearchByDirectory " << directory;

  std::vector<base::FilePath> files;
  if (!DocumentsTable::SearchByDirectory(sql_database_.get(), directory,
                                         files)) {
    LOG(ERROR) << "Failed to get file paths from the db.";
    LogErrorUma(ErrorStatus::kFailedToSearchByDirectory);
    return {};
  }

  return files;
}

std::vector<ImageInfo> AnnotationStorage::FindImagePath(
    const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image_path.empty());
  DVLOG(1) << "FindImagePath " << image_path;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT a.term, d.directory_path, d.file_name,"
      "d.last_modified_time, d.file_size "
          "FROM annotations AS a "
          "JOIN inverted_index AS ii ON a.term_id = ii.term_id "
          "JOIN documents AS d ON ii.document_id = d.document_id "
          "WHERE d.directory_path=? AND d.file_name=? "
          "ORDER BY a.term";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    LogErrorUma(ErrorStatus::kFailedToFindImagePath);
    return {};
  }
  // Safe on ChromeOS.
  statement->BindString(0, image_path.DirName().AsUTF8Unsafe());
  statement->BindString(1, image_path.BaseName().AsUTF8Unsafe());

  std::vector<ImageInfo> matched_paths;
  while (statement->Step()) {
    const std::string annotation = statement->ColumnString(0);
    const base::Time time = statement->ColumnTime(3);
    const int64_t file_size = statement->ColumnInt64(4);
    DVLOG(1) << "Select find: " << annotation << ", " << image_path << ", "
             << time << ", " << file_size;
    matched_paths.push_back({{std::move(annotation)},
                             std::move(image_path),
                             std::move(time),
                             file_size});
  }

  return matched_paths;
}

const ImageStatus AnnotationStorage::GetImageStatus(
    const base::FilePath& image_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (image_path.empty()) {
    return ImageStatus();
  }

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT last_modified_time, ocr_version, ica_version "
      "FROM documents "
      "WHERE directory_path=? AND file_name=?";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    LogErrorUma(ErrorStatus::kFailedToFindImagePath);
    return ImageStatus();
  }
  // Safe on ChromeOS.
  statement->BindString(0, image_path.DirName().AsUTF8Unsafe());
  statement->BindString(1, image_path.BaseName().AsUTF8Unsafe());

  // We only need the first row because (directory_path, file_name) is the
  // primary key of the image, which ensures the found result is unique.
  if (statement->Step()) {
    ImageStatus image_status;
    image_status.last_modified = statement->ColumnTime(0);
    image_status.ocr_version = statement->ColumnInt(1);
    image_status.ica_version = statement->ColumnInt(2);
    return image_status;
  }
  return ImageStatus();
}

std::vector<FileSearchResult> AnnotationStorage::PrefixSearch(
    const std::u16string& query_term) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "PrefixSearch " << query_term;

  static constexpr char kQuery[] =
      // clang-format off
      "SELECT a.term, d.directory_path, d.file_name, d.last_modified_time, "
          "MAX(IFNULL(ii.score,?)) "
          "FROM annotations AS a "
          "JOIN inverted_index AS ii ON a.term_id = ii.term_id "
          "JOIN documents AS d ON ii.document_id = d.document_id "
          "WHERE a.term LIKE ? "
          "GROUP BY a.term, d.directory_path, d.file_name "
          "ORDER BY d.directory_path, d.file_name "
          "LIMIT 1000";
  // clang-format on

  std::unique_ptr<sql::Statement> statement =
      sql_database_->GetStatementForQuery(SQL_FROM_HERE, kQuery);
  if (!statement) {
    LOG(ERROR) << "Couldn't create the statement";
    LogErrorUma(ErrorStatus::kFailedToPrefixSearch);
    return {};
  }
  statement->BindDouble(0, kDefaultScore);
  statement->BindString(1, base::StrCat({base::UTF16ToUTF8(query_term), "%"}));

  std::vector<FileSearchResult> matched_paths;
  TokenizedString tokenized_query(query_term, Mode::kWords);
  while (statement->Step()) {
    double relevance = FuzzyTokenizedStringMatch::TokenSetRatio(
        tokenized_query,
        TokenizedString(base::UTF8ToUTF16(statement->ColumnString(0)),
                        Mode::kWords),
        /*partial=*/false);
    if (relevance < GetRelevanceThreshold()) {
      continue;
    }

    base::FilePath file_path(statement->ColumnString(1));
    file_path = file_path.Append(statement->ColumnString(2));
    const base::Time time = statement->ColumnTime(3);
    // Updates the relevance as a weighted average of the query-term relevance
    // and the image annotation relevance score.
    relevance = kRelevanceWeight * relevance +
                (1 - kRelevanceWeight) * statement->ColumnDouble(4);
    DVLOG(1) << "Select: " << statement->ColumnString(0) << ", " << file_path
             << ", " << time << " rl: " << relevance;

    if (matched_paths.empty() || matched_paths.back().file_path != file_path) {
      matched_paths.emplace_back(file_path, std::move(time), relevance);
    } else if (matched_paths.back().relevance < relevance) {
      matched_paths.back().relevance = relevance;
    }
  }
  return matched_paths;
}

std::vector<FileSearchResult> AnnotationStorage::Search(
    const std::u16string& query,
    size_t max_num_results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (max_num_results < 1) {
    return {};
  }

  TokenizedString tokenized_query(query, Mode::kWords);
  if (tokenized_query.tokens().empty()) {
    return {};
  }

  std::vector<FileSearchResult> results;
  int normalization_constant = tokenized_query.tokens().size();
  bool fist_result = true;
  for (const auto& token : tokenized_query.tokens()) {
    if (IsStopWord(base::UTF16ToUTF8(token))) {
      normalization_constant -= 1;
      continue;
    }

    std::vector<FileSearchResult> next_result = PrefixSearch(token);
    if (next_result.empty()) {
      return {};
    }
    results =
        (fist_result) ? next_result : FindIntersection(results, next_result);
    fist_result = false;
  }

  if (results.size() <= max_num_results) {
    std::sort(
        results.begin(), results.end(),
        [](const FileSearchResult& a, const FileSearchResult& b) {
          // Sort in descending order by relevance and last_modified, then in
          // ascending order by file_path
          return std::tie(a.relevance, a.last_modified, b.file_path.value()) >
                 std::tie(b.relevance, b.last_modified, a.file_path.value());
        });
  } else {
    std::partial_sort(
        results.begin(), results.begin() + max_num_results, results.end(),
        [](const FileSearchResult& a, const FileSearchResult& b) {
          return std::tie(a.relevance, a.last_modified, b.file_path.value()) >
                 std::tie(b.relevance, b.last_modified, a.file_path.value());
        });
    results = std::vector<FileSearchResult>(results.begin(),
                                            results.begin() + max_num_results);
  }

  // Normalize to [0, 1].
  for (auto& result : results) {
    result.relevance = result.relevance / normalization_constant;
  }

  return results;
}

}  // namespace app_list
