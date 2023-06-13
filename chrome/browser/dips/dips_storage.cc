// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_utils.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace {

inline void UmaHistogramTimeToInteraction(base::TimeDelta sample,
                                          DIPSCookieMode mode) {
  const std::string name = base::StrCat(
      {"Privacy.DIPS.TimeFromStorageToInteraction", GetHistogramSuffix(mode)});

  base::UmaHistogramCustomTimes(name, sample,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::Days(7), 100);
}

inline void UmaHistogramTimeToStorage(base::TimeDelta sample,
                                      DIPSCookieMode mode) {
  const std::string name = base::StrCat(
      {"Privacy.DIPS.TimeFromInteractionToStorage", GetHistogramSuffix(mode)});

  base::UmaHistogramCustomTimes(name, sample,
                                /*min=*/base::TimeDelta(),
                                /*max=*/base::Days(7), 100);
}

// The number of sites to process in each call to DIPSStorage::Prepopulate().
// Intended to be constant; settable only for testing.
size_t g_prepopulate_chunk_size = 100;

}  // namespace

DIPSStorage::PrepopulateArgs::PrepopulateArgs(base::Time time,
                                              size_t offset,
                                              std::vector<std::string> sites,
                                              base::OnceClosure on_complete)
    : time(time),
      offset(offset),
      sites(std::move(sites)),
      on_complete(std::move(on_complete)) {}

DIPSStorage::PrepopulateArgs::PrepopulateArgs(PrepopulateArgs&&) = default;

DIPSStorage::PrepopulateArgs::~PrepopulateArgs() = default;

DIPSStorage::DIPSStorage(const absl::optional<base::FilePath>& path)
    : db_(std::make_unique<DIPSDatabase>(path)) {
  base::AssertLongCPUWorkAllowed();
}

DIPSStorage::~DIPSStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// DIPSDatabase interaction functions ------------------------------------------

DIPSState DIPSStorage::Read(const GURL& url) {
  return ReadSite(GetSiteForDIPS(url));
}

DIPSState DIPSStorage::ReadSite(std::string site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  absl::optional<StateValue> state = db_->Read(site);

  if (state.has_value()) {
    // We should not have entries in the DB without any timestamps.
    DCHECK(state->site_storage_times.has_value() ||
           state->user_interaction_times.has_value() ||
           state->stateful_bounce_times.has_value() ||
           state->bounce_times.has_value());

    return DIPSState(this, std::move(site), state.value());
  }
  return DIPSState(this, std::move(site));
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  db_->Write(state.site(), state.site_storage_times(),
             state.user_interaction_times(), state.stateful_bounce_times(),
             state.bounce_times());
}

void DIPSStorage::RemoveEvents(base::Time delete_begin,
                               base::Time delete_end,
                               network::mojom::ClearDataFilterPtr filter,
                               const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DCHECK(delete_end.is_null() || delete_begin <= delete_end);

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (filter.is_null()) {
    db_->RemoveEventsByTime(delete_begin, delete_end, type);
  } else if (type == DIPSEventRemovalType::kStorage &&
             filter->origins.empty()) {
    // Site-filtered deletion is only supported for cookie-related
    // DIPS events, since only cookie deletion allows domains but not hosts.
    //
    // TODO(jdh): Assess the use of cookie deletions with both a time range and
    // a list of domains to determine whether supporting time ranges here is
    // necessary.
    // Time ranges aren't currently supported for site-filtered
    // deletion of DIPS Events.
    if (delete_begin != base::Time::Min() || delete_end != base::Time::Max()) {
      // TODO (kaklilu@): Add a UMA metric to record if this happens.
      return;
    }

    bool preserve =
        (filter->type == network::mojom::ClearDataFilter::Type::KEEP_MATCHES);
    std::vector<std::string> sites = std::move(filter->domains);

    db_->RemoveEventsBySite(preserve, sites, type);
  }
}

void DIPSStorage::RemoveRows(const std::vector<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  db_->RemoveRows(sites);
}

void DIPSStorage::RemoveRowsWithoutInteraction(
    const std::set<std::string>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  std::set<std::string> filtered_sites = FilterSitesWithoutInteraction(sites);

  RemoveRows(
      std::vector<std::string>(filtered_sites.begin(), filtered_sites.end()));
}

// DIPSTabHelper Function Impls ------------------------------------------------

void DIPSStorage::RecordStorage(const GURL& url,
                                base::Time time,
                                DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (!state.site_storage_times().has_value() &&
      state.user_interaction_times().has_value()) {
    // First storage, but previous interaction.
    UmaHistogramTimeToStorage(time - state.user_interaction_times()->second,
                              mode);
  }

  state.update_site_storage_time(time);
}

void DIPSStorage::RecordInteraction(const GURL& url,
                                    base::Time time,
                                    DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (!state.user_interaction_times().has_value() &&
      state.site_storage_times().has_value()) {
    // Site previously wrote to storage. Record metric for the time delay
    // between first storage and interaction.
    UmaHistogramTimeToInteraction(time - state.site_storage_times()->first,
                                  mode);
  }

  state.update_user_interaction_time(time);
}

void DIPSStorage::RecordBounce(const GURL& url,
                               base::Time time,
                               bool stateful) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DIPSState state = Read(url);
  state.update_bounce_time(time);
  if (stateful) {
    state.update_stateful_bounce_time(time);
  }
}

std::set<std::string> DIPSStorage::FilterSitesWithoutInteraction(
    std::set<std::string> sites) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  std::set<std::string> interacted_sites =
      db_->FilterSitesWithInteraction(sites);

  for (const auto& site : interacted_sites) {
    if (sites.count(site)) {
      sites.erase(site);
    }
  }

  return sites;
}

std::vector<std::string> DIPSStorage::GetSitesThatBounced(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBounced(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesThatBouncedWithState(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBouncedWithState(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesThatUsedStorage(
    base::TimeDelta grace_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatUsedStorage(grace_period);
}

std::vector<std::string> DIPSStorage::GetSitesToClear(
    absl::optional<base::TimeDelta> custom_period) const {
  std::vector<std::string> sites_to_clear;
  base::TimeDelta grace_period =
      custom_period.value_or(dips::kGracePeriod.Get());

  switch (dips::kTriggeringAction.Get()) {
    case DIPSTriggeringAction::kNone: {
      return {};
    }
    case DIPSTriggeringAction::kStorage: {
      sites_to_clear = GetSitesThatUsedStorage(grace_period);
      break;
    }
    case DIPSTriggeringAction::kBounce: {
      sites_to_clear = GetSitesThatBounced(grace_period);
      break;
    }
    case DIPSTriggeringAction::kStatefulBounce: {
      sites_to_clear = GetSitesThatBouncedWithState(grace_period);
      break;
    }
  }

  return sites_to_clear;
}

bool DIPSStorage::DidSiteHaveInteractionSince(const GURL& url,
                                              base::Time bound) {
  const DIPSState state = Read(url);
  return state.user_interaction_times().has_value() &&
         state.user_interaction_times()->second >= bound;
}

/* static */
void DIPSStorage::DeleteDatabaseFiles(base::FilePath path,
                                      base::OnceClosure on_complete) {
  // TODO (jdh): Decide how to handle the case of failing to delete db files.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(IgnoreResult(&sql::Database::Delete), std::move(path)),
      std::move(on_complete));
}

/* static */
size_t DIPSStorage::SetPrepopulateChunkSizeForTesting(size_t size) {
  return std::exchange(g_prepopulate_chunk_size, size);
}

void DIPSStorage::PrepopulateChunk(PrepopulateArgs args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_LE(args.offset, args.sites.size());

  size_t chunk_size =
      std::min(args.sites.size() - args.offset, g_prepopulate_chunk_size);
  for (size_t i = 0; i < chunk_size; i++) {
    DIPSState state = ReadSite(args.sites[args.offset + i]);
    if (state.user_interaction_times().has_value()) {
      continue;
    }

    state.update_user_interaction_time(args.time);

    if (!state.site_storage_times().has_value()) {
      // If we set a fake interaction time but no storage time, then when
      // storage does happen we'll report an incorrect
      // TimeFromInteractionToStorage metric. So set the storage time too.
      state.update_site_storage_time(args.time);
    }
  }

  // Increment chunk offset in args and resubmit task if incomplete.
  args.offset += chunk_size;
  if (args.offset < args.sites.size()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DIPSStorage::PrepopulateChunk,
                                  weak_factory_.GetWeakPtr(), std::move(args)));
  } else {
    db_->MarkAsPrepopulated();
    std::move(args.on_complete).Run();
  }
}
