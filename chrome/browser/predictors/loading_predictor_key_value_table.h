// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_KEY_VALUE_TABLE_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_KEY_VALUE_TABLE_H_

#include <map>
#include <string>
#include <vector>

#include "sql/statement.h"

namespace google {
namespace protobuf {
class MessageLite;
}
}  // namespace google

namespace predictors {

namespace internal {

void BindDataToStatement(const std::string& key,
                         const google::protobuf::MessageLite& data,
                         sql::Statement* statement);

std::string GetSelectAllSql(const std::string& table_name);
std::string GetReplaceSql(const std::string& table_name);
std::string GetDeleteSql(const std::string& table_name);
std::string GetDeleteAllSql(const std::string& table_name);

}  // namespace internal

// The backend class helps perform database operations on a single table. The
// table name is passed as a constructor argument. The table schema is fixed: it
// always consists of two columns, TEXT type "key" and BLOB type "proto". The
// class doesn't manage the creation and the deletion of the table.
//
// All the functions except of the constructor must be called on a DB sequence
// of the ResourcePrefetchPredictorTables.
// The preferred way to call the methods of this class is passing the method to
// ResourcePrefetchPredictorTables::ScheduleDBTask().
//
// Example:
// tables_->ScheduleDBTask(
//     FROM_HERE,
//     base::BindOnce(&LoadingPredictorKeyValueTable<PrefetchData>::UpdateData,
//                    base::Unretained(table_), key, data));
template <typename T>
class LoadingPredictorKeyValueTable {
 public:
  explicit LoadingPredictorKeyValueTable(const std::string& table_name);
  // Virtual for testing.
  virtual ~LoadingPredictorKeyValueTable() {}
  virtual void GetAllData(std::map<std::string, T>* data_map,
                          sql::Database* db) const;
  virtual void UpdateData(const std::string& key,
                          const T& data,
                          sql::Database* db);
  virtual void DeleteData(const std::vector<std::string>& keys,
                          sql::Database* db);
  virtual void DeleteAllData(sql::Database* db);

 private:
  const std::string table_name_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorKeyValueTable);
};

template <typename T>
LoadingPredictorKeyValueTable<T>::LoadingPredictorKeyValueTable(
    const std::string& table_name)
    : table_name_(table_name) {}

template <typename T>
void LoadingPredictorKeyValueTable<T>::GetAllData(
    std::map<std::string, T>* data_map,
    sql::Database* db) const {
  sql::Statement reader(db->GetUniqueStatement(
      ::predictors::internal::GetSelectAllSql(table_name_).c_str()));
  while (reader.Step()) {
    auto it = data_map->emplace(reader.ColumnString(0), T()).first;
    int size = reader.ColumnByteLength(1);
    const void* blob = reader.ColumnBlob(1);
    DCHECK(blob);
    it->second.ParseFromArray(blob, size);
  }
}

template <typename T>
void LoadingPredictorKeyValueTable<T>::UpdateData(const std::string& key,
                                                  const T& data,
                                                  sql::Database* db) {
  sql::Statement inserter(db->GetUniqueStatement(
      ::predictors::internal::GetReplaceSql(table_name_).c_str()));
  ::predictors::internal::BindDataToStatement(key, data, &inserter);
  inserter.Run();
}

template <typename T>
void LoadingPredictorKeyValueTable<T>::DeleteData(
    const std::vector<std::string>& keys,
    sql::Database* db) {
  sql::Statement deleter(db->GetUniqueStatement(
      ::predictors::internal::GetDeleteSql(table_name_).c_str()));
  for (const auto& key : keys) {
    deleter.BindString(0, key);
    deleter.Run();
    deleter.Reset(true);
  }
}

template <typename T>
void LoadingPredictorKeyValueTable<T>::DeleteAllData(sql::Database* db) {
  sql::Statement deleter(db->GetUniqueStatement(
      ::predictors::internal::GetDeleteAllSql(table_name_).c_str()));
  deleter.Run();
}

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_KEY_VALUE_TABLE_H_
