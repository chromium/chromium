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
      site_storage_time_(state.site_storage_time),
      user_interaction_time_(state.user_interaction_time) {}

DIPSState::DIPSState(DIPSState&&) = default;

DIPSState::~DIPSState() {
  if (dirty_) {
    storage_->Write(*this);
  }
}

void DIPSState::set_site_storage_time(absl::optional<base::Time> time) {
  if (time == site_storage_time_) {
    return;
  }

  site_storage_time_ = time;
  dirty_ = true;
}

void DIPSState::set_user_interaction_time(absl::optional<base::Time> time) {
  if (time == user_interaction_time_) {
    return;
  }

  user_interaction_time_ = time;
  dirty_ = true;
}
