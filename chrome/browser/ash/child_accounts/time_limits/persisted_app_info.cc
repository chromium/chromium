// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/persisted_app_info.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"

namespace ash {
namespace app_time {

namespace {

// The following keys are stored in user pref. Modification will result in
// errors when parsing old data.
constexpr char kAppInfoKey[] = "app_info";
constexpr char kAppStateKey[] = "app_state";
constexpr char kRunningActiveTimeKey[] = "running_active_time";
constexpr char kActiveTimesKey[] = "active_times";
constexpr char kActiveFromKey[] = "active_from";
constexpr char kActiveToKey[] = "active_to";

std::optional<AppActivity::ActiveTime> AppActivityFromDict(
    const base::Value::Dict& dict) {
  const std::string* active_from = dict.FindString(kActiveFromKey);
  if (!active_from) {
    VLOG(1) << "Invalid |active_from| entry in dictionary";
    return std::nullopt;
  }

  const std::string* active_to = dict.FindString(kActiveToKey);
  if (!active_to) {
    VLOG(1) << "Invalid |active_to| entry in dictionary.";
    return std::nullopt;
  }

  int64_t active_from_microseconds;
  int64_t active_to_microseconds;
  if (!base::StringToInt64(*active_from, &active_from_microseconds) ||
      !base::StringToInt64(*active_to, &active_to_microseconds)) {
    return std::nullopt;
  }

  base::Time active_from_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(active_from_microseconds));
  base::Time active_to_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(active_to_microseconds));

  return AppActivity::ActiveTime(active_from_time, active_to_time);
}

base::Value::Dict AppActivityToDict(
    const AppActivity::ActiveTime& active_time) {
  base::Value::Dict dict;

  auto serializeTime = [](base::Time time) -> std::string {
    return base::NumberToString(
        time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  };

  dict.Set(kActiveFromKey, serializeTime(active_time.active_from()));
  dict.Set(kActiveToKey, serializeTime(active_time.active_to()));

  return dict;
}

std::vector<AppActivity::ActiveTime> AppActiveTimesFromList(
    const base::Value::List* list) {
  std::vector<AppActivity::ActiveTime> active_times;

  if (!list) {
    VLOG(1) << " Invalid app activity list";
    return active_times;
  }

  for (const auto& value : *list) {
    auto* dict = value.GetIfDict();
    if (!dict) {
      VLOG(1) << "Value is not a dictionary";
      continue;
    }
    std::optional<AppActivity::ActiveTime> entry = AppActivityFromDict(*dict);
    if (!entry)
      continue;
    active_times.push_back(entry.value());
  }

  return active_times;
}

}  // namespace

// static
std::optional<PersistedAppInfo> PersistedAppInfo::PersistedAppInfoFromDict(
    const base::Value::Dict* dict,
    bool include_app_activity_array) {
  if (!dict) {
    VLOG(1) << "Invalid application information.";
    return std::nullopt;
  }

  std::optional<AppId> app_id = policy::AppIdFromAppInfoDict(dict);
  if (!app_id)
    return std::nullopt;

  std::optional<AppState> state = GetAppStateFromDict(dict);
  if (!state) {
    VLOG(1) << "Invalid application state.";
    return std::nullopt;
  }

  const std::string* running_active_time =
      dict->FindString(kRunningActiveTimeKey);
  if (!running_active_time) {
    VLOG(1) << "Invalid running active time.";
    return std::nullopt;
  }

  int64_t running_active_time_int;
  if (!base::StringToInt64(*running_active_time, &running_active_time_int)) {
    VLOG(1) << "Invalid running active time.";
    return std::nullopt;
  }

  std::vector<AppActivity::ActiveTime> active_times;
  if (include_app_activity_array) {
    const base::Value::List* list = dict->FindList(kActiveTimesKey);
    active_times = AppActiveTimesFromList(list);
  }

  return PersistedAppInfo(app_id.value(), state.value(),
                          base::Microseconds(running_active_time_int),
                          std::move(active_times));
}

// static
std::vector<PersistedAppInfo> PersistedAppInfo::PersistedAppInfosFromList(
    const base::Value::List& list,
    bool include_app_activity_array) {
  std::vector<PersistedAppInfo> apps_info;

  for (const auto& per_app_info : list) {
    std::optional<PersistedAppInfo> info = PersistedAppInfoFromDict(
        per_app_info.GetIfDict(), include_app_activity_array);
    if (!info.has_value())
      continue;

    apps_info.push_back(std::move(info.value()));
  }

  return apps_info;
}

// static
std::optional<AppState> PersistedAppInfo::GetAppStateFromDict(
    const base::Value::Dict* value) {
  if (!value) {
    return std::nullopt;
  }

  std::optional<int> state = value->FindInt(kAppStateKey);
  if (!state.has_value())
    return std::nullopt;

  return static_cast<AppState>(state.value());
}

PersistedAppInfo::PersistedAppInfo(
    const AppId& app_id,
    AppState state,
    base::TimeDelta active_running_time,
    std::vector<AppActivity::ActiveTime> active_times)
    : app_id_(app_id),
      app_state_(state),
      active_running_time_(active_running_time),
      active_times_(active_times) {}

PersistedAppInfo::PersistedAppInfo(const PersistedAppInfo& info)
    : app_id_(info.app_id_),
      app_state_(info.app_state_),
      active_running_time_(info.active_running_time_),
      active_times_(info.active_times_) {}

PersistedAppInfo::PersistedAppInfo(PersistedAppInfo&& info)
    : app_id_(info.app_id_),
      app_state_(info.app_state_),
      active_running_time_(info.active_running_time_),
      active_times_(std::move(info.active_times_)) {}

PersistedAppInfo& PersistedAppInfo::operator=(const PersistedAppInfo& info) {
  app_id_ = info.app_id_;
  app_state_ = info.app_state_;
  active_running_time_ = info.active_running_time_;
  active_times_ = info.active_times_;
  return *this;
}

PersistedAppInfo& PersistedAppInfo::operator=(PersistedAppInfo&& info) {
  app_id_ = info.app_id_;
  app_state_ = info.app_state_;
  active_running_time_ = info.active_running_time_;
  active_times_ = std::move(info.active_times_);
  return *this;
}

PersistedAppInfo::~PersistedAppInfo() = default;

void PersistedAppInfo::UpdateAppActivityPreference(
    base::Value::Dict& dict,
    bool replace_activity) const {
  dict.Set(kAppInfoKey, policy::AppIdToDict(app_id_));
  dict.Set(kAppStateKey, static_cast<int>(app_state()));
  dict.Set(kRunningActiveTimeKey,
           base::NumberToString(active_running_time().InMicroseconds()));

  if (replace_activity) {
    base::Value::List active_times_list;
    for (const auto& entry : active_times_) {
      active_times_list.Append(AppActivityToDict(entry));
    }

    dict.SetByDottedPath(kActiveTimesKey, std::move(active_times_list));
    return;
  }

  base::Value::List* list = dict.FindList(kActiveTimesKey);
  if (!list) {
    list =
        &dict.SetByDottedPath(kActiveTimesKey, base::Value::List())->GetList();
  }

  if (active_times_.size() == 0)
    return;

  // start index into |active_times_|
  size_t start_index = 0;

  // If the last entry in |list| can be merged with the first entry in
  // |active_times_| merge them.
  base::Value::List& list_view = *list;
  if (list_view.size() > 0) {
    base::Value& mergeable_entry = list_view[list_view.size() - 1];
    CHECK(mergeable_entry.is_dict());
    std::optional<AppActivity::ActiveTime> active_time =
        AppActivityFromDict(mergeable_entry.GetDict());
    CHECK(active_time.has_value());

    std::optional<AppActivity::ActiveTime> merged =
        AppActivity::ActiveTime::Merge(active_time.value(), active_times_[0]);
    if (merged.has_value()) {
      mergeable_entry = base::Value(AppActivityToDict(merged.value()));
      start_index = 1;
    }
  }

  for (size_t i = start_index; i < active_times_.size(); i++) {
    list->Append(AppActivityToDict(active_times_[i]));
  }
}

void PersistedAppInfo::RemoveActiveTimeEarlierThan(base::Time timestamp) {
  std::vector<AppActivity::ActiveTime> active_times = std::move(active_times_);

  // |active_times_| is empty now. Populate it with active times later than
  // |timestamp|.
  for (auto& entry : active_times) {
    if (entry.IsLaterThan(timestamp)) {
      active_times_.push_back(entry);
    } else if (entry.Contains(timestamp)) {
      entry.set_active_from(timestamp);
      active_times_.push_back(entry);
    }
  }
}

bool PersistedAppInfo::ShouldRestoreApp() const {
  bool is_installed = app_state() != AppState::kUninstalled;
  bool has_active_running_time = active_running_time() > base::Seconds(0);
  return is_installed || has_active_running_time;
}

bool PersistedAppInfo::ShouldRemoveApp() const {
  return !ShouldRestoreApp() && active_times().empty();
}

}  // namespace app_time
}  // namespace ash
