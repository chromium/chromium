// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
#define CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"

class Profile;

namespace usage_stats {

class WebsiteEvent;
class Suspension;
class TokenMapping;

using leveldb_proto::ProtoDatabase;

// Stores website events, suspensions and token to fully-qualified domain name
// (FQDN) mappings in LevelDB.
class UsageStatsDatabase {
 public:
  enum class Error { kNoError, kUnknownError };

  using EventsCallback =
      base::OnceCallback<void(Error, std::vector<WebsiteEvent>)>;

  using SuspensionsCallback =
      base::OnceCallback<void(Error, std::vector<std::string>)>;

  using TokenMap = std::map<std::string, std::string>;
  using TokenMappingsCallback = base::OnceCallback<void(Error, TokenMap)>;

  using StatusCallback = base::OnceCallback<void(Error)>;

  // Digital Wellbeing doesn't show activity older than a week, so 7 days is the
  // max age for event retention.
  constexpr static int EXPIRY_THRESHOLD_DAYS = 7;

  // Initializes the database with user |profile|.
  explicit UsageStatsDatabase(Profile* profile);

  ~UsageStatsDatabase();

  void GetAllEvents(EventsCallback callback);

  // Get all events in range between |startTime| (inclusive) and |endTime|
  // (exclusive) at second-level granularity.
  void QueryEventsInRange(base::Time startTime,
                          base::Time endTime,
                          EventsCallback callback);

  void AddEvents(std::vector<WebsiteEvent> events, StatusCallback callback);

  void DeleteAllEvents(StatusCallback callback);

  // Delete all events in range between |startTime| (inclusive) and
  // |endTime| (exclusive) at second-level granularity.
  void DeleteEventsInRange(base::Time startTime,
                           base::Time endTime,
                           StatusCallback callback);

  void DeleteEventsWithMatchingDomains(base::flat_set<std::string> domains,
                                       StatusCallback callback);

  // Delete events older than EXPIRY_THRESHOLD_DAYS.
  void ExpireEvents(base::Time now);

  void GetAllSuspensions(SuspensionsCallback callback);

  // Persists all the suspensions in |domains| and deletes any suspensions *not*
  // in |domains|.
  void SetSuspensions(base::flat_set<std::string> domains,
                      StatusCallback callback);

  void GetAllTokenMappings(TokenMappingsCallback callback);

  // Persists all the mappings in |mappings| and deletes any mappings *not* in
  // |mappings|. The map's key is the token, and its value is the FQDN.
  void SetTokenMappings(TokenMap mappings, StatusCallback callback);

 protected:
  // For testing only.
  UsageStatsDatabase(
      std::unique_ptr<ProtoDatabase<WebsiteEvent>> website_event_db,
      std::unique_ptr<ProtoDatabase<Suspension>> suspension_db,
      std::unique_ptr<ProtoDatabase<TokenMapping>> token_mapping_db);

 private:
  void InitializeDBs();

  void OnWebsiteEventInitDone(bool retry,
                              leveldb_proto::Enums::InitStatus status);

  void OnSuspensionInitDone(bool retry,
                            leveldb_proto::Enums::InitStatus status);

  void OnTokenMappingInitDone(bool retry,
                              leveldb_proto::Enums::InitStatus status);

  void OnWebsiteEventExpiryDone(Error error);

  void OnUpdateEntries(StatusCallback callback, bool isSuccess);

  void OnLoadEntriesForGetAllEvents(
      EventsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<WebsiteEvent>> stats);

  void OnLoadEntriesForQueryEventsInRange(
      EventsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map);

  void OnLoadEntriesForDeleteEventsInRange(
      StatusCallback callback,
      bool isSuccess,
      std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map);

  void OnLoadEntriesForGetAllSuspensions(
      SuspensionsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<Suspension>> suspensions);

  void OnLoadEntriesForGetAllTokenMappings(
      TokenMappingsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<TokenMapping>> mappings);

  std::unique_ptr<ProtoDatabase<WebsiteEvent>> website_event_db_;
  std::unique_ptr<ProtoDatabase<Suspension>> suspension_db_;
  std::unique_ptr<ProtoDatabase<TokenMapping>> token_mapping_db_;

  // Track initialization state of proto databases.
  bool website_event_db_initialized_;
  bool suspension_db_initialized_;
  bool token_mapping_db_initialized_;

  // Store callbacks for delayed execution once database is initialized.
  base::queue<base::OnceClosure> website_event_db_callbacks_;
  base::queue<base::OnceClosure> suspension_db_callbacks_;
  base::queue<base::OnceClosure> token_mapping_db_callbacks_;

  base::WeakPtrFactory<UsageStatsDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsageStatsDatabase);
};

}  // namespace usage_stats

#endif  // CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
