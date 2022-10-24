// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_state.h"

#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"

class DIPSStorage;

DIPSState::DIPSState(DIPSStorage* storage, std::string site)
    : storage_(storage), site_(std::move(site)), was_loaded_(false) {}

DIPSState::DIPSState(DIPSStorage* storage,
                     std::string site,
                     const StateValue& state)
    : storage_(storage),
      site_(std::move(site)),
      was_loaded_(true),
      state_(state) {}

DIPSState::DIPSState(DIPSState&&) = default;

DIPSState::~DIPSState() {
  if (dirty_) {
    storage_->Write(*this);
  }
}

void DIPSState::update_site_storage_time(base::Time time) {
  if (time == site_storage_times().first || time == site_storage_times().last)
    return;

  if (!site_storage_times().first.has_value())
    state_.site_storage_times.first = time;

  DCHECK_GE(time, site_storage_times().first.value());
  state_.site_storage_times.last = time;
  dirty_ = true;
}

void DIPSState::update_user_interaction_time(base::Time time) {
  if (time == user_interaction_times().first ||
      time == user_interaction_times().last) {
    return;
  }

  if (!user_interaction_times().first.has_value())
    state_.user_interaction_times.first = time;

  DCHECK_GE(time, user_interaction_times().first.value());
  state_.user_interaction_times.last = time;
  dirty_ = true;
}

void DIPSState::update_stateful_bounce_time(base::Time time) {
  if (time == stateful_bounce_times().first ||
      time == stateful_bounce_times().last)
    return;

  if (!stateful_bounce_times().first.has_value())
    state_.stateful_bounce_times.first = time;

  DCHECK_GE(time, stateful_bounce_times().first.value());
  state_.stateful_bounce_times.last = time;
  dirty_ = true;
}

void DIPSState::update_stateless_bounce_time(base::Time time) {
  if (time == stateless_bounce_times().first ||
      time == stateless_bounce_times().last) {
    return;
  }

  if (!stateless_bounce_times().first.has_value())
    state_.stateless_bounce_times.first = time;

  DCHECK_GE(time, stateless_bounce_times().first.value());
  state_.stateless_bounce_times.last = time;
  dirty_ = true;
}
