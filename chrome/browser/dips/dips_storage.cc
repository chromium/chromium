// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/dips/dips_utils.h"
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

}  // namespace

DIPSStorage::DIPSStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DIPSStorage::~DIPSStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

DIPSState DIPSStorage::Read(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string site = GetDIPSSite(url);
  auto iter = map_.find(site);
  if (iter == map_.end()) {
    return DIPSState(this, std::move(site));
  }
  const StateValue& value = iter->second;
  DIPSState state(this, std::move(site), value);
  return state;
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StateValue& value = map_[state.site()];
  value.site_storage_time = state.site_storage_time();
  value.user_interaction_time = state.user_interaction_time();
}

void DIPSStorage::RecordStorage(const GURL& url,
                                base::Time time,
                                DIPSCookieMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
