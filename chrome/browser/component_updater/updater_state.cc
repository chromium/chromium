
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#include <map>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/enterprise_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {

UpdaterState::State::State() = default;
UpdaterState::State::State(const UpdaterState::State&) = default;
UpdaterState::State& UpdaterState::State::operator=(
    const UpdaterState::State&) = default;
UpdaterState::State::~State() = default;

std::unique_ptr<UpdaterState::StateReader> UpdaterState::StateReader::Create() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_MAC)
  return std::make_unique<UpdaterState::StateReaderKeystone>();
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<UpdaterState::StateReaderOmaha>();
#else
  return nullptr;
#endif  // IS_MAC
#else
  return nullptr;
#endif  // GOOGLE_CHROME_BRANDING
}

UpdaterState::State UpdaterState::StateReader::Read(bool is_machine) const {
  State state;
  state.updater_name = GetUpdaterName();
  state.updater_version = GetUpdaterVersion(is_machine);
  state.last_autoupdate_started = GetUpdaterLastStartedAU(is_machine);
  state.last_checked = GetUpdaterLastChecked(is_machine);
  state.is_autoupdate_check_enabled = IsAutoupdateCheckEnabled();
  state.update_policy = [this]() {
    const int update_policy = GetUpdatePolicy();
    DCHECK((update_policy >= 0 && update_policy <= 3) || update_policy == -1);
    return update_policy;
  }();
  return state;
}

UpdaterState::UpdaterState(bool is_machine)
    : is_machine_(is_machine), state_(ReadState(is_machine)) {}

UpdaterState::~UpdaterState() = default;

UpdaterState::Attributes UpdaterState::GetState(bool is_machine) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return UpdaterState(is_machine).Serialize();
#else
  return {};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

absl::optional<UpdaterState::State> UpdaterState::ReadState(bool is_machine) {
  std::unique_ptr<UpdaterState::StateReader> state_reader =
      UpdaterState::StateReader::Create();
  if (!state_reader)
    return absl::nullopt;
  return state_reader->Read(is_machine);
}

UpdaterState::Attributes UpdaterState::Serialize() const {
  Attributes attributes;

  attributes["ismachine"] = is_machine_ ? "1" : "0";

  if (state_) {
    attributes["name"] = state_->updater_name;

    if (state_->updater_version.IsValid())
      attributes["version"] = state_->updater_version.GetString();

    const base::Time now = base::Time::NowFromSystemTime();
    if (!state_->last_autoupdate_started.is_null())
      attributes["laststarted"] =
          NormalizeTimeDelta(now - state_->last_autoupdate_started);
    if (!state_->last_checked.is_null())
      attributes["lastchecked"] =
          NormalizeTimeDelta(now - state_->last_checked);

    attributes["autoupdatecheckenabled"] =
        state_->is_autoupdate_check_enabled ? "1" : "0";

    attributes["updatepolicy"] = base::NumberToString(state_->update_policy);
  }

  return attributes;
}

std::string UpdaterState::NormalizeTimeDelta(const base::TimeDelta& delta) {
  const base::TimeDelta two_weeks = base::Days(14);
  const base::TimeDelta two_months = base::Days(56);

  std::string val;  // Contains the value to return in hours.
  if (delta <= two_weeks) {
    val = "0";
  } else if (two_weeks < delta && delta <= two_months) {
    val = "336";  // 2 weeks in hours.
  } else {
    val = "1344";  // 2*28 days in hours.
  }

  DCHECK(!val.empty());
  return val;
}

}  // namespace component_updater
