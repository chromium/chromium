// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_

#include <stdint.h>

#include <string>

#include "chrome/browser/extensions/activity_log/activity_database.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"

class GURL;

namespace extensions {

// A policy for logging the full stream of actions, including all arguments.
// It's mostly intended to be used in testing and analysis.
//
// NOTE: The FullStreamUIPolicy deliberately keeps almost all information,
// including some data that could be privacy sensitive (full URLs including
// incognito URLs, full headers when WebRequest is used, etc.).  It should not
// be used during normal browsing if users care about privacy.
class FullStreamUIPolicy : public ActivityLogDatabasePolicy {
 public:
  // For more info about these member functions, see the super class.
  explicit FullStreamUIPolicy(Profile* profile);

  void ProcessAction(scoped_refptr<Action> action) override;

  void ReadFilteredData(
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> callback)
      override;

  void Close() override;

  // Remove the actions stored for this policy according to the passed IDs.
  void RemoveActions(const std::vector<int64_t>& action_ids) override;

  // Clean the URL data stored for this policy.
  void RemoveURLs(const std::vector<GURL>& restrict_urls) override;

  // Clean the data related to this extension for this policy.
  void RemoveExtensionData(const std::string& extension_id) override;

  // Delete everything in the database.
  void DeleteDatabase() override;

  // Database table schema.
  static const char* const kTableContentFields[];
  static const char* const kTableFieldTypes[];
  static const int kTableFieldCount;

 protected:
  // Only ever run by OnDatabaseClose() below; see the comments on the
  // ActivityDatabase class for an overall discussion of how cleanup works.
  ~FullStreamUIPolicy() override;

  // The ActivityDatabase::Delegate interface.  These are always called from
  // the database thread.
  bool InitDatabase(sql::Database* db) override;
  bool FlushDatabase(sql::Database* db) override;
  void OnDatabaseFailure() override;
  void OnDatabaseClose() override;

  // Strips arguments if needed by policy.  May return the original object (if
  // unmodified), or a copy (if modifications were made).  The implementation
  // in FullStreamUIPolicy returns the action unmodified.
  virtual scoped_refptr<Action> ProcessArguments(
      scoped_refptr<Action> action) const;

  // The implementation of RemoveActions; this must only run on the database
  // thread.
  void DoRemoveActions(const std::vector<int64_t>& action_ids);

  // The implementation of RemoveURLs; this must only run on the database
  // thread.
  void DoRemoveURLs(const std::vector<GURL>& restrict_urls);

  // The implementation of RemoveExtensionData; this must only run on the
  // database thread.
  void DoRemoveExtensionData(const std::string& extension_id);

  // The implementation of DeleteDatabase; this must only run on the database
  // thread.
  void DoDeleteDatabase();

  // Tracks any pending updates to be written to the database, if write
  // batching is turned on.  Should only be accessed from the database thread.
  Action::ActionVector queued_actions_;

 private:
  // Adds an Action to queued_actions_; this should be invoked only on the
  // database thread.
  void QueueAction(scoped_refptr<Action> action);

  // Internal method to read data from the database; called on the database
  // thread.
  std::unique_ptr<Action::ActionVector> DoReadFilteredData(
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_FULLSTREAM_UI_POLICY_H_
