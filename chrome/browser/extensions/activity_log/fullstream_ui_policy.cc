// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

using base::FilePath;
using base::Unretained;
using content::BrowserThread;

namespace constants = activity_log_constants;

namespace extensions {

const char FullStreamUIPolicy::kTableName[] = "activitylog_full";
const char* const FullStreamUIPolicy::kTableContentFields[] = {
  "extension_id", "time", "action_type", "api_name", "args", "page_url",
  "page_title", "arg_url", "other"
};
const char* const FullStreamUIPolicy::kTableFieldTypes[] = {
  "LONGVARCHAR NOT NULL", "INTEGER", "INTEGER", "LONGVARCHAR", "LONGVARCHAR",
  "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR", "LONGVARCHAR"
};
const int FullStreamUIPolicy::kTableFieldCount =
    base::size(FullStreamUIPolicy::kTableContentFields);

FullStreamUIPolicy::FullStreamUIPolicy(Profile* profile)
    : ActivityLogDatabasePolicy(
          profile,
          FilePath(chrome::kExtensionActivityLogFilename)) {}

FullStreamUIPolicy::~FullStreamUIPolicy() {}

bool FullStreamUIPolicy::InitDatabase(sql::Database* db) {
  // Create the unified activity log entry table.
  return ActivityDatabase::InitializeTable(db, kTableName, kTableContentFields,
                                           kTableFieldTypes,
                                           base::size(kTableContentFields));
}

bool FullStreamUIPolicy::FlushDatabase(sql::Database* db) {
  if (queued_actions_.empty())
    return true;

  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  std::string sql_str =
      "INSERT INTO " + std::string(FullStreamUIPolicy::kTableName) +
      " (extension_id, time, action_type, api_name, args, "
      "page_url, page_title, arg_url, other) VALUES (?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));

  for (size_t i = 0; i != queued_actions_.size(); ++i) {
    const Action& action = *queued_actions_[i];
    statement.Reset(true);
    statement.BindString(0, action.extension_id());
    statement.BindInt64(1, action.time().ToInternalValue());
    statement.BindInt(2, static_cast<int>(action.action_type()));
    statement.BindString(3, action.api_name());
    if (action.args()) {
      statement.BindString(4, Util::Serialize(action.args()));
    }
    std::string page_url_string = action.SerializePageUrl();
    if (!page_url_string.empty()) {
      statement.BindString(5, page_url_string);
    }
    if (!action.page_title().empty()) {
      statement.BindString(6, action.page_title());
    }
    std::string arg_url_string = action.SerializeArgUrl();
    if (!arg_url_string.empty()) {
      statement.BindString(7, arg_url_string);
    }
    if (action.other()) {
      statement.BindString(8, Util::Serialize(action.other()));
    }

    if (!statement.Run()) {
      LOG(ERROR) << "Activity log database I/O failed: " << sql_str;
      return false;
    }
  }

  if (!transaction.Commit())
    return false;

  queued_actions_.clear();
  return true;
}

std::unique_ptr<Action::ActionVector> FullStreamUIPolicy::DoReadFilteredData(
    const std::string& extension_id,
    const Action::ActionType type,
    const std::string& api_name,
    const std::string& page_url,
    const std::string& arg_url,
    const int days_ago) {
  // Ensure data is flushed to the database first so that we query over all
  // data.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);
  std::unique_ptr<Action::ActionVector> actions(new Action::ActionVector());

  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    return actions;
  }

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
  }
  if (days_ago >= 0)
    where_str += where_next + "time BETWEEN ? AND ?";
  std::string query_str = base::StringPrintf(
      "SELECT extension_id,time,action_type,api_name,args,page_url,page_title,"
      "arg_url,other,rowid FROM %s %s %s ORDER BY time DESC LIMIT 300",
      kTableName,
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
        query.ColumnString(3), query.ColumnInt64(9));

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
    actions->push_back(action);
  }

  return actions;
}

void FullStreamUIPolicy::DoRemoveActions(
    const std::vector<int64_t>& action_ids) {
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
      return;
    }
  }

  if (!transaction.Commit()) {
    LOG(ERROR) << "Removing activities from database failed";
  }
}

void FullStreamUIPolicy::DoRemoveURLs(const std::vector<GURL>& restrict_urls) {
  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  // Make sure any queued in memory are sent to the database before cleaning.
  activity_database()->AdviseFlush(ActivityDatabase::kFlushImmediately);

  // If no restrictions then then all URLs need to be removed.
  if (restrict_urls.empty()) {
    sql::Statement statement;
    std::string sql_str = base::StringPrintf(
        "UPDATE %s SET page_url=NULL,page_title=NULL,arg_url=NULL",
        kTableName);
    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));

    if (!statement.Run()) {
      LOG(ERROR) << "Removing URLs from database failed: "
                 << statement.GetSQLStatement();
    }
    return;
  }

  // If URLs are specified then restrict to only those URLs.
  for (size_t i = 0; i < restrict_urls.size(); ++i) {
    if (!restrict_urls[i].is_valid()) {
      continue;
    }

    // Remove any matching page url info.
    sql::Statement statement;
    std::string sql_str = base::StringPrintf(
      "UPDATE %s SET page_url=NULL,page_title=NULL WHERE page_url=?",
      kTableName);
    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindString(0, restrict_urls[i].spec());

    if (!statement.Run()) {
      LOG(ERROR) << "Removing page URL from database failed: "
                 << statement.GetSQLStatement();
      return;
    }

    // Remove any matching arg urls.
    sql_str = base::StringPrintf("UPDATE %s SET arg_url=NULL WHERE arg_url=?",
                                 kTableName);
    statement.Assign(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
    statement.BindString(0, restrict_urls[i].spec());

    if (!statement.Run()) {
      LOG(ERROR) << "Removing arg URL from database failed: "
                 << statement.GetSQLStatement();
      return;
    }
  }
}

void FullStreamUIPolicy::DoRemoveExtensionData(
    const std::string& extension_id) {
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
      "DELETE FROM %s WHERE extension_id=?", kTableName);
  sql::Statement statement;
  statement.Assign(
      db->GetCachedStatement(sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
  statement.BindString(0, extension_id);
  if (!statement.Run()) {
    LOG(ERROR) << "Removing URLs for extension "
               << extension_id << "from database failed: "
               << statement.GetSQLStatement();
  }
}

void FullStreamUIPolicy::DoDeleteDatabase() {
  sql::Database* db = GetDatabaseConnection();
  if (!db) {
    LOG(ERROR) << "Unable to connect to database";
    return;
  }

  queued_actions_.clear();

  // Not wrapped in a transaction because the deletion should happen even if
  // the vacuuming fails.
  std::string sql_str = base::StringPrintf("DELETE FROM %s;", kTableName);
  sql::Statement statement(db->GetCachedStatement(
      sql::StatementID(SQL_FROM_HERE), sql_str.c_str()));
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

void FullStreamUIPolicy::OnDatabaseFailure() {
  queued_actions_.clear();
}

void FullStreamUIPolicy::OnDatabaseClose() {
  delete this;
}

void FullStreamUIPolicy::Close() {
  ScheduleAndForget(activity_database(), &ActivityDatabase::Close);
}

void FullStreamUIPolicy::ReadFilteredData(
    const std::string& extension_id,
    const Action::ActionType type,
    const std::string& api_name,
    const std::string& page_url,
    const std::string& arg_url,
    const int days_ago,
    base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> callback) {
  base::PostTaskAndReplyWithResult(
      GetActivityLogTaskRunner().get(), FROM_HERE,
      base::BindOnce(&FullStreamUIPolicy::DoReadFilteredData,
                     base::Unretained(this), extension_id, type, api_name,
                     page_url, arg_url, days_ago),
      std::move(callback));
}

void FullStreamUIPolicy::RemoveActions(const std::vector<int64_t>& action_ids) {
  ScheduleAndForget(this, &FullStreamUIPolicy::DoRemoveActions, action_ids);
}

void FullStreamUIPolicy::RemoveURLs(const std::vector<GURL>& restrict_urls) {
  ScheduleAndForget(this, &FullStreamUIPolicy::DoRemoveURLs, restrict_urls);
}

void FullStreamUIPolicy::RemoveExtensionData(const std::string& extension_id) {
  ScheduleAndForget(
      this, &FullStreamUIPolicy::DoRemoveExtensionData, extension_id);
}

void FullStreamUIPolicy::DeleteDatabase() {
  ScheduleAndForget(this, &FullStreamUIPolicy::DoDeleteDatabase);
}

scoped_refptr<Action> FullStreamUIPolicy::ProcessArguments(
    scoped_refptr<Action> action) const {
  return action;
}

void FullStreamUIPolicy::ProcessAction(scoped_refptr<Action> action) {
  // TODO(mvrable): Right now this argument stripping updates the Action object
  // in place, which isn't good if there are other users of the object.  When
  // database writing is moved to policy class, the modifications should be
  // made locally.
  action = ProcessArguments(action);
  ScheduleAndForget(this, &FullStreamUIPolicy::QueueAction, action);
}

void FullStreamUIPolicy::QueueAction(scoped_refptr<Action> action) {
  if (activity_database()->is_db_valid()) {
    queued_actions_.push_back(action);
    activity_database()->AdviseFlush(queued_actions_.size());
  }
}

}  // namespace extensions
