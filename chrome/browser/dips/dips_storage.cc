// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
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

DIPSStorage::DIPSStorage() {
  base::AssertLongCPUWorkAllowed();
}

DIPSStorage::~DIPSStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DIPSStorage::Init(const absl::optional<base::FilePath>& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<DIPSDatabase>(path);

  if (db_->Init() != sql::INIT_OK) {
    CHECK(!db_->in_memory()) << "In-memory db failed to initialize.";
    // TODO: collect a UMA metric so we know if problems are widespread.

    // Try making an in-memory database instead.
    db_ = std::make_unique<DIPSDatabase>(absl::nullopt);
    sql::InitStatus status = db_->Init();
    CHECK_EQ(status, sql::INIT_OK);
  }
}

// DIPSDatabase interaction functions ------------------------------------------

DIPSState DIPSStorage::Read(const GURL& url) {
  return ReadSite(GetSiteForDIPS(url));
}

DIPSState DIPSStorage::ReadSite(std::string site) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::optional<StateValue> state = db_->Read(site);

  if (state.has_value()) {
    return DIPSState(this, std::move(site), state.value());
  }
  return DIPSState(this, std::move(site));
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_->Write(state.site(), state.site_storage_time(),
             state.user_interaction_time());
}

// DIPSTabHelper Function Impls ------------------------------------------------

void DIPSStorage::RecordStorage(const GURL& url,
                                base::Time time,
                                DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (state.site_storage_time()) {
    // We want the time that storage was first written, so don't overwrite the
    // existing timestamp.
    return;
  }

  if (state.user_interaction_time()) {
    // First storage, but previous interaction.
    UmaHistogramTimeToStorage(time - state.user_interaction_time().value(),
                              mode);
  }

  state.set_site_storage_time(time);
}

void DIPSStorage::RecordInteraction(const GURL& url,
                                    base::Time time,
                                    DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  DIPSState state = Read(url);
  if (!state.user_interaction_time()) {
    // First interaction on site.
    if (state.site_storage_time()) {
      // Site previously wrote to storage. Record metric for the time delay
      // between storage and interaction.
      UmaHistogramTimeToInteraction(time - state.site_storage_time().value(),
                                    mode);
    }
  }

  // Unlike for storage, we want to know the time of the most recent user
  // interaction, so overwrite any existing timestamp. (If interaction happened
  // a long time ago, it may no longer be relevant.)
  state.set_user_interaction_time(time);
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
    if (state.user_interaction_time()) {
      continue;
    }

    state.set_user_interaction_time(args.time);

    if (!state.site_storage_time()) {
      // If we set a fake interaction time but no storage time, then when
      // storage does happen we'll report an incorrect
      // TimeFromInteractionToStorage metric. So set the storage time too.
      state.set_site_storage_time(args.time);
    }
  }

  // Increment chunk offset in args and resubmit task if incomplete.
  args.offset += chunk_size;
  if (args.offset < args.sites.size()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DIPSStorage::PrepopulateChunk,
                                  weak_factory_.GetWeakPtr(), std::move(args)));
  }
}
