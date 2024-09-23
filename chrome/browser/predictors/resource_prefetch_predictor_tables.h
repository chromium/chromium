// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.pb.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/table_manager.h"

namespace predictors {

// Interface for database tables used by the ResourcePrefetchPredictor.
// All methods except the ExecuteDBTaskOnDBSequence need to be called on the UI
// thread.
//
// Currently manages:
//  - HostRedirectTable - key: host, value: RedirectData
//  - OriginTable - key: host, value: OriginData
//  - LcppTable - key: host, value: LcppData
class ResourcePrefetchPredictorTables : public sqlite_proto::TableManager {
 public:
  ResourcePrefetchPredictorTables(const ResourcePrefetchPredictorTables&) =
      delete;
  ResourcePrefetchPredictorTables& operator=(
      const ResourcePrefetchPredictorTables&) = delete;

  virtual sqlite_proto::KeyValueTable<RedirectData>* host_redirect_table();
  virtual sqlite_proto::KeyValueTable<OriginData>* origin_table();

  // Removes the redirects with more than |max_consecutive_misses| consecutive
  // misses from |data|.
  static void TrimRedirects(RedirectData* data, size_t max_consecutive_misses);

  // Removes the origins with more than |max_consecutive_misses| consecutive
  // misses from |data|.
  static void TrimOrigins(OriginData* data, size_t max_consecutive_misses);

  // Sorts the origins by score, decreasing. Prioritizes |main_frame_origin|
  // if found in |data|.
  static void SortOrigins(OriginData* data,
                          const std::string& main_frame_origin);

  // Computes score of |origin|.
  static float ComputeOriginScore(const OriginStat& origin);

  // The maximum length of the string that can be stored in the DB.
  static constexpr size_t kMaxStringLength = 1024;

 protected:
  // Protected for testing. Use PredictorDatabase::resource_prefetch_tables()
  // instead of this constructor.
  explicit ResourcePrefetchPredictorTables(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  ~ResourcePrefetchPredictorTables() override;

 private:
  friend class PredictorDatabaseInternal;
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseVersionIsSet);
  FRIEND_TEST_ALL_PREFIXES(ResourcePrefetchPredictorTablesTest,
                           DatabaseIsResetWhenIncompatible);

  // Database version. Always increment it when any change is made to the data
  // schema (including the .proto).
  static constexpr int kDatabaseVersion = 11;

  // sqlite_proto::TableManager:
  void CreateOrClearTablesIfNecessary() override;
  void LogDatabaseStats() override;

  static bool DropTablesIfOutdated(sql::Database* db);
  static int GetDatabaseVersion(sql::Database* db);
  static bool SetDatabaseVersion(sql::Database* db, int version);

  std::unique_ptr<sqlite_proto::KeyValueTable<RedirectData>>
      host_redirect_table_;
  std::unique_ptr<sqlite_proto::KeyValueTable<OriginData>> origin_table_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TABLES_H_
