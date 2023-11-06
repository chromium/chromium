// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/budget_database.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/budget.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/site_engagement/content/site_engagement_score.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserThread;

namespace {

// The default amount of time during which a budget will be valid.
constexpr int kBudgetDurationInDays = 4;

// The amount of budget that a maximally engaged site should receive per hour.
// For context, silent push messages cost 2 each, so this allows 6 silent push
// messages a day for a fully engaged site. See budget_manager.cc for costs of
// various actions.
constexpr double kMaximumHourlyBudget = 12.0 / 24.0;

}  // namespace

BudgetState::BudgetState() = default;
BudgetState::BudgetState(const BudgetState& other) = default;
BudgetState::~BudgetState() = default;

BudgetState& BudgetState::operator=(const BudgetState& other) = default;

BudgetDatabase::BudgetInfo::BudgetInfo() = default;

BudgetDatabase::BudgetInfo::BudgetInfo(const BudgetInfo&& other)
    : last_engagement_award(other.last_engagement_award) {
  chunks = std::move(other.chunks);
}

BudgetDatabase::BudgetInfo::~BudgetInfo() = default;

BudgetDatabase::BudgetDatabase(Profile* profile)
    : profile_(profile), clock_(base::WrapUnique(new base::DefaultClock)) {
  auto* protodb_provider =
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider();
  // In incognito mode the provider service is not created.
  if (!protodb_provider)
    return;

  db_ = protodb_provider->GetDB<budget_service::Budget>(
      leveldb_proto::ProtoDbType::BUDGET_DATABASE,
      profile->GetPath().Append(FILE_PATH_LITERAL("BudgetDatabase")),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
  db_->Init(base::BindOnce(&BudgetDatabase::OnDatabaseInit,
                           weak_ptr_factory_.GetWeakPtr()));
}

BudgetDatabase::~BudgetDatabase() = default;

void BudgetDatabase::GetBudgetDetails(const url::Origin& origin,
                                      GetBudgetCallback callback) {
  SyncCache(origin, base::BindOnce(&BudgetDatabase::GetBudgetAfterSync,
                                   weak_ptr_factory_.GetWeakPtr(), origin,
                                   std::move(callback)));
}

void BudgetDatabase::SpendBudget(const url::Origin& origin,
                                 SpendBudgetCallback callback,
                                 double amount) {
  SyncCache(origin, base::BindOnce(&BudgetDatabase::SpendBudgetAfterSync,
                                   weak_ptr_factory_.GetWeakPtr(), origin,
                                   amount, std::move(callback)));
}

void BudgetDatabase::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void BudgetDatabase::OnDatabaseInit(leveldb_proto::Enums::InitStatus status) {
  // TODO(harkness): Consider caching the budget database now?
  if (status != leveldb_proto::Enums::InitStatus::kOK)
    db_.reset();
}

bool BudgetDatabase::IsCached(const url::Origin& origin) const {
  return budget_map_.find(origin) != budget_map_.end();
}

double BudgetDatabase::GetBudget(const url::Origin& origin) const {
  double total = 0;
  auto iter = budget_map_.find(origin);
  if (iter == budget_map_.end())
    return total;

  const BudgetInfo& info = iter->second;
  for (const BudgetChunk& chunk : info.chunks)
    total += chunk.amount;
  return total;
}

void BudgetDatabase::AddToCache(
    const url::Origin& origin,
    CacheCallback callback,
    bool success,
    std::unique_ptr<budget_service::Budget> budget_proto) {
  // If the database read failed or there's nothing to add, just return.
  if (!success || !budget_proto) {
    std::move(callback).Run(success);
    return;
  }

  // If there were two simultaneous loads, don't overwrite the cache value,
  // which might have been updated after the previous load.
  if (IsCached(origin)) {
    std::move(callback).Run(success);
    return;
  }

  // Add the data to the cache, converting from the proto format to an STL
  // format which is better for removing things from the list.
  BudgetInfo& info = budget_map_[origin];
  for (const auto& chunk : budget_proto->budget()) {
    info.chunks.emplace_back(chunk.amount(),
                             base::Time::FromInternalValue(chunk.expiration()));
  }

  info.last_engagement_award =
      base::Time::FromInternalValue(budget_proto->engagement_last_updated());

  std::move(callback).Run(success);
}

void BudgetDatabase::GetBudgetAfterSync(const url::Origin& origin,
                                        GetBudgetCallback callback,
                                        bool success) {
  std::vector<BudgetState> predictions;

  // If the database wasn't able to read the information, return the
  // failure and an empty predictions array.
  if (!success) {
    std::move(callback).Run(std::move(predictions));
    return;
  }

  // Now, build up the BudgetExpection. This is different from the format
  // in which the cache stores the data. The cache stores chunks of budget and
  // when that budget expires. The mojo array describes a set of times
  // and the budget at those times.
  double total = GetBudget(origin);

  // Always add one entry at the front of the list for the total budget now.
  {
    BudgetState prediction;
    prediction.budget_at = total;
    prediction.time = clock_->Now().InMillisecondsFSinceUnixEpoch();
    predictions.push_back(prediction);
  }

  // Starting with the soonest expiring chunks, add entries for the
  // expiration times going forward.
  const BudgetChunks& chunks = budget_map_[origin].chunks;
  for (const auto& chunk : chunks) {
    BudgetState prediction;
    total -= chunk.amount;
    prediction.budget_at = total;
    prediction.time = chunk.expiration.InMillisecondsFSinceUnixEpoch();
    predictions.push_back(prediction);
  }

  std::move(callback).Run(std::move(predictions));
}

void BudgetDatabase::SpendBudgetAfterSync(const url::Origin& origin,
                                          double amount,
                                          SpendBudgetCallback callback,
                                          bool success) {
  if (!success) {
    std::move(callback).Run(false /* success */);
    return;
  }

  // Walk the list of budget chunks to see if the origin has enough budget.
  double total = 0;
  BudgetInfo& info = budget_map_[origin];
  for (const BudgetChunk& chunk : info.chunks)
    total += chunk.amount;

  if (total < amount) {
    std::move(callback).Run(false /* success */);
    return;
  }

  // Walk the chunks and remove enough budget to cover the needed amount.
  double bill = amount;
  for (auto iter = info.chunks.begin(); iter != info.chunks.end();) {
    if (iter->amount > bill) {
      iter->amount -= bill;
      bill = 0;
      break;
    }
    bill -= iter->amount;
    iter = info.chunks.erase(iter);
  }

  // There should have been enough budget to cover the entire bill.
  DCHECK_EQ(0, bill);

  // Now that the cache is updated, write the data to the database.
  WriteCachedValuesToDatabase(
      origin,
      base::BindOnce(&BudgetDatabase::SpendBudgetAfterWrite,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

// This converts the bool value which is returned from the database to a Mojo
// error type.
void BudgetDatabase::SpendBudgetAfterWrite(SpendBudgetCallback callback,
                                           bool write_successful) {
  // TODO(harkness): If the database write fails, the cache will be out of sync
  // with the database. Consider ways to mitigate this.
  if (!write_successful) {
    std::move(callback).Run(false /* success */);
    return;
  }
  std::move(callback).Run(true /* success */);
}

void BudgetDatabase::WriteCachedValuesToDatabase(const url::Origin& origin,
                                                 StoreBudgetCallback callback) {
  if (!db_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  // Create the data structures that are passed to the ProtoDatabase.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<budget_service::Budget>::KeyEntryVector>
      entries(new leveldb_proto::ProtoDatabase<
              budget_service::Budget>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  // Each operation can either update the existing budget or remove the origin's
  // budget information.
  if (IsCached(origin)) {
    // Build the Budget proto object.
    budget_service::Budget budget;
    const BudgetInfo& info = budget_map_[origin];
    for (const auto& chunk : info.chunks) {
      budget_service::BudgetChunk* budget_chunk = budget.add_budget();
      budget_chunk->set_amount(chunk.amount);
      budget_chunk->set_expiration(chunk.expiration.ToInternalValue());
    }
    budget.set_engagement_last_updated(
        info.last_engagement_award.ToInternalValue());
    entries->push_back(std::make_pair(origin.Serialize(), budget));
  } else {
    // If the origin doesn't exist in the cache, this is a remove operation.
    keys_to_remove->push_back(origin.Serialize());
  }

  // Send the updates to the database.
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove),
                     std::move(callback));
}

void BudgetDatabase::SyncCache(const url::Origin& origin,
                               CacheCallback callback) {
  // If the origin isn't already cached, add it to the cache.
  if (!IsCached(origin)) {
    if (!db_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), false));
      return;
    }
    CacheCallback add_callback = base::BindOnce(
        &BudgetDatabase::SyncLoadedCache, weak_ptr_factory_.GetWeakPtr(),
        origin, std::move(callback));
    db_->GetEntry(origin.Serialize(),
                  base::BindOnce(&BudgetDatabase::AddToCache,
                                 weak_ptr_factory_.GetWeakPtr(), origin,
                                 std::move(add_callback)));
    return;
  }
  SyncLoadedCache(origin, std::move(callback), true /* success */);
}

void BudgetDatabase::SyncLoadedCache(const url::Origin& origin,
                                     CacheCallback callback,
                                     bool success) {
  if (!success) {
    std::move(callback).Run(false /* success */);
    return;
  }

  // Now, cleanup any expired budget chunks for the origin.
  bool needs_write = CleanupExpiredBudget(origin);

  // Get the SES score and add engagement budget for the site.
  AddEngagementBudget(origin);

  if (needs_write)
    WriteCachedValuesToDatabase(origin, std::move(callback));
  else
    std::move(callback).Run(success);
}

void BudgetDatabase::AddEngagementBudget(const url::Origin& origin) {
  // Calculate how much budget should be awarded. The award depends on the
  // time elapsed since the last award and the SES score.
  // By default, give the origin kBudgetDurationInDays worth of budget, but
  // reduce that if budget has already been given during that period.
  base::TimeDelta elapsed = base::Days(kBudgetDurationInDays);
  if (IsCached(origin)) {
    elapsed = clock_->Now() - budget_map_[origin].last_engagement_award;
    // Don't give engagement awards for periods less than an hour.
    if (elapsed.InHours() < 1)
      return;
    // Cap elapsed time to the budget duration.
    if (elapsed.InDays() > kBudgetDurationInDays)
      elapsed = base::Days(kBudgetDurationInDays);
  }

  // Get the current SES score, and calculate the hourly budget for that score.
  double hourly_budget = kMaximumHourlyBudget *
                         GetSiteEngagementScoreForOrigin(origin) /
                         site_engagement::SiteEngagementService::GetMaxPoints();

  // Update the last_engagement_award to the current time. If the origin wasn't
  // already in the map, this adds a new entry for it.
  budget_map_[origin].last_engagement_award = clock_->Now();

  // Add a new chunk of budget for the origin at the default expiration time.
  base::Time expiration = clock_->Now() + base::Days(kBudgetDurationInDays);
  budget_map_[origin].chunks.emplace_back(elapsed.InHours() * hourly_budget,
                                          expiration);
}

// Cleans up budget in the cache. Relies on the caller eventually writing the
// cache back to the database.
bool BudgetDatabase::CleanupExpiredBudget(const url::Origin& origin) {
  if (!IsCached(origin))
    return false;

  base::Time now = clock_->Now();
  BudgetChunks& chunks = budget_map_[origin].chunks;
  auto cleanup_iter = chunks.begin();

  // This relies on the list of chunks being in timestamp order.
  while (cleanup_iter != chunks.end() && cleanup_iter->expiration <= now)
    cleanup_iter = chunks.erase(cleanup_iter);

  // If the entire budget is empty now AND there have been no engagements
  // in the last kBudgetDurationInDays days, remove this from the cache.
  if (chunks.empty() && budget_map_[origin].last_engagement_award <
                            clock_->Now() - base::Days(kBudgetDurationInDays)) {
    budget_map_.erase(origin);
    return true;
  }

  // Although some things may have expired, there are some chunks still valid.
  // Don't write to the DB now, write either when all chunks expire or when the
  // origin spends some budget.
  return false;
}

double BudgetDatabase::GetSiteEngagementScoreForOrigin(
    const url::Origin& origin) const {
  if (profile_->IsOffTheRecord())
    return 0;

  return site_engagement::SiteEngagementService::Get(profile_)->GetScore(
      origin.GetURL());
}
