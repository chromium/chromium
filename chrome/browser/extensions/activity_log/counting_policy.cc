// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A policy for storing activity log data to a database that performs
// aggregation to reduce the size of the database.  The database layout is
// nearly the same as FullStreamUIPolicy, which stores a complete log, with a
// few changes:
//   - a "count" column is added to track how many log records were merged
//     together into this row
//   - the "time" column measures the most recent time that the current row was
//     updated
// When writing a record, if a row already exists where all other columns
// (extension_id, action_type, api_name, args, urls, etc.) all match, and the
// previous time falls within today (the current time), then the count field on
// the old row is incremented.  Otherwise, a new row is written.
//
// For many text columns, repeated strings are compressed by moving string
// storage to a separate table ("string_ids") and storing only an identifier in
// the logging table.  For example, if the api_name_x column contained the
// value 4 and the string_ids table contained a row with primary key 4 and
// value 'tabs.query', then the api_name field should be taken to have the
// value 'tabs.query'.  Each column ending with "_x" is compressed in this way.
// All lookups are to the string_ids table, except for the page_url_x and
// arg_url_x columns, which are converted via the url_ids table (this
// separation of URL values is to help simplify history clearing).
//
// The activitylog_uncompressed view allows for simpler reading of the activity
// log contents with identifiers already translated to string values.

#include "chrome/browser/extensions/activity_log/counting_policy.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/common/chrome_constants.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace {

using extensions::Action;

// Delay between cleaning passes (to delete old action records) through the
// database.
constexpr base::TimeDelta kCleaningDelay = base::TimeDelta::FromHours(12);

// We should log the arguments to these API calls.  Be careful when
// constructing this whitelist to not keep arguments that might compromise
// privacy by logging too much data to the activity log.
//
// TODO(mvrable): The contents of this whitelist should be reviewed and
// expanded as needed.
struct ApiList {
  Action::ActionType type;
  const char* name;
};

const ApiList kAlwaysLog[] = {
    {Action::ACTION_API_CALL, "bookmarks.create"},
    {Action::ACTION_API_CALL, "bookmarks.update"},
    {Action::ACTION_API_CALL, "cookies.get"},
    {Action::ACTION_API_CALL, "cookies.getAll"},
    {Action::ACTION_API_CALL, "extension.connect"},
    {Action::ACTION_API_CALL, "extension.sendMessage"},
    {Action::ACTION_API_CALL, "fileSystem.chooseEntry"},
    {Action::ACTION_API_CALL, "socket.bind"},
    {Action::ACTION_API_CALL, "socket.connect"},
    {Action::ACTION_API_CALL, "socket.create"},
    {Action::ACTION_API_CALL, "socket.listen"},
    {Action::ACTION_API_CALL, "tabs.executeScript"},
    {Action::ACTION_API_CALL, "tabs.insertCSS"},
    {Action::ACTION_API_CALL, "types.ChromeSetting.clear"},
    {Action::ACTION_API_CALL, "types.ChromeSetting.get"},
    {Action::ACTION_API_CALL, "types.ChromeSetting.set"},
    {Action::ACTION_CONTENT_SCRIPT, ""},
    {Action::ACTION_DOM_ACCESS, "Document.createElement"},
    {Action::ACTION_DOM_ACCESS, "Document.createElementNS"},
};

// Columns in the main database table.  See the file-level comment for a
// discussion of how data is stored and the meanings of the _x columns.
const char* const kTableContentFields[] = {
    "count", "extension_id_x", "time", "action_type", "api_name_x", "args_x",
    "page_url_x", "page_title_x", "arg_url_x", "other_x"};
const char* const kTableFieldTypes[] = {
    "INTEGER NOT NULL DEFAULT 1", "INTEGER NOT NULL", "INTEGER", "INTEGER",
    "INTEGER", "INTEGER", "INTEGER", "INTEGER", "INTEGER",
    "INTEGER"};

// Miscellaneous SQL commands for initializing the database; these should be
// idempotent.
static const char kPolicyMiscSetup[] =
    // The activitylog_uncompressed view performs string lookups for simpler
    // access to the log data.
    "DROP VIEW IF EXISTS activitylog_uncompressed;\n"
    "CREATE VIEW activitylog_uncompressed AS\n"
    "SELECT count,\n"
    "    x1.value AS extension_id,\n"
    "    time,\n"
    "    action_type,\n"
    "    x2.value AS api_name,\n"
    "    x3.value AS args,\n"
    "    x4.value AS page_url,\n"
    "    x5.value AS page_title,\n"
    "    x6.value AS arg_url,\n"
    "    x7.value AS other,\n"
    "    activitylog_compressed.rowid AS activity_id\n"
    "FROM activitylog_compressed\n"
    "    LEFT JOIN string_ids AS x1 ON (x1.id = extension_id_x)\n"
    "    LEFT JOIN string_ids AS x2 ON (x2.id = api_name_x)\n"
    "    LEFT JOIN string_ids AS x3 ON (x3.id = args_x)\n"
    "    LEFT JOIN url_ids    AS x4 ON (x4.id = page_url_x)\n"
    "    LEFT JOIN string_ids AS x5 ON (x5.id = page_title_x)\n"
    "    LEFT JOIN url_ids    AS x6 ON (x6.id = arg_url_x)\n"
    "    LEFT JOIN string_ids AS x7 ON (x7.id = other_x);\n"
    // An index on all fields except count and time: all the fields that aren't
    // changed when incrementing a count.  This should accelerate finding the
    // rows to update (at worst several rows will need to be checked to find
    // the one in the right time range).
    "CREATE INDEX IF NOT EXISTS activitylog_compressed_index\n"
    "ON activitylog_compressed(extension_id_x, action_type, api_name_x,\n"
    "    args_x, page_url_x, page_title_x, arg_url_x, other_x)";

// SQL statements to clean old, unused entries out of the string and URL id
// tables.
static const char kStringTableCleanup[] =
    "DELETE FROM string_ids WHERE id NOT IN\n"
    "(SELECT extension_id_x FROM activitylog_compressed\n"
    "    WHERE extension_id_x IS NOT NULL\n"
    " UNION SELECT api_name_x FROM activitylog_compressed\n"
    "    WHERE api_name_x IS NOT NULL\n"
    " UNION SELECT args_x FROM activitylog_compressed\n"
    "    WHERE args_x IS NOT NULL\n"
    " UNION SELECT page_title_x FROM activitylog_compressed\n"
    "    WHERE page_title_x IS NOT NULL\n"
    " UNION SELECT other_x FROM activitylog_compressed\n"
    "    WHERE other_x IS NOT NULL)";
static const char kUrlTableCleanup[] =
    "DELETE FROM url_ids WHERE id NOT IN\n"
    "(SELECT page_url_x FROM activitylog_compressed\n"
    "    WHERE page_url_x IS NOT NULL\n"
    " UNION SELECT arg_url_x FROM activitylog_compressed\n"
    "    WHERE arg_url_x IS NOT NULL)";

}  // namespace

namespace extensions {

const char CountingPolicy::kTableName[] = "activitylog_compressed";
const char CountingPolicy::kReadViewName[] = "activitylog_uncompressed";

CountingPolicy::CountingPolicy(Profile* profile)
    : ActivityLogDatabasePolicy(
          profile,
          base::FilePath(chrome::kExtensionActivityLogFilename)),
      string_table_("string_ids"),
      url_table_("url_ids"),
      retention_time_(base::TimeDelta::FromHours(60)) {
  for (size_t i = 0; i < base::size(kAlwaysLog); i++) {
    api_arg_whitelist_.insert(
        std::make_pair(kAlwaysLog[i].type, kAlwaysLog[i].name));
  }
}

CountingPolicy::~CountingPolicy() {}

bool CountingPolicy::InitDatabase(sql::Database* db) {
  if (!string_table_.Initialize(db))
    return false;
  if (!url_table_.Initialize(db))
    return false;

  // Create the unified activity log entry table.
  if (!ActivityDatabase::InitializeTable(db, kTableName, kTableContentFields,
                                         kTableFieldTypes,
                                         base::size(kTableContentFields)))
    return false;

  // Create a view for easily accessing the uncompressed form of the data, and
  // any necessary indexes if needed.
  return db->Execute(kPolicyMiscSetup);
}

void CountingPolicy::ProcessAction(scoped_refptr<Action> action) {
  ScheduleAndForget(this, &CountingPolicy::QueueAction, action);
}

void CountingPolicy::QueueAction(scoped_refptr<Action> action) {
  if (activity_database()->is_db_valid()) {
    action = action->Clone();
    Util::StripPrivacySensitiveFields(action);
    Util::StripArguments(api_arg_whitelist_, action);

    // If the current action falls on a different date than the ones in the
    // queue, flush the queue out now to prevent any false merging (actions
    // from different days being merged).
    base::Time new_date = action->time().LocalMidnight();
    if (new_date != queued_actions_date_)
      activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);
    queued_actions_date_ = new_date;

    auto queued_entry = queued_actions_.find(action);
    if (queued_entry == queued_actions_.end()) {
      queued_actions_[action] = 1;
    } else {
      // Update the timestamp in the key to be the latest time seen.  Modifying
      // the time is safe since that field is not involved in key comparisons
      // in the map.
      using std::max;
      queued_entry->first->set_time(
          max(queued_entry->first->time(), action->time()));
      queued_entry->second++;
    }
    activity_database()->AdviseFlush(queued_actions_.size());
  }
}

bool CountingPolicy::FlushDatabase(sql::Database* db) {
  // Columns that must match exactly for database rows to be coalesced.
  static const char* const matched_columns[] = {
      "extension_id_x", "action_type", "api_name_x", "args_x", "page_url_x",
      "page_title_x", "arg_url_x", "other_x"};
  ActionQueue queue;
  queue.swap(queued_actions_);

  // Whether to clean old records out of the activity log database.  Do this
  // much less frequently than database flushes since it is expensive, but
  // always check on the first database flush (since there might be a large
  // amount of data to clear).
  bool clean_database = (last_database_cleaning_time_.is_null() ||
                         Now() - last_database_cleaning_time_ > kCleaningDelay);

  if (queue.empty() && !clean_database)
    return true;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Adding an Action to the database is a two step process that depends on
  // whether the count on an existing row can be incremented or a new row needs
  // to be inserted.
  //   1. Run the query in locate_str to search for a row which matches and can
  //      have the count incremented.
  //  2a. If found, increment the count using update_str and the rowid found in
  //      step 1, or
  //  2b. If not found, insert a new row using insert_str.
  std::string locate_str =
      "SELECT rowid FROM " + std::string(kTableName) +
      " WHERE time >= ? AND time < ?";
  std::string insert_str =
      "INSERT INTO " + std::string(kTableName) + "(count, time";
  std::string update_str =
      "UPDATE " + std::string(kTableName) +
      " SET count = count + ?, time = max(?, time)"
      " WHERE rowid = ?";

  for (size_t i = 0; i < base::size(matched_columns); i++) {
    locate_str = base::StringPrintf(
        "%s AND %s IS ?", locate_str.c_str(), matched_columns[i]);
    insert_str =
        base::StringPrintf("%s, %s", insert_str.c_str(), matched_columns[i]);
  }
  insert_str += ") VALUES (?, ?";
  for (size_t i = 0; i < base::size(matched_columns); i++) {
    insert_str += ", ?";
  }
  locate_str += " ORDER BY time DESC LIMIT 1";
  insert_str += ")";

  for (auto i = queue.begin(); i != queue.end(); ++i) {
    const Action& action = *i->first;
    int count = i->second;

    base::Time day_start = action.time().LocalMidnight();
    base::Time next_day = Util::AddDays(day_start, 1);

    // The contents in values must match up with fields in matched_columns.  A
    // value of -1 is used to encode a null database value.
    int64_t id;
    std::vector<int64_t> matched_values;

    if (!string_table_.StringToInt(db, action.extension_id(), &id))
      return false;
    matched_values.push_back(id);

    matched_values.push_back(static_cast<int>(action.action_type()));

    if (!string_table_.StringToInt(db, action.api_name(), &id))
      return false;
    matched_values.push_back(id);

    if (action.args()) {
      std::string args = Util::Serialize(action.args());
      // TODO(mvrable): For now, truncate long argument lists.  This is a
      // workaround for excessively-long values coming from DOM logging.  When
      // the V8ValueConverter is fixed to return more reasonable values, we can
      // drop the truncation.
      if (args.length() > 10000) {
        args = "[\"<too_large>\"]";
      }
      if (!string_table_.StringToInt(db, args, &id))
        return false;
      matched_values.push_back(id);
    } else {
      matched_values.push_back(-1);
    }

    std::string page_url_string = action.SerializePageUrl();
    if (!page_url_string.empty()) {
      if (!url_table_.StringToInt(db, page_url_string, &id))
        return false;
      matched_values.push_back(id);
    } else {
      matched_values.push_back(-1);
    }

    // TODO(mvrable): Create a title_table_?
    if (!action.page_title().empty()) {
      if (!string_table_.StringToInt(db, action.page_title(), &id))
        return false;
      matched_values.push_back(id);
    } else {
      matched_values.push_back(-1);
    }

    std::string arg_url_string = action.SerializeArgUrl();
    if (!arg_url_string.empty()) {
      if (!url_table_.StringToInt(db, arg_url_string, &id))
        return false;
      matched_values.push_back(id);
    } else {
      matched_values.push_back(-1);
    }

    if (action.other()) {
      if (!string_table_.StringToInt(db, Util::Serialize(action.other()), &id))
        return false;
      matched_values.push_back(id);
    } else {
      matched_values.push_back(-1);
    }

    // Search for a matching row for this action whose count can be
    // incremented.
    sql::Statement locate_statement(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), locate_str.c_str()));
    locate_statement.BindInt64(0, day_start.ToInternalValue());
    locate_statement.BindInt64(1, next_day.ToInternalValue());
    for (size_t j = 0; j < matched_values.size(); j++) {
      // A call to BindNull when matched_values contains -1 is likely not
      // necessary as parameters default to null before they are explicitly
      // bound.  But to be completely clear, and in case a cached statement
      // ever comes with some values already bound, we bind all parameters
      // (even null ones) explicitly.
      if (matched_values[j] == -1)
        locate_statement.BindNull(j + 2);
      else
        locate_statement.BindInt64(j + 2, matched_values[j]);
    }

    if (locate_statement.Step()) {
      // A matching row was found.  Update the count and time.
      int64_t rowid = locate_statement.ColumnInt64(0);
      sql::Statement update_statement(db->GetCachedStatement(
          sql::StatementID(SQL_FROM_HERE), update_str.c_str()));
      update_statement.BindInt(0, count);
      update_statement.BindInt64(1, action.time().ToInternalValue());
      update_statement.BindInt64(2, rowid);
      if (!update_statement.Run())
        return false;
    } else if (locate_statement.Succeeded()) {
      // No matching row was found, so we need to insert one.
      sql::Statement insert_statement(db->GetCachedStatement(
          sql::StatementID(SQL_FROM_HERE), insert_str.c_str()));
      insert_statement.BindInt(0, count);
      insert_statement.BindInt64(1, action.time().ToInternalValue());
      for (size_t j = 0; j < matched_values.size(); j++) {
        if (matched_values[j] == -1)
          insert_statement.BindNull(j + 2);
        else
          insert_statement.BindInt64(j + 2, matched_values[j]);
      }
      if (!insert_statement.Run())
        return false;
    } else {
      // Database error.
      return false;
    }
  }

  if (clean_database) {
    base::Time cutoff = (Now() - retention_time()).LocalMidnight();
    if (!CleanOlderThan(db, cutoff))
      return false;
    last_database_cleaning_time_ = Now();
  }

  if (!transaction.Commit())
    return false;

  return true;
}

std::unique_ptr<Action::ActionVector> CountingPolicy::DoReadFilteredData(
    const std::string& extension_id,
    const Action::ActionType type,
    const std::string& api_name,
    const std::string& page_url,
    const std::string& arg_url,
    const int days_ago) {
  DCHECK(GetActivityLogTaskRunner()->RunsTasksInCurrentSequence());
  // Ensure data is flushed to the database first so that we query over all
  // data.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);
  std::unique_ptr<Action::ActionVector> actions(new Action::ActionVector());

  sql::Database* db = GetDatabaseConnection();
  if (!db)
    return actions;

  // Build up the query based on which parameters were specified.
  std::string where_str = "";
  std::string where_next = "";
  if (!extension_id.empty()) {
    where_str += "extension_id=?";
    where_next = " AND ";
  }
  if (!api_name.empty()) {
    where_str += where_next + "api_name LIKE ?";
    where_next = " AND ";
  }
  if (type != Action::ACTION_ANY) {
    where_str += where_next + "action_type=?";
    where_next = " AND ";
  }
  if (!page_url.empty()) {
    where_str += where_next + "page_url LIKE ?";
    where_next = " AND ";
  }
  if (!arg_url.empty()) {
    where_str += where_next + "arg_url LIKE ?";
    where_next = " AND ";
  }
  if (days_ago >= 0)
    where_str += where_next + "time BETWEEN ? AND ?";

  std::string query_str = base::StringPrintf(
      "SELECT extension_id,time, action_type, api_name, args, page_url,"
      "page_title, arg_url, other, count, activity_id FROM %s %s %s ORDER BY "
      "count DESC, time DESC LIMIT 300",
      kReadViewName,
      where_str.empty() ? "" : "WHERE",
      where_str.c_str());
  sql::Statement query(db->GetUniqueStatement(query_str.c_str()));
  int i = -1;
  if (!extension_id.empty())
    query.BindString(++i, extension_id);
  if (!api_name.empty())
    query.BindString(++i, api_name + "%");
  if (type != Action::ACTION_ANY)
    query.BindInt(++i, static_cast<int>(type));
  if (!page_url.empty())
    query.BindString(++i, page_url + "%");
  if (!arg_url.empty())
    query.BindString(++i, arg_url + "%");
  if (days_ago >= 0) {
    int64_t early_bound;
    int64_t late_bound;
    Util::ComputeDatabaseTimeBounds(Now(), days_ago, &early_bound, &late_bound);
    query.BindInt64(++i, early_bound);
    query.BindInt64(++i, late_bound);
  }

  // Execute the query and get results.
  while (query.is_valid() && query.Step()) {
    auto action = base::MakeRefCounted<Action>(
        query.ColumnString(0),
        base::Time::FromInternalValue(query.ColumnInt64(1)),
        static_cast<Action::ActionType>(query.ColumnInt(2)),
        query.ColumnString(3), query.ColumnInt64(10));

    if (query.GetColumnType(4) != sql::ColumnType::kNull) {
      std::unique_ptr<base::Value> parsed_value =
          base::JSONReader::ReadDeprecated(query.ColumnString(4));
      if (parsed_value && parsed_value->is_list()) {
        action->set_args(base::WrapUnique(
            static_cast<base::ListValue*>(parsed_value.release())));
      }
    }

    action->ParsePageUrl(query.ColumnString(5));
    action->set_page_title(query.ColumnString(6));
    action->ParseArgUrl(query.ColumnString(7));

    if (query.GetColumnType(8) != sql::ColumnType::kNull) {
      std::unique_ptr<base::Value> parsed_value =
          base::JSONReader::ReadDeprecated(query.ColumnString(8));
      if (parsed_value && parsed_value->is_dict()) {
        action->set_other(base::WrapUnique(
            static_cast<base::DictionaryValue*>(parsed_value.release())));
      }
    }
    action->set_count(query.ColumnInt(9));
    actions->push_back(action);
  }

  return actions;
}

void CountingPolicy::DoRemoveActions(const std::vector<int64_t>& action_ids) {
  if (action_ids.empty())
    return;

  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  // Flush data first so the activity removal affects queued-up data as well.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return;

  std::string statement_str =
      base::StringPrintf("DELETE FROM %s WHERE rowid = ?", kTableName);
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), statement_str.c_str()));
  for (size_t i = 0; i < action_ids.size(); i++) {
    statement.Reset(true);
    statement.BindInt64(0, action_ids[i]);
    if (!statement.Run()) {
      LOG(ERROR) << "Removing activities from database failed: "
                 << statement.GetSQLStatement();
      break;
    }
  }

  CleanStringTables(db);

  if (!transaction.Commit()) {
    LOG(ERROR) << "Removing activities from database failed";
  }
}

void CountingPolicy::DoRemoveURLs(const std::vector<GURL>& restrict_urls) {
  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  // Flush data first so the URL clearing affects queued-up data as well.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);

  // If no restrictions then then all URLs need to be removed.
  if (restrict_urls.empty()) {
    std::string sql_str = base::StringPrintf(
      "UPDATE %s SET page_url_x=NULL,page_title_x=NULL,arg_url_x=NULL",
      kTableName);

    sql::Statement statement;
    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));

    if (!statement.Run()) {
      LOG(ERROR) << "Removing all URLs from database failed: "
                 << statement.GetSQLStatement();
      return;
    }
  }

  // If URLs are specified then restrict to only those URLs.
  for (size_t i = 0; i < restrict_urls.size(); ++i) {
    int64_t url_id;
    if (!restrict_urls[i].is_valid() ||
        !url_table_.StringToInt(db, restrict_urls[i].spec(), &url_id)) {
      continue;
    }

    // Remove any that match the page_url.
    std::string sql_str = base::StringPrintf(
      "UPDATE %s SET page_url_x=NULL,page_title_x=NULL WHERE page_url_x IS ?",
      kTableName);

    sql::Statement statement;
    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindInt64(0, url_id);

    if (!statement.Run()) {
      LOG(ERROR) << "Removing page URL from database failed: "
                 << statement.GetSQLStatement();
      return;
    }

    // Remove any that match the arg_url.
    sql_str = base::StringPrintf(
      "UPDATE %s SET arg_url_x=NULL WHERE arg_url_x IS ?", kTableName);

    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindInt64(0, url_id);

    if (!statement.Run()) {
      LOG(ERROR) << "Removing arg URL from database failed: "
                 << statement.GetSQLStatement();
      return;
    }
  }

  // Clean up unused strings from the strings and urls table to really delete
  // the urls and page titles. Should be called even if an error occured when
  // removing a URL as there may some things to clean up.
  CleanStringTables(db);
}

void CountingPolicy::DoRemoveExtensionData(const std::string& extension_id) {
  if (extension_id.empty())
    return;

  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  // Make sure any queued in memory are sent to the database before cleaning.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);

  std::string sql_str = base::StringPrintf(
      "DELETE FROM %s WHERE extension_id_x=?", kTableName);
  sql::Statement statement(
      db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  int64_t id;
  if (string_table_.StringToInt(db, extension_id, &id)) {
    statement.BindInt64(0, id);
  } else {
    // If the string isn't listed, that means we never recorded anything about
    // the extension so there's no deletion to do.
    statement.Clear();
    return;
  }
  if (!statement.Run()) {
    LOG(ERROR) << "Removing URLs for extension "
               << extension_id << "from database failed: "
               << statement.GetSQLStatement();
  }
  CleanStringTables(db);
}

void CountingPolicy::DoDeleteDatabase() {
  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  queued_actions_.clear();

  // Not wrapped in a transaction because a late failure shouldn't undo a
  // previous deletion.
  std::string sql_str = base::StringPrintf("DELETE FROM %s", kTableName);
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE),
      sql_str.c_str()));
  if (!statement.Run()) {
    LOG(ERROR) << "Deleting the database failed: "
               << statement.GetSQLStatement();
    return;
  }
  statement.Clear();
  string_table_.ClearCache();
  statement.Assign(db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE),
                                          "DELETE FROM string_ids"));
  if (!statement.Run()) {
    LOG(ERROR) << "Deleting the database failed: "
               << statement.GetSQLStatement();
    return;
  }
  statement.Clear();
  url_table_.ClearCache();
  statement.Assign(db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE),
                                          "DELETE FROM url_ids"));
  if (!statement.Run()) {
    LOG(ERROR) << "Deleting the database failed: "
               << statement.GetSQLStatement();
    return;
  }
  statement.Clear();
  statement.Assign(db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE),
                                          "VACUUM"));
  if (!statement.Run()) {
    LOG(ERROR) << "Vacuuming the database failed: "
               << statement.GetSQLStatement();
  }
}

void CountingPolicy::ReadFilteredData(
    const std::string& extension_id,
    const Action::ActionType type,
    const std::string& api_name,
    const std::string& page_url,
    const std::string& arg_url,
    const int days_ago,
    base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> callback) {
  base::PostTaskAndReplyWithResult(
      GetActivityLogTaskRunner().get(), FROM_HERE,
      base::BindOnce(&CountingPolicy::DoReadFilteredData,
                     base::Unretained(this), extension_id, type, api_name,
                     page_url, arg_url, days_ago),
      std::move(callback));
}

void CountingPolicy::RemoveActions(const std::vector<int64_t>& action_ids) {
  ScheduleAndForget(this, &CountingPolicy::DoRemoveActions, action_ids);
}

void CountingPolicy::RemoveURLs(const std::vector<GURL>& restrict_urls) {
  ScheduleAndForget(this, &CountingPolicy::DoRemoveURLs, restrict_urls);
}

void CountingPolicy::RemoveExtensionData(const std::string& extension_id) {
  ScheduleAndForget(this, &CountingPolicy::DoRemoveExtensionData, extension_id);
}

void CountingPolicy::DeleteDatabase() {
  ScheduleAndForget(this, &CountingPolicy::DoDeleteDatabase);
}

void CountingPolicy::OnDatabaseFailure() {
  queued_actions_.clear();
}

void CountingPolicy::OnDatabaseClose() {
  delete this;
}

// Cleans old records from the activity log database.
bool CountingPolicy::CleanOlderThan(sql::Database* db,
                                    const base::Time& cutoff) {
  std::string clean_statement =
      "DELETE FROM " + std::string(kTableName) + " WHERE time < ?";
  sql::Statement cleaner(db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE),
                                                clean_statement.c_str()));
  cleaner.BindInt64(0, cutoff.ToInternalValue());
  if (!cleaner.Run())
    return false;
  return CleanStringTables(db);
}

// Cleans unused interned strings from the database.  This should be run after
// deleting rows from the main log table to clean out stale values.
bool CountingPolicy::CleanStringTables(sql::Database* db) {
  sql::Statement cleaner1(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), kStringTableCleanup));
  if (!cleaner1.Run())
    return false;
  if (db->GetLastChangeCount() > 0)
    string_table_.ClearCache();

  sql::Statement cleaner2(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), kUrlTableCleanup));
  if (!cleaner2.Run())
    return false;
  if (db->GetLastChangeCount() > 0)
    url_table_.ClearCache();

  return true;
}

void CountingPolicy::Close() {
  ScheduleAndForget(activity_database(), &ActivityDatabase::Close);
}

}  // namespace extensions
