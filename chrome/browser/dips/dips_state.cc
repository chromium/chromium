// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_state.h"

#include "chrome/browser/dips/dips_storage.h"

namespace dips {

class DIPSStorage;

DIPSState::DIPSState(DIPSStorage* storage, std::string site, bool was_loaded)
    : storage_(storage), site_(site), was_loaded_(was_loaded) {}

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

}  // namespace dips
