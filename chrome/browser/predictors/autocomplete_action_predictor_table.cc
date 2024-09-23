// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor_table.h"

#include <cstddef>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "content/public/browser/browser_thread.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

// TODO(shishir): Rename the table for consistency.
#define AUTOCOMPLETE_PREDICTOR_TABLE_NAME "network_action_predictor"

// The maximum length allowed for strings in the database.
const size_t kMaxDataLength = 2048;

void BindRowToStatement(
    const predictors::AutocompleteActionPredictorTable::Row& row,
    sql::Statement* statement) {
  DCHECK(base::Uuid::ParseCaseInsensitive(row.id).is_valid());
  statement->BindString(0, row.id);
  statement->BindString16(1, row.user_text.substr(0, kMaxDataLength));
  statement->BindString(2, row.url.spec().substr(0, kMaxDataLength));
  statement->BindInt(3, row.number_of_hits);
  statement->BindInt(4, row.number_of_misses);
}

bool StepAndInitializeRow(
    sql::Statement* statement,
    predictors::AutocompleteActionPredictorTable::Row* row) {
  if (!statement->Step())
    return false;

  row->id = statement->ColumnString(0);
  row->user_text = statement->ColumnString16(1);
  row->url = GURL(statement->ColumnString(2));
  row->number_of_hits = statement->ColumnInt(3);
  row->number_of_misses = statement->ColumnInt(4);
  return true;
}

}  // namespace

namespace predictors {

AutocompleteActionPredictorTable::Row::Row()
    : number_of_hits(0),
      number_of_misses(0) {
}

AutocompleteActionPredictorTable::Row::Row(const Row::Id& id,
                                           const std::u16string& user_text,
                                           const GURL& url,
                                           int number_of_hits,
                                           int number_of_misses)
    : id(id),
      user_text(user_text),
      url(url),
      number_of_hits(number_of_hits),
      number_of_misses(number_of_misses) {}

AutocompleteActionPredictorTable::Row::Row(const Row& row)
    : id(row.id),
      user_text(row.user_text),
      url(row.url),
      number_of_hits(row.number_of_hits),
      number_of_misses(row.number_of_misses) {
}


void AutocompleteActionPredictorTable::GetRow(const Row::Id& id, Row* row) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT * FROM " AUTOCOMPLETE_PREDICTOR_TABLE_NAME " WHERE id=?"));
  statement.BindString(0, id);

  bool success = StepAndInitializeRow(&statement, row);
  DCHECK(success) << "Failed to get row " << id << " from "
                  << AUTOCOMPLETE_PREDICTOR_TABLE_NAME;
}

void AutocompleteActionPredictorTable::GetAllRows(Rows* row_buffer) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  row_buffer->clear();

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "SELECT * FROM " AUTOCOMPLETE_PREDICTOR_TABLE_NAME));
  if (!statement.is_valid())
    return;

  Row row;
  while (StepAndInitializeRow(&statement, &row))
    row_buffer->push_back(row);
}

void AutocompleteActionPredictorTable::AddRow(
    const AutocompleteActionPredictorTable::Row& row) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  AddAndUpdateRows(Rows(1, row), Rows());
}

void AutocompleteActionPredictorTable::UpdateRow(
    const AutocompleteActionPredictorTable::Row& row) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  AddAndUpdateRows(Rows(), Rows(1, row));
}

void AutocompleteActionPredictorTable::AddAndUpdateRows(
    const Rows& rows_to_add,
    const Rows& rows_to_update) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin())
    return;
  for (auto it = rows_to_add.begin(); it != rows_to_add.end(); ++it) {
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO " AUTOCOMPLETE_PREDICTOR_TABLE_NAME
                       "(id, user_text, url, number_of_hits, number_of_misses) "
                       "VALUES (?,?,?,?,?)"));
    if (!statement.is_valid())
      return;

    BindRowToStatement(*it, &statement);
    if (!statement.Run())
      return;
  }
  for (auto it = rows_to_update.begin(); it != rows_to_update.end(); ++it) {
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "UPDATE " AUTOCOMPLETE_PREDICTOR_TABLE_NAME
        " SET id=?, user_text=?, url=?, number_of_hits=?, number_of_misses=?"
        " WHERE id=?1"));
    if (!statement.is_valid())
      return;

    BindRowToStatement(*it, &statement);
    if (!statement.Run())
      return;
    DCHECK_GT(DB()->GetLastChangeCount(), 0);
  }
  transaction.Commit();
}

void AutocompleteActionPredictorTable::DeleteRows(
    const std::vector<Row::Id>& id_list) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  sql::Transaction transaction(DB());
  if (!transaction.Begin())
    return;
  for (auto it = id_list.begin(); it != id_list.end(); ++it) {
    sql::Statement statement(DB()->GetCachedStatement(
        SQL_FROM_HERE,
        "DELETE FROM " AUTOCOMPLETE_PREDICTOR_TABLE_NAME " WHERE id=?"));
    if (!statement.is_valid())
      return;

    statement.BindString(0, *it);
    if (!statement.Run())
      return;
  }
  transaction.Commit();
}

void AutocompleteActionPredictorTable::DeleteAllRows() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  sql::Statement statement(DB()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM " AUTOCOMPLETE_PREDICTOR_TABLE_NAME));
  if (!statement.is_valid())
    return;

  statement.Run();
}

AutocompleteActionPredictorTable::AutocompleteActionPredictorTable(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : sqlite_proto::TableManager(std::move(db_task_runner)) {}

AutocompleteActionPredictorTable::~AutocompleteActionPredictorTable() = default;

void AutocompleteActionPredictorTable::CreateOrClearTablesIfNecessary() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  if (DB()->DoesTableExist(AUTOCOMPLETE_PREDICTOR_TABLE_NAME)) {
    return;
  }

  bool success = DB()->Execute("CREATE TABLE " AUTOCOMPLETE_PREDICTOR_TABLE_NAME
                               "(id TEXT PRIMARY KEY, "
                               "user_text TEXT, "
                               "url TEXT, "
                               "number_of_hits INTEGER, "
                               "number_of_misses INTEGER)");
  if (!success)
    ResetDB();
}

void AutocompleteActionPredictorTable::LogDatabaseStats()  {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (CantAccessDatabase())
    return;

  sql::Statement count_statement(DB()->GetUniqueStatement(
      "SELECT count(id) FROM " AUTOCOMPLETE_PREDICTOR_TABLE_NAME));
  if (!count_statement.is_valid() || !count_statement.Step())
    return;
}

}  // namespace predictors
