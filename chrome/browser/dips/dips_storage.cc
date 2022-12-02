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
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/dips/dips_utils.h"
#include "sql/init_status.h"
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
                                              std::vector<std::string> sites)
    : time(time), offset(offset), sites(std::move(sites)) {}

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
    DCHECK(state->site_storage_times.first.has_value() ||
           state->site_storage_times.last.has_value() ||
           state->user_interaction_times.first.has_value() ||
           state->user_interaction_times.last.has_value() ||
           state->stateful_bounce_times.first.has_value() ||
           state->stateful_bounce_times.last.has_value() ||
           state->stateless_bounce_times.first.has_value() ||
           state->stateless_bounce_times.last.has_value());

    return DIPSState(this, std::move(site), state.value());
  }
  return DIPSState(this, std::move(site));
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  db_->Write(state.site(), state.site_storage_times(),
             state.user_interaction_times(), state.stateful_bounce_times(),
             state.stateless_bounce_times());
}

void DIPSStorage::RemoveEvents(base::Time delete_begin,
                               base::Time delete_end,
                               const UrlPredicate& predicate,
                               const DIPSEventRemovalType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  DCHECK(delete_end.is_null() || delete_begin <= delete_end);

  if (delete_end.is_null())
    delete_end = base::Time::Max();

  // Currently, only time-based deletions are supported.
  if (!predicate.is_null())
    return;

  db_->RemoveEventsByTime(delete_begin, delete_end, type);
}

// DIPSTabHelper Function Impls ------------------------------------------------

void DIPSStorage::RecordStorage(const GURL& url,
                                base::Time time,
                                DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (!state.site_storage_times().first.has_value() &&
      state.user_interaction_times().last.has_value()) {
    // First storage, but previous interaction.
    UmaHistogramTimeToStorage(
        time - state.user_interaction_times().last.value(), mode);
  }

  state.update_site_storage_time(time);
}

void DIPSStorage::RecordInteraction(const GURL& url,
                                    base::Time time,
                                    DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (!state.user_interaction_times().first.has_value() &&
      state.site_storage_times().first.has_value()) {
    // Site previously wrote to storage. Record metric for the time delay
    // between first storage and interaction.
    UmaHistogramTimeToInteraction(
        time - state.site_storage_times().first.value(), mode);
  }

  state.update_user_interaction_time(time);
}

void DIPSStorage::RecordStatefulBounce(const GURL& url, base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  Read(url).update_stateful_bounce_time(time);
}

void DIPSStorage::RecordStatelessBounce(const GURL& url, base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  Read(url).update_stateless_bounce_time(time);
}

std::vector<std::string> DIPSStorage::GetSitesThatBounced(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBounced(range_start, last_interaction);
}

std::vector<std::string> DIPSStorage::GetSitesThatBouncedWithState(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatBouncedWithState(range_start, last_interaction);
}

std::vector<std::string> DIPSStorage::GetSitesThatUsedStorage(
    base::Time range_start,
    base::Time last_interaction) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);
  return db_->GetSitesThatUsedStorage(range_start, last_interaction);
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
    if (state.user_interaction_times().first) {
      continue;
    }

    state.update_user_interaction_time(args.time);

    if (!state.site_storage_times().first) {
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
  }
}
