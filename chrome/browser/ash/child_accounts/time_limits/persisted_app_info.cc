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

absl::optional<AppActivity::ActiveTime> AppActivityFromDict(
    const base::Value& value) {
  if (!value.is_dict()) {
    VLOG(1) << "Value is not a dictionary";
    return absl::nullopt;
  }

  const std::string* active_from = value.FindStringKey(kActiveFromKey);
  if (!active_from) {
    VLOG(1) << "Invalid |active_from| entry in dictionary";
    return absl::nullopt;
  }

  const std::string* active_to = value.FindStringKey(kActiveToKey);
  if (!active_to) {
    VLOG(1) << "Invalid |active_to| entry in dictionary.";
    return absl::nullopt;
  }

  int64_t active_from_microseconds;
  int64_t active_to_microseconds;
  if (!base::StringToInt64(*active_from, &active_from_microseconds) ||
      !base::StringToInt64(*active_to, &active_to_microseconds)) {
    return absl::nullopt;
  }

  base::Time active_from_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(active_from_microseconds));
  base::Time active_to_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(active_to_microseconds));

  return AppActivity::ActiveTime(active_from_time, active_to_time);
}

base::Value AppActivityToDict(const AppActivity::ActiveTime& active_time) {
  base::Value value(base::Value::Type::DICTIONARY);

  auto serializeTime = [](base::Time time) -> std::string {
    return base::NumberToString(
        time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  };

  value.SetStringKey(kActiveFromKey, serializeTime(active_time.active_from()));
  value.SetStringKey(kActiveToKey, serializeTime(active_time.active_to()));

  return value;
}

std::vector<AppActivity::ActiveTime> AppActiveTimesFromList(
    const base::Value* list) {
  std::vector<AppActivity::ActiveTime> active_times;

  if (!list || !list->is_list()) {
    VLOG(1) << " Invalid app activity list";
    return active_times;
  }

  const base::Value::List& list_view = list->GetList();

  for (const auto& value : list_view) {
    absl::optional<AppActivity::ActiveTime> entry = AppActivityFromDict(value);
    if (!entry)
      continue;
    active_times.push_back(entry.value());
  }

  return active_times;
}

}  // namespace

// static
absl::optional<PersistedAppInfo> PersistedAppInfo::PersistedAppInfoFromDict(
    const base::Value* dict,
    bool include_app_activity_array) {
  if (!dict || !dict->is_dict()) {
    VLOG(1) << "Invalid application information.";
    return absl::nullopt;
  }

  absl::optional<AppId> app_id = policy::AppIdFromAppInfoDict(*dict);
  if (!app_id)
    return absl::nullopt;

  absl::optional<AppState> state = GetAppStateFromDict(dict);
  if (!state) {
    VLOG(1) << "Invalid application state.";
    return absl::nullopt;
  }

  const std::string* running_active_time =
      dict->FindStringKey(kRunningActiveTimeKey);
  if (!running_active_time) {
    VLOG(1) << "Invalid running active time.";
    return absl::nullopt;
  }

  int64_t running_active_time_int;
  if (!base::StringToInt64(*running_active_time, &running_active_time_int)) {
    VLOG(1) << "Invalid running active time.";
    return absl::nullopt;
  }

  std::vector<AppActivity::ActiveTime> active_times;
  if (include_app_activity_array) {
    const base::Value* list = dict->FindListKey(kActiveTimesKey);
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
    absl::optional<PersistedAppInfo> info =
        PersistedAppInfoFromDict(&per_app_info, include_app_activity_array);
    if (!info.has_value())
      continue;

    apps_info.push_back(std::move(info.value()));
  }

  return apps_info;
}

// static
absl::optional<AppState> PersistedAppInfo::GetAppStateFromDict(
    const base::Value* value) {
  if (!value || !value->is_dict())
    return absl::nullopt;

  absl::optional<int> state = value->FindIntKey(kAppStateKey);
  if (!state.has_value())
    return absl::nullopt;

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
    base::Value* dict,
    bool replace_activity) const {
  DCHECK(!!dict && dict->is_dict());

  dict->SetKey(kAppInfoKey, policy::AppIdToDict(app_id_));
  dict->SetIntKey(kAppStateKey, static_cast<int>(app_state()));
  dict->SetStringKey(
      kRunningActiveTimeKey,
      base::NumberToString(active_running_time().InMicroseconds()));

  if (replace_activity) {
    base::Value active_times_value(base::Value::Type::LIST);
    for (const auto& entry : active_times_) {
      active_times_value.Append(AppActivityToDict(entry));
    }

    dict->SetPath(kActiveTimesKey, std::move(active_times_value));
    return;
  }

  base::Value* value = dict->FindListKey(kActiveTimesKey);
  if (!value || !value->is_list()) {
    value =
        dict->SetPath(kActiveTimesKey, base::Value(base::Value::Type::LIST));
  }

  if (active_times_.size() == 0)
    return;

  // start index into |active_times_|
  size_t start_index = 0;

  // If the last entry in |value| can be merged with the first entry in
  // |active_times_| merge them.
  base::Value::List& list_view = value->GetList();
  if (list_view.size() > 0) {
    base::Value& mergeable_entry = list_view[list_view.size() - 1];
    absl::optional<AppActivity::ActiveTime> active_time =
        AppActivityFromDict(mergeable_entry);
    DCHECK(active_time.has_value());

    absl::optional<AppActivity::ActiveTime> merged =
        AppActivity::ActiveTime::Merge(active_time.value(), active_times_[0]);
    if (merged.has_value()) {
      mergeable_entry = AppActivityToDict(merged.value());
      start_index = 1;
    }
  }

  for (size_t i = start_index; i < active_times_.size(); i++) {
    value->Append(AppActivityToDict(active_times_[i]));
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
