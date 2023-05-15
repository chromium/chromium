// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/field_trial.h"

namespace base {

// static
void FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
    FieldTrial::ActiveGroups* active_groups) {
  return FieldTrialList::GetActiveFieldTrialGroupsInternal(
      active_groups, /*include_low_anonymity=*/true);
}

// static
bool FieldTrialListIncludingLowAnonymity::AddObserver(
    FieldTrialList::Observer* observer) {
  return FieldTrialList::AddObserverInternal(observer,
                                             /*include_low_anonymity=*/true);
}

// static
void FieldTrialListIncludingLowAnonymity::RemoveObserver(
    FieldTrialList::Observer* observer) {
  FieldTrialList::RemoveObserverInternal(observer,
                                         /*include_low_anonymity=*/true);
}

}  // namespace base
