// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "sql/statement.h"

using google::protobuf::MessageLite;

namespace {

const char kMetadataTableName[] = "resource_prefetch_predictor_metadata";
const char kHostRedirectTableName[] =
    "resource_prefetch_predictor_host_redirect";
const char kOriginTableName[] = "resource_prefetch_predictor_origin";

const char kCreateGlobalMetadataStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, value INTEGER, "
    "PRIMARY KEY (key))";
const char kCreateProtoTableStatementTemplate[] =
    "CREATE TABLE %s ( "
    "key TEXT, "
    "proto BLOB, "
    "PRIMARY KEY(key))";

}  // namespace

namespace predictors {

// static
void ResourcePrefetchPredictorTables::TrimRedirects(
    RedirectData* data,
    size_t max_consecutive_misses) {
  auto new_end =
      std::remove_if(data->mutable_redirect_endpoints()->begin(),
                     data->mutable_redirect_endpoints()->end(),
                     [max_consecutive_misses](const RedirectStat& x) {
                       return x.consecutive_misses() >= max_consecutive_misses;
                     });
  data->mutable_redirect_endpoints()->erase(
      new_end, data->mutable_redirect_endpoints()->end());
}

// static
void ResourcePrefetchPredictorTables::TrimOrigins(
    OriginData* data,
    size_t max_consecutive_misses) {
  auto* origins = data->mutable_origins();
  auto new_end = std::remove_if(
      origins->begin(), origins->end(), [=](const OriginStat& x) {
        return x.consecutive_misses() >= max_consecutive_misses;
      });
  origins->erase(new_end, origins->end());
}

// static
void ResourcePrefetchPredictorTables::SortOrigins(
    OriginData* data,
    const std::string& main_frame_origin) {
  auto* origins = data->mutable_origins();
  auto it = std::find_if(origins->begin(), origins->end(),
                         [&main_frame_origin](const OriginStat& x) {
                           return x.origin() == main_frame_origin;
                         });
  int iterator_offset = 0;
  if (it != origins->end()) {
    origins->SwapElements(0, it - origins->begin());
    iterator_offset = 1;
  }
  std::sort(origins->begin() + iterator_offset, origins->end(),
            [](const OriginStat& x, const OriginStat& y) {
              // Decreasing score ordering.
              return ComputeOriginScore(x) > ComputeOriginScore(y);
            });
}

ResourcePrefetchPredictorTables::ResourcePrefetchPredictorTables(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : PredictorTableBase(db_task_runner) {
  host_redirect_table_ =
      std::make_unique<LoadingPredictorKeyValueTable<RedirectData>>(
          kHostRedirectTableName);
  origin_table_ = std::make_unique<LoadingPredictorKeyValueTable<OriginData>>(
      kOriginTableName);
}

ResourcePrefetchPredictorTables::~ResourcePrefetchPredictorTables() = default;

// static
float ResourcePrefetchPredictorTables::ComputeOriginScore(
    const OriginStat& origin) {
  // The ranking is done by considering, in this order:
  // 1. High confidence resources (>75% and more than 10 hits)
  // 2. Mandatory network access
  // 3. Network accessed
  // 4. Average position (decreasing)
  float score = 0;
  float confidence = static_cast<float>(origin.number_of_hits()) /
                     (origin.number_of_hits() + origin.number_of_misses());

  bool is_high_confidence = confidence > .75 && origin.number_of_hits() > 10;
  score += is_high_confidence * 1e6;

  score += origin.always_access_network() * 1e4;
  score += origin.accessed_network() * 1e2;
  score += 1e2 - origin.average_position();

  return score;
}

void ResourcePrefetchPredictorTables::ScheduleDBTask(
    const base::Location& from_here,
    DBTask task) {
  GetTaskRunner()->PostTask(
      from_here,
      base::BindOnce(
          &ResourcePrefetchPredictorTables::ExecuteDBTaskOnDBSequence, this,
          std::move(task)));
}

void ResourcePrefetchPredictorTables::ExecuteDBTaskOnDBSequence(DBTask task) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  std::move(task).Run(DB());
}

LoadingPredictorKeyValueTable<RedirectData>*
ResourcePrefetchPredictorTables::host_redirect_table() {
  return host_redirect_table_.get();
}
LoadingPredictorKeyValueTable<OriginData>*
ResourcePrefetchPredictorTables::origin_table() {
  return origin_table_.get();
}

// static
bool ResourcePrefetchPredictorTables::DropTablesIfOutdated(sql::Database* db) {
  int version = GetDatabaseVersion(db);
  bool success = true;
  // Too new is also a problem.
  bool incompatible_version = version != kDatabaseVersion;

  // These are deprecated tables but they still have to be removed if present.
  static const char kUrlMetadataTableName[] =
      "resource_prefetch_predictor_url_metadata";
  static const char kHostMetadataTableName[] =
      "resource_prefetch_predictor_host_metadata";
  static const char kManifestTableName[] =
      "resource_prefetch_predictor_manifest";
  static const char kUrlResourceTableName[] = "resource_prefetch_predictor_url";
  static const char kUrlRedirectTableName[] =
      "resource_prefetch_predictor_url_redirect";
  static const char kHostResourceTableName[] =
      "resource_prefetch_predictor_host";

  if (incompatible_version) {
    for (const char* table_name :
         {kMetadataTableName, kUrlResourceTableName, kHostResourceTableName,
          kUrlRedirectTableName, kHostRedirectTableName, kManifestTableName,
          kUrlMetadataTableName, kHostMetadataTableName, kOriginTableName}) {
      success =
          success &&
          db->Execute(base::StringPrintf("DROP TABLE IF EXISTS %s", table_name)
                          .c_str());
    }
  }

  if (incompatible_version) {
    success =
        success &&
        db->Execute(base::StringPrintf(kCreateGlobalMetadataStatementTemplate,
                                       kMetadataTableName)
                        .c_str());
    success = success && SetDatabaseVersion(db, kDatabaseVersion);
  }

  return success;
}

// static
int ResourcePrefetchPredictorTables::GetDatabaseVersion(sql::Database* db) {
  int version = 0;
  if (db->DoesTableExist(kMetadataTableName)) {
    sql::Statement statement(db->GetUniqueStatement(
        base::StringPrintf("SELECT value FROM %s WHERE key='version'",
                           kMetadataTableName)
            .c_str()));
    if (statement.Step())
      version = statement.ColumnInt(0);
  }
  return version;
}

// static
bool ResourcePrefetchPredictorTables::SetDatabaseVersion(sql::Database* db,
                                                         int version) {
  sql::Statement statement(db->GetUniqueStatement(
      base::StringPrintf(
          "INSERT OR REPLACE INTO %s (key,value) VALUES ('version',%d)",
          kMetadataTableName, version)
          .c_str()));
  return statement.Run();
}

void ResourcePrefetchPredictorTables::CreateTableIfNonExistent() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  // Database initialization is all-or-nothing.
  sql::Database* db = DB();
  bool success = db->BeginTransaction();
  success = success && DropTablesIfOutdated(db);

  for (const char* table_name : {kHostRedirectTableName, kOriginTableName}) {
    success = success &&
              (db->DoesTableExist(table_name) ||
               db->Execute(base::StringPrintf(
                               kCreateProtoTableStatementTemplate, table_name)
                               .c_str()));
  }

  if (success)
    success = db->CommitTransaction();
  else
    db->RollbackTransaction();

  if (!success)
    ResetDB();
}

void ResourcePrefetchPredictorTables::LogDatabaseStats() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  // TODO(alexilin): Add existing tables stats.
}

}  // namespace predictors
