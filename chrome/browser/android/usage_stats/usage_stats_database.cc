// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "content/public/browser/storage_partition.h"

namespace usage_stats {

using leveldb_proto::ProtoDatabaseProvider;

const char kNamespace[] = "UsageStats";
const char kEventsDbName[] = "Events";
const char kSuspensionsDbName[] = "Suspensions";
const char kTokensDbName[] = "Tokens";

const char kKeySeparator[] = "_";

const int kUnixTimeDigits = 11;
// Formats an integer with a minimum width of 11, right-justified, and
// zero-filled (example: 1548353315 => 01548353315).
const char kUnixTimeFormat[] = "%011d";

UsageStatsDatabase::UsageStatsDatabase(Profile* profile)
    : website_event_db_initialized_(false),
      suspension_db_initialized_(false),
      token_mapping_db_initialized_(false) {
  ProtoDatabaseProvider* db_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  base::FilePath usage_stats_dir = profile->GetPath().Append(kNamespace);

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  website_event_db_ = db_provider->GetDB<WebsiteEvent>(
      leveldb_proto::ProtoDbType::USAGE_STATS_WEBSITE_EVENT,
      usage_stats_dir.Append(kEventsDbName), db_task_runner);

  suspension_db_ = db_provider->GetDB<Suspension>(
      leveldb_proto::ProtoDbType::USAGE_STATS_SUSPENSION,
      usage_stats_dir.Append(kSuspensionsDbName), db_task_runner);

  token_mapping_db_ = db_provider->GetDB<TokenMapping>(
      leveldb_proto::ProtoDbType::USAGE_STATS_TOKEN_MAPPING,
      usage_stats_dir.Append(kTokensDbName), db_task_runner);

  InitializeDBs();
  ExpireEvents(base::Time::NowFromSystemTime());
}

UsageStatsDatabase::UsageStatsDatabase(
    std::unique_ptr<ProtoDatabase<WebsiteEvent>> website_event_db,
    std::unique_ptr<ProtoDatabase<Suspension>> suspension_db,
    std::unique_ptr<ProtoDatabase<TokenMapping>> token_mapping_db)
    : website_event_db_(std::move(website_event_db)),
      suspension_db_(std::move(suspension_db)),
      token_mapping_db_(std::move(token_mapping_db)),
      website_event_db_initialized_(false),
      suspension_db_initialized_(false),
      token_mapping_db_initialized_(false) {
  InitializeDBs();
}

UsageStatsDatabase::~UsageStatsDatabase() = default;

namespace {

bool DoesNotContainFilter(const base::flat_set<std::string>& set,
                          const std::string& key) {
  return !set.contains(key);
}

bool KeyContainsDomainFilter(const base::flat_set<std::string>& domains,
                             const std::string& key) {
  return domains.contains(key.substr(kUnixTimeDigits + 1));
}

UsageStatsDatabase::Error ToError(bool isSuccess) {
  return isSuccess ? UsageStatsDatabase::Error::kNoError
                   : UsageStatsDatabase::Error::kUnknownError;
}

std::string CreateWebsiteEventKey(int64_t seconds_since_unix_epoch,
                                  const std::string& fqdn) {
  // Zero-pad |seconds_since_unix_epoch|. Allows ascending timestamps
  // to sort lexicographically, supporting efficient range queries by key.
  char unixTime[kUnixTimeDigits + 1];
  ssize_t printed = base::strings::SafeSPrintf(unixTime, kUnixTimeFormat,
                                               seconds_since_unix_epoch);
  DCHECK(printed == kUnixTimeDigits);

  // Create the key from the time and fqdn (example: 01548276551_foo.com).
  return base::StrCat({unixTime, kKeySeparator, fqdn});
}

}  // namespace

void UsageStatsDatabase::InitializeDBs() {
  // Asynchronously initialize databases.
  website_event_db_->Init(
      base::BindOnce(&UsageStatsDatabase::OnWebsiteEventInitDone,
                     weak_ptr_factory_.GetWeakPtr(), true));

  suspension_db_->Init(base::BindOnce(&UsageStatsDatabase::OnSuspensionInitDone,
                                      weak_ptr_factory_.GetWeakPtr(), true));

  token_mapping_db_->Init(
      base::BindOnce(&UsageStatsDatabase::OnTokenMappingInitDone,
                     weak_ptr_factory_.GetWeakPtr(), true));
}

void UsageStatsDatabase::GetAllEvents(EventsCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::GetAllEvents,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Load all WebsiteEvents.
  website_event_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllEvents,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::QueryEventsInRange(base::Time startTime,
                                            base::Time endTime,
                                            EventsCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(base::BindOnce(
        &UsageStatsDatabase::QueryEventsInRange, weak_ptr_factory_.GetWeakPtr(),
        startTime, endTime, std::move(callback)));
    return;
  }

  // Load all WebsiteEvents where the timestamp is in the specified range.
  // Function accepts a half-open range [startTime, endTime) as input, but the
  // database operates on fully-closed ranges. Because the timestamps are
  // represented by integers, [startTime, endTime) is equivalent to  [startTime,
  // endTime - 1].
  website_event_db_->LoadKeysAndEntriesInRange(
      CreateWebsiteEventKey(startTime.InSecondsFSinceUnixEpoch(), ""),
      CreateWebsiteEventKey(endTime.InSecondsFSinceUnixEpoch() - 1, ""),
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForQueryEventsInRange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::AddEvents(std::vector<WebsiteEvent> events,
                                   StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(base::BindOnce(
        &UsageStatsDatabase::AddEvents, weak_ptr_factory_.GetWeakPtr(),
        std::move(events), std::move(callback)));
    return;
  }

  auto entries =
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>();
  entries->reserve(events.size());

  for (WebsiteEvent event : events) {
    std::string key =
        CreateWebsiteEventKey(event.timestamp().seconds(), event.fqdn());

    entries->emplace_back(key, event);
  }

  // Add all entries created from input vector.
  website_event_db_->UpdateEntries(
      std::move(entries), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteAllEvents(StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::DeleteAllEvents,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Remove all WebsiteEvent entries.
  website_event_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteEventsInRange(base::Time startTime,
                                             base::Time endTime,
                                             StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::DeleteEventsInRange,
                       weak_ptr_factory_.GetWeakPtr(), startTime, endTime,
                       std::move(callback)));
    return;
  }

  // If leveldb_proto adds a DeleteEntriesInRange function, these two proto_db_
  // calls could be consolidated into a single call (crbug.com/939136).

  // Load all WebsiteEvents where the timestamp is in the specified range.
  // Function accepts a half-open range [startTime, endTime) as input, but the
  // database operates on fully-closed ranges. Because the timestamps are
  // represented by integers, [startTime, endTime) is equivalent to  [startTime,
  // endTime - 1].
  website_event_db_->LoadKeysAndEntriesInRange(
      CreateWebsiteEventKey(startTime.InSecondsFSinceUnixEpoch(), ""),
      CreateWebsiteEventKey(endTime.InSecondsFSinceUnixEpoch() - 1, ""),
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForDeleteEventsInRange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteEventsWithMatchingDomains(
    base::flat_set<std::string> domains,
    StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!website_event_db_initialized_) {
    website_event_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::DeleteEventsWithMatchingDomains,
                       weak_ptr_factory_.GetWeakPtr(), std::move(domains),
                       std::move(callback)));
    return;
  }

  // Remove all events with domains in the given set.
  website_event_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
      base::BindRepeating(&KeyContainsDomainFilter, std::move(domains)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::ExpireEvents(base::Time now) {
  base::Time seven_days_ago = now - base::Days(EXPIRY_THRESHOLD_DAYS);
  DeleteEventsInRange(
      base::Time::FromSecondsSinceUnixEpoch(1), seven_days_ago,
      base::BindOnce(&UsageStatsDatabase::OnWebsiteEventExpiryDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UsageStatsDatabase::GetAllSuspensions(SuspensionsCallback callback) {
  if (!suspension_db_initialized_) {
    // Defer execution if database is uninitialized.
    suspension_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::GetAllSuspensions,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Load all Suspensions.
  suspension_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetSuspensions(base::flat_set<std::string> domains,
                                        StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!suspension_db_initialized_) {
    suspension_db_callbacks_.emplace(base::BindOnce(
        &UsageStatsDatabase::SetSuspensions, weak_ptr_factory_.GetWeakPtr(),
        std::move(domains), std::move(callback)));
    return;
  }

  auto entries = std::make_unique<ProtoDatabase<Suspension>::KeyEntryVector>();

  for (std::string domain : domains) {
    Suspension suspension;
    suspension.set_fqdn(domain);

    entries->emplace_back(domain, suspension);
  }

  // Add all entries created from domain set, remove all entries not in the set.
  suspension_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(domains)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::GetAllTokenMappings(TokenMappingsCallback callback) {
  // Defer execution if database is uninitialized.
  if (!token_mapping_db_initialized_) {
    token_mapping_db_callbacks_.emplace(
        base::BindOnce(&UsageStatsDatabase::GetAllTokenMappings,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Load all TokenMappings.
  token_mapping_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetTokenMappings(TokenMap mappings,
                                          StatusCallback callback) {
  // Defer execution if database is uninitialized.
  if (!token_mapping_db_initialized_) {
    token_mapping_db_callbacks_.emplace(base::BindOnce(
        &UsageStatsDatabase::SetTokenMappings, weak_ptr_factory_.GetWeakPtr(),
        std::move(mappings), std::move(callback)));
    return;
  }

  std::vector<std::string> keys;
  keys.reserve(mappings.size());

  auto entries =
      std::make_unique<ProtoDatabase<TokenMapping>::KeyEntryVector>();

  for (const auto& mapping : mappings) {
    std::string token = mapping.first;
    std::string fqdn = mapping.second;

    keys.emplace_back(token);

    TokenMapping token_mapping;
    token_mapping.set_token(token);
    token_mapping.set_fqdn(fqdn);

    entries->emplace_back(token, token_mapping);
  }

  auto key_set = base::flat_set<std::string>(keys);

  // Add all entries created from map, remove all entries not in the map.
  token_mapping_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(key_set)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::OnWebsiteEventInitDone(
    bool retry,
    leveldb_proto::Enums::InitStatus status) {
  website_event_db_initialized_ =
      status == leveldb_proto::Enums::InitStatus::kOK;

  if (!website_event_db_initialized_) {
    if (retry) {
      // Retry unsuccessful initialization.
      website_event_db_->Init(
          base::BindOnce(&UsageStatsDatabase::OnWebsiteEventInitDone,
                         weak_ptr_factory_.GetWeakPtr(), false));
    }
    return;
  }

  // Execute deferred operations on sucessfully initialized database.
  while (!website_event_db_callbacks_.empty()) {
    std::move(website_event_db_callbacks_.front()).Run();
    website_event_db_callbacks_.pop();
  }
}

void UsageStatsDatabase::OnSuspensionInitDone(
    bool retry,
    leveldb_proto::Enums::InitStatus status) {
  suspension_db_initialized_ = status == leveldb_proto::Enums::InitStatus::kOK;

  if (!suspension_db_initialized_) {
    if (retry) {
      // Retry unsuccessful initialization.
      suspension_db_->Init(
          base::BindOnce(&UsageStatsDatabase::OnSuspensionInitDone,
                         weak_ptr_factory_.GetWeakPtr(), false));
    }
    return;
  }

  // Execute deferred operations on sucessfully initialized database.
  while (!suspension_db_callbacks_.empty()) {
    std::move(suspension_db_callbacks_.front()).Run();
    suspension_db_callbacks_.pop();
  }
}

void UsageStatsDatabase::OnTokenMappingInitDone(
    bool retry,
    leveldb_proto::Enums::InitStatus status) {
  token_mapping_db_initialized_ =
      status == leveldb_proto::Enums::InitStatus::kOK;

  if (!token_mapping_db_initialized_) {
    if (retry) {
      // Retry unsuccessful initialization.
      token_mapping_db_->Init(
          base::BindOnce(&UsageStatsDatabase::OnTokenMappingInitDone,
                         weak_ptr_factory_.GetWeakPtr(), false));
    }
    return;
  }

  // Execute deferred operations on sucessfully initialized database.
  while (!token_mapping_db_callbacks_.empty()) {
    std::move(token_mapping_db_callbacks_.front()).Run();
    token_mapping_db_callbacks_.pop();
  }
}

void UsageStatsDatabase::OnWebsiteEventExpiryDone(Error error) {}

void UsageStatsDatabase::OnUpdateEntries(StatusCallback callback,
                                         bool isSuccess) {
  std::move(callback).Run(ToError(isSuccess));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllEvents(
    EventsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<WebsiteEvent>> events) {
  if (isSuccess && events) {
    std::move(callback).Run(ToError(isSuccess), *events);
  } else {
    std::move(callback).Run(ToError(isSuccess), std::vector<WebsiteEvent>());
  }
}

void UsageStatsDatabase::OnLoadEntriesForQueryEventsInRange(
    EventsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map) {
  std::vector<WebsiteEvent> results;

  if (event_map) {
    results.reserve(event_map->size());
    for (const auto& entry : *event_map) {
      results.emplace_back(entry.second);
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

void UsageStatsDatabase::OnLoadEntriesForDeleteEventsInRange(
    StatusCallback callback,
    bool isSuccess,
    std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map) {
  if (isSuccess && event_map) {
    // Collect keys found in range to be deleted.
    auto keys_to_delete = std::make_unique<std::vector<std::string>>();
    keys_to_delete->reserve(event_map->size());

    for (const auto& entry : *event_map) {
      keys_to_delete->emplace_back(entry.first);
    }

    // Remove all entries found in range.
    website_event_db_->UpdateEntries(
        std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
        std::move(keys_to_delete),
        base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(ToError(isSuccess));
  }
}

void UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions(
    SuspensionsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<Suspension>> suspensions) {
  std::vector<std::string> results;

  if (suspensions) {
    results.reserve(suspensions->size());
    for (Suspension suspension : *suspensions) {
      results.emplace_back(suspension.fqdn());
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings(
    TokenMappingsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<TokenMapping>> mappings) {
  TokenMap results;

  if (mappings) {
    for (TokenMapping mapping : *mappings) {
      results.emplace(mapping.token(), mapping.fqdn());
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

}  // namespace usage_stats
