// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client_errors.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#endif

namespace component_updater {
namespace {

// These literals must not be changed since they affect the forward and
// backward compatibility with //chrome/updater.
constexpr char kUpdaterPrefsActiveVersion[] = "active_version";
constexpr char kUpdaterPrefsLastChecked[] = "last_checked";
constexpr char kUpdaterPrefsLastStarted[] = "last_started";
}  // namespace

UpdaterState::State::State() = default;
UpdaterState::State::State(const UpdaterState::State&) = default;
UpdaterState::State& UpdaterState::State::operator=(
    const UpdaterState::State&) = default;
UpdaterState::State::~State() = default;

std::unique_ptr<UpdaterState::StateReader> UpdaterState::StateReader::Create(
    bool is_machine) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (std::unique_ptr<StateReader> state_reader_chromium_updater =
          [is_machine]() -> std::unique_ptr<StateReader> {
        // Create a `StateReaderChromiumUpdater` instance only if a prefs.json
        // file for the updater can be found and parsed successfully.
        const updater::UpdaterScope updater_scope =
            is_machine ? updater::UpdaterScope::kSystem
                       : updater::UpdaterScope::kUser;
        const std::optional<base::FilePath> global_prefs_dir =
#if BUILDFLAG(IS_WIN)
            // Google Chrome ships with an x86 updater.
            updater::GetInstallDirectoryX86(updater_scope);
#else
            updater::GetInstallDirectory(updater_scope);
#endif  //  IS_WIN
        if (!global_prefs_dir)
          return nullptr;
        std::string contents;
        constexpr char kUpdaterPrefsFilename[] = "prefs.json";
        constexpr int kMaxPrefsFileSize = 0x20000;  // 128KiB.
        if (!base::ReadFileToStringWithMaxSize(
                global_prefs_dir->AppendASCII(kUpdaterPrefsFilename), &contents,
                kMaxPrefsFileSize)) {
          return nullptr;
        }
        std::optional<base::Value::Dict> parsed_json =
            base::JSONReader::ReadDict(contents);
        return parsed_json ? std::make_unique<StateReaderChromiumUpdater>(
                                 std::move(*parsed_json))
                           : nullptr;
      }()) {
    return state_reader_chromium_updater;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

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

UpdaterState::StateReaderChromiumUpdater::StateReaderChromiumUpdater(
    base::Value::Dict parsed_json)
    : parsed_json_(std::move(parsed_json)) {}

base::Time UpdaterState::StateReaderChromiumUpdater::FindTimeKey(
    std::string_view key) const {
  return base::ValueToTime(parsed_json_.Find(key)).value_or(base::Time());
}

std::string UpdaterState::StateReaderChromiumUpdater::GetUpdaterName() const {
  return "ChromiumUpdater";
}

base::Version UpdaterState::StateReaderChromiumUpdater::GetUpdaterVersion(
    bool /*is_machine*/) const {
  const std::string* val = parsed_json_.FindString(kUpdaterPrefsActiveVersion);
  return val ? base::Version(*val) : base::Version();
}

bool UpdaterState::StateReaderChromiumUpdater::IsAutoupdateCheckEnabled()
    const {
  return UpdaterState::IsAutoupdateCheckEnabled();
}

base::Time UpdaterState::StateReaderChromiumUpdater::GetUpdaterLastStartedAU(
    bool /*is_machine*/) const {
  return FindTimeKey(kUpdaterPrefsLastStarted);
}

base::Time UpdaterState::StateReaderChromiumUpdater::GetUpdaterLastChecked(
    bool /*is_machine*/) const {
  return FindTimeKey(kUpdaterPrefsLastChecked);
}

int UpdaterState::StateReaderChromiumUpdater::GetUpdatePolicy() const {
  return UpdaterState::GetUpdatePolicy();
}

update_client::CategorizedError
UpdaterState::StateReaderChromiumUpdater::GetLastUpdateCheckError() const {
  return {
      .category_ = static_cast<update_client::ErrorCategory>(
          parsed_json_
              .FindInt(update_client::kLastUpdateCheckErrorCategoryPreference)
              .value_or(0)),
      .code_ =
          parsed_json_.FindInt(update_client::kLastUpdateCheckErrorPreference)
              .value_or(0),
      .extra_ =
          parsed_json_
              .FindInt(update_client::kLastUpdateCheckErrorExtraCode1Preference)
              .value_or(0)};
}

UpdaterState::State UpdaterState::StateReader::Read(bool is_machine) const {
  State state;
  state.updater_name = GetUpdaterName();
  state.updater_version = GetUpdaterVersion(is_machine);
  state.last_autoupdate_started = GetUpdaterLastStartedAU(is_machine);
  state.last_checked = GetUpdaterLastChecked(is_machine);
  state.is_autoupdate_check_enabled = IsAutoupdateCheckEnabled();
  state.update_policy = [this] {
    const int update_policy = GetUpdatePolicy();
    CHECK((update_policy >= 0 && update_policy <= 3) || update_policy == -1);
    return update_policy;
  }();
  state.last_update_check_error = GetLastUpdateCheckError();
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

std::optional<UpdaterState::State> UpdaterState::ReadState(bool is_machine) {
  std::unique_ptr<UpdaterState::StateReader> state_reader =
      UpdaterState::StateReader::Create(is_machine);
  if (!state_reader) {
    return std::nullopt;
  }
  return state_reader->Read(is_machine);
}

UpdaterState::Attributes UpdaterState::Serialize() const {
  Attributes attributes;

  attributes["ismachine"] = is_machine_ ? "1" : "0";

  if (state_) {
    attributes["name"] = state_->updater_name;

    if (state_->updater_version.IsValid()) {
      attributes["version"] = state_->updater_version.GetString();
    }

    const base::Time now = base::Time::NowFromSystemTime();
    if (!state_->last_autoupdate_started.is_null()) {
      attributes["laststarted"] =
          NormalizeTimeDelta(now - state_->last_autoupdate_started);
    }
    if (!state_->last_checked.is_null()) {
      attributes["lastchecked"] =
          NormalizeTimeDelta(now - state_->last_checked);
    }

    attributes["autoupdatecheckenabled"] =
        state_->is_autoupdate_check_enabled ? "1" : "0";

    attributes["updatepolicy"] = base::NumberToString(state_->update_policy);
    attributes["lastupdatecheckerrorcode"] =
        state_->last_update_check_error.code_;
    attributes["lastupdatecheckerrorcat"] =
        static_cast<int>(state_->last_update_check_error.category_);
    attributes["lastupdatecheckextracode1"] =
        state_->last_update_check_error.extra_;
  }

  return attributes;
}

std::string UpdaterState::NormalizeTimeDelta(base::TimeDelta delta) {
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

  CHECK(!val.empty());
  return val;
}

}  // namespace component_updater
