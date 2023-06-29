// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_state.h"

#include "chrome/browser/dips/dips_storage.h"
#include "chrome/browser/dips/dips_utils.h"

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
  dirty_ |= UpdateTimestampRange(state_.site_storage_times, time);
}

void DIPSState::update_user_interaction_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.user_interaction_times, time);
}

void DIPSState::update_stateful_bounce_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.stateful_bounce_times, time);
}

void DIPSState::update_bounce_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.bounce_times, time);
}

void DIPSState::update_web_authn_assertion_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.web_authn_assertion_times, time);
}
