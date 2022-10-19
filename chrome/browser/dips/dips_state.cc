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
  if (time == first_site_storage_time() || time == last_site_storage_time())
    return;

  if (!first_site_storage_time().has_value())
    state_.site_storage_times.first = time;

  DCHECK_GE(time, first_site_storage_time().value());
  state_.site_storage_times.last = time;
  dirty_ = true;
}

void DIPSState::update_user_interaction_time(base::Time time) {
  if (time == first_user_interaction_time() ||
      time == last_user_interaction_time()) {
    return;
  }

  if (!first_user_interaction_time().has_value())
    state_.user_interaction_times.first = time;

  DCHECK_GE(time, first_user_interaction_time().value());
  state_.user_interaction_times.last = time;
  dirty_ = true;
}
