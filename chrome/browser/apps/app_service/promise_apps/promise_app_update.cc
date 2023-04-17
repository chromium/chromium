// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"

#include "base/logging.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/macros.h"

namespace apps {

PromiseAppUpdate::PromiseAppUpdate(const PromiseApp* state,
                                   const PromiseApp* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK_EQ(state_->package_id, delta->package_id);
  }
}

void PromiseAppUpdate::Merge(PromiseApp* state, const PromiseApp* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if (delta->package_id != state->package_id) {
    LOG(ERROR) << "inconsistent package_id: " << delta->package_id.ToString()
               << " vs " << state->package_id.ToString();
    return;
  }

  SET_OPTIONAL_VALUE(progress);
  SET_ENUM_VALUE(status, PromiseStatus::kUnknown);

  // When adding new fields to the PromiseApp struct, this function should also
  // be updated.
}

const PackageId& PromiseAppUpdate::PackageId() const {
  DCHECK(state_ || delta_);
  if (delta_) {
    return delta_->package_id;
  } else {
    return state_->package_id;
  }
}

absl::optional<std::string> PromiseAppUpdate::Name() const {
  if (delta_ && delta_->name.has_value()) {
    return *delta_->name;
  }
  if (state_ && state_->name.has_value()) {
    return *state_->name;
  }
  return absl::nullopt;
}

bool PromiseAppUpdate::NameChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(name);
}

absl::optional<float> PromiseAppUpdate::Progress() const {
  if (delta_ && delta_->progress.has_value()) {
    return *delta_->progress;
  }
  if (state_ && state_->progress.has_value()) {
    return *state_->progress;
  }
  return absl::nullopt;
}

bool PromiseAppUpdate::ProgressChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(progress);
}

PromiseStatus PromiseAppUpdate::Status() const {
  GET_VALUE_WITH_DEFAULT_VALUE(status, PromiseStatus::kUnknown);
}

bool PromiseAppUpdate::StatusChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(status, PromiseStatus::kUnknown);
}

bool PromiseAppUpdate::ShouldShow() const {
  GET_VALUE_WITH_DEFAULT_VALUE(should_show, false);
}

bool PromiseAppUpdate::ShouldShowChanged() const {
  IS_VALUE_CHANGED_WITH_DEFAULT_VALUE(should_show, false);
}

}  // namespace apps
