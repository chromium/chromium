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
      first_site_storage_time_(state.first_site_storage_time),
      last_site_storage_time_(state.last_site_storage_time),
      first_user_interaction_time_(state.first_user_interaction_time),
      last_user_interaction_time_(state.last_user_interaction_time) {}

DIPSState::DIPSState(DIPSState&&) = default;

DIPSState::~DIPSState() {
  if (dirty_) {
    storage_->Write(*this);
  }
}

void DIPSState::update_site_storage_time(base::Time time) {
  if (time == first_site_storage_time_ || time == last_site_storage_time_) {
    return;
  }

  if (!first_site_storage_time_.has_value())
    first_site_storage_time_ = time;

  DCHECK_GE(time, first_site_storage_time_.value());
  last_site_storage_time_ = time;
  dirty_ = true;
}

void DIPSState::update_user_interaction_time(base::Time time) {
  if (time == first_user_interaction_time_ ||
      time == last_user_interaction_time_) {
    return;
  }

  if (!first_user_interaction_time_.has_value())
    first_user_interaction_time_ = time;

  DCHECK_GE(time, first_user_interaction_time_.value());
  last_user_interaction_time_ = time;
  dirty_ = true;
}
