// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_TABLE_H_
#define CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_TABLE_H_

#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "components/sqlite_proto/table_manager.h"
#include "url/gurl.h"

namespace predictors {

// This manages the autocomplete predictor table within the SQLite database
// passed in to the constructor. It expects the following scheme:
//
// network_action_predictor
//   id                 A unique id.
//   user_text          What the user typed.
//   url                The URL of the entry.
//   number_of_hits     Number of times the entry was shown to the user and
//                      selected.
//   number_of_misses   Number of times the entry was shown to the user but not
//                      selected.
//
// TODO(dominich): Consider adding this table to one of the history databases.
// In memory is currently used, but adding to the on-disk visits database
// would allow DeleteOldEntries to be cheaper through use of a join.
//
// All the functions apart from constructor and destructor have to be called in
// the DB sequence provided to the constructor of this class.
class AutocompleteActionPredictorTable : public sqlite_proto::TableManager {
 public:
  struct Row {
    // TODO(dominich): Make this 64-bit integer as an optimization. This
    // requires some investigation into how to make sure the id is unique for
    // each user_text/url pair.
    // http://crbug.com/102020
    typedef std::string Id;

    Row();

    // Only used by unit tests.
    Row(const Id& id,
        const std::u16string& user_text,
        const GURL& url,
        int number_of_hits,
        int number_of_misses);

    Row(const Row& row);

    Id id;
    std::u16string user_text;
    GURL url;
    int number_of_hits;
    int number_of_misses;
  };

  typedef std::vector<Row> Rows;

  AutocompleteActionPredictorTable(const AutocompleteActionPredictorTable&) =
      delete;
  AutocompleteActionPredictorTable& operator=(
      const AutocompleteActionPredictorTable&) = delete;

  // DB sequence functions.
  void GetRow(const Row::Id& id, Row* row);
  void GetAllRows(Rows* row_buffer);
  void AddRow(const Row& row);
  void UpdateRow(const Row& row);
  void AddAndUpdateRows(const Rows& rows_to_add, const Rows& rows_to_update);
  void DeleteRows(const std::vector<Row::Id>& id_list);
  void DeleteAllRows();

 private:
  friend class PredictorDatabaseInternal;

  explicit AutocompleteActionPredictorTable(
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);
  ~AutocompleteActionPredictorTable() override;

  // TableManager methods (DB sequence).
  void CreateOrClearTablesIfNecessary() override;
  void LogDatabaseStats() override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_TABLE_H_
