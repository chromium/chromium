// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "components/services/app_service/public/cpp/macros.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace apps {

PromiseAppUpdate::PromiseAppUpdate(const PromiseApp* state,
                                   const PromiseApp* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK_EQ(state_->package_id, delta->package_id);
  }
}

bool PromiseAppUpdate::operator==(const PromiseAppUpdate& rhs) const {
  bool states_are_same = false;
  bool deltas_are_same = false;
  if (!this->state_ && !rhs.state_) {
    states_are_same = true;
  }
  if (this->state_ && rhs.state_) {
    states_are_same = *(this->state_) == *(rhs.state_);
  }

  if (!this->delta_ && !rhs.delta_) {
    deltas_are_same = true;
  }
  if (this->delta_ && rhs.delta_) {
    deltas_are_same = *(this->delta_) == *(rhs.delta_);
  }
  return states_are_same && deltas_are_same;
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

  SET_OPTIONAL_VALUE(name);
  SET_OPTIONAL_VALUE(progress);
  SET_ENUM_VALUE(status, PromiseStatus::kUnknown);
  SET_OPTIONAL_VALUE(should_show);
  SET_OPTIONAL_VALUE(installed_app_id);

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

std::optional<std::string> PromiseAppUpdate::Name() const {
  if (delta_ && delta_->name.has_value()) {
    return *delta_->name;
  }
  if (state_ && state_->name.has_value()) {
    return *state_->name;
  }
  return std::nullopt;
}

bool PromiseAppUpdate::NameChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(name);
}

std::optional<float> PromiseAppUpdate::Progress() const {
  if (delta_ && delta_->progress.has_value()) {
    return *delta_->progress;
  }
  if (state_ && state_->progress.has_value()) {
    return *state_->progress;
  }
  return std::nullopt;
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

std::string PromiseAppUpdate::InstalledAppId() const {
  if (delta_ && delta_->installed_app_id.has_value()) {
    return *delta_->installed_app_id;
  }
  if (state_ && state_->installed_app_id.has_value()) {
    return *state_->installed_app_id;
  }
  return "";
}

bool PromiseAppUpdate::InstalledAppIdChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(installed_app_id);
}

bool PromiseAppUpdate::ShouldShow() const {
  GET_VALUE_WITH_FALLBACK(should_show, false);
}

bool PromiseAppUpdate::ShouldShowChanged() const {
  RETURN_OPTIONAL_VALUE_CHANGED(should_show);
}

std::ostream& operator<<(std::ostream& out, const PromiseAppUpdate& update) {
  out << "Package_id: " << update.PackageId().ToString() << std::endl;
  out << "- Name Changed: " << update.NameChanged() << std::endl;
  out << "- Name: " << update.Name().value_or("N/A") << std::endl;
  out << "- Progress Changed: " << update.ProgressChanged() << std::endl;
  out << "- Progress: "
      << (update.Progress().has_value()
              ? base::NumberToString(update.Progress().value())
              : "N/A")
      << std::endl;
  out << "- Status Changed: " << update.StatusChanged() << std::endl;
  out << "- Status: " << EnumToString(update.Status()) << std::endl;
  out << "- Should Show Changed: " << update.ShouldShowChanged() << std::endl;
  out << "- Should Show: " << update.ShouldShow() << std::endl;
  out << "- Installed App ID " << update.InstalledAppId() << std::endl;
  return out;
}
}  // namespace apps
