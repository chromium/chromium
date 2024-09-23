// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"

namespace ash {
namespace app_time {

namespace {

std::string AppTypeToString(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kUnknown:
      return "Unknown";
    case apps::AppType::kArc:
      return "Arc";
    case apps::AppType::kWeb:
      return "Web";
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      return "Extension";
    case apps::AppType::kBuiltIn:
      return "Built in";
    case apps::AppType::kCrostini:
      return "Crostini";
    case apps::AppType::kPluginVm:
      return "Plugin VM";
    case apps::AppType::kStandaloneBrowser:
      return "LaCrOS";
    case apps::AppType::kRemote:
      return "Remote";
    case apps::AppType::kBorealis:
      return "Borealis";
    case apps::AppType::kBruschetta:
      return "Bruschetta";
    case apps::AppType::kSystemWeb:
      return "SystemWeb";
  }
  NOTREACHED_IN_MIGRATION();
}

// static
bool CanMerge(const AppActivity::ActiveTime& t1,
              const AppActivity::ActiveTime& t2) {
  if (t1.active_from() <= t2.active_from() &&
      t1.active_to() >=
          t2.active_from() -
              AppActivity::ActiveTime::kActiveTimeMergePrecision) {
    return true;
  }

  if (t2.active_from() <= t1.active_from() &&
      t2.active_to() >=
          t1.active_from() -
              AppActivity::ActiveTime::kActiveTimeMergePrecision) {
    return true;
  }
  return false;
}

}  // namespace

AppId::AppId(apps::AppType app_type, const std::string& app_id)
    : app_type_(app_type), app_id_(app_id) {
  DCHECK(!app_id.empty());
}

AppId::AppId(const AppId&) = default;

AppId& AppId::operator=(const AppId&) = default;

AppId::AppId(AppId&&) = default;

AppId& AppId::operator=(AppId&&) = default;

AppId::~AppId() = default;

bool AppId::operator==(const AppId& rhs) const {
  return app_type_ == rhs.app_type() && app_id_ == rhs.app_id();
}

bool AppId::operator!=(const AppId& rhs) const {
  return !(*this == rhs);
}

bool AppId::operator<(const AppId& rhs) const {
  return app_id_ < rhs.app_id();
}

std::ostream& operator<<(std::ostream& out, const AppId& id) {
  return out << " [" << AppTypeToString(id.app_type()) << " : " << id.app_id()
             << "]";
}

PauseAppInfo::PauseAppInfo(const AppId& app,
                           base::TimeDelta limit,
                           bool show_dialog)
    : app_id(app), daily_limit(limit), show_pause_dialog(show_dialog) {}

AppLimit::AppLimit(AppRestriction restriction,
                   std::optional<base::TimeDelta> daily_limit,
                   base::Time last_updated)
    : restriction_(restriction),
      daily_limit_(daily_limit),
      last_updated_(last_updated) {
  DCHECK_EQ(restriction_ == AppRestriction::kBlocked,
            daily_limit_ == std::nullopt);
  DCHECK(daily_limit_ == std::nullopt || daily_limit >= base::Hours(0));
  DCHECK(daily_limit_ == std::nullopt || daily_limit <= base::Hours(24));
}

AppLimit::AppLimit(const AppLimit&) = default;

AppLimit& AppLimit::operator=(const AppLimit&) = default;

AppLimit::AppLimit(AppLimit&&) = default;

AppLimit& AppLimit::operator=(AppLimit&&) = default;

AppLimit::~AppLimit() = default;

// static
std::optional<AppActivity::ActiveTime> AppActivity::ActiveTime::Merge(
    const ActiveTime& t1,
    const ActiveTime& t2) {
  if (!CanMerge(t1, t2))
    return std::nullopt;

  base::Time active_from = std::min(t1.active_from(), t2.active_from());
  base::Time active_to = std::max(t1.active_to(), t2.active_to());
  return AppActivity::ActiveTime(active_from, active_to);
}

// static
const base::TimeDelta AppActivity::ActiveTime::kActiveTimeMergePrecision =
    base::Seconds(1);

AppActivity::ActiveTime::ActiveTime(base::Time start, base::Time end)
    : active_from_(start), active_to_(end) {
  DCHECK_GT(active_to_, active_from_);
}

AppActivity::ActiveTime::ActiveTime(const AppActivity::ActiveTime& rhs) =
    default;

AppActivity::ActiveTime& AppActivity::ActiveTime::operator=(
    const AppActivity::ActiveTime& rhs) = default;

bool AppActivity::ActiveTime::operator==(const ActiveTime& rhs) const {
  return active_from_ == rhs.active_from() && active_to_ == rhs.active_to();
}

bool AppActivity::ActiveTime::operator!=(const ActiveTime& rhs) const {
  return !(*this == rhs);
}

bool AppActivity::ActiveTime::Contains(base::Time timestamp) const {
  return active_from_ < timestamp && active_to_ > timestamp;
}

bool AppActivity::ActiveTime::IsEarlierThan(base::Time timestamp) const {
  return active_to_ <= timestamp;
}

bool AppActivity::ActiveTime::IsLaterThan(base::Time timestamp) const {
  return active_from_ >= timestamp;
}

void AppActivity::ActiveTime::set_active_from(base::Time active_from) {
  DCHECK_GT(active_to_, active_from);
  active_from_ = active_from;
}

void AppActivity::ActiveTime::set_active_to(base::Time active_to) {
  DCHECK_GT(active_to, active_from_);
  active_to_ = active_to;
}

AppActivity::AppActivity(AppState app_state)
    : app_state_(app_state),
      running_active_time_(base::Seconds(0)),
      last_updated_time_ticks_(base::TimeTicks::Now()) {}
AppActivity::AppActivity(AppState app_state,
                         base::TimeDelta running_active_time)
    : app_state_(app_state),
      running_active_time_(running_active_time),
      last_updated_time_ticks_(base::TimeTicks::Now()) {}
AppActivity::AppActivity(const AppActivity&) = default;
AppActivity& AppActivity::operator=(const AppActivity&) = default;
AppActivity::AppActivity(AppActivity&&) = default;
AppActivity& AppActivity::operator=(AppActivity&&) = default;
AppActivity::~AppActivity() = default;

void AppActivity::SetAppState(AppState app_state) {
  app_state_ = app_state;
  CaptureOngoingActivity(base::Time::Now());
  if (!is_active_)
    last_updated_time_ticks_ = base::TimeTicks::Now();
}

void AppActivity::SetAppActive(base::Time timestamp) {
  DCHECK(!is_active_);
  DCHECK(app_state_ == AppState::kAvailable ||
         app_state_ == AppState::kAlwaysAvailable);
  is_active_ = true;
  last_updated_time_ticks_ = base::TimeTicks::Now();
}

void AppActivity::SetAppInactive(base::Time timestamp) {
  if (!is_active_)
    return;
  CaptureOngoingActivity(timestamp);
  is_active_ = false;
}

void AppActivity::ResetRunningActiveTime(base::Time timestamp) {
  CaptureOngoingActivity(timestamp);
  running_active_time_ = base::Minutes(0);
}

base::TimeDelta AppActivity::RunningActiveTime() const {
  if (!is_active_)
    return running_active_time_;

  return running_active_time_ +
         (base::TimeTicks::Now() - last_updated_time_ticks_);
}

void AppActivity::CaptureOngoingActivity(base::Time timestamp) {
  if (!is_active_)
    return;

  // Log the active time before the until the reset.
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta active_time = now - last_updated_time_ticks_;

  // Update |running_active_time_|.
  running_active_time_ += active_time;

  base::Time start_time = timestamp - active_time;

  // Timestamps can be equal if SetAppInactive() is called directly after
  // SetAppState(). Happens in tests.
  DCHECK_GE(timestamp, start_time);
  if (timestamp > start_time)
    active_times_.push_back(ActiveTime(start_time, timestamp));

  last_updated_time_ticks_ = now;
}

std::vector<AppActivity::ActiveTime> AppActivity::TakeActiveTimes() {
  return std::move(active_times_);
}

}  // namespace app_time
}  // namespace ash
