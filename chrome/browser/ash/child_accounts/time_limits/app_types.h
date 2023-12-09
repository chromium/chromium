// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash {
namespace app_time {

// Type of usage restriction that can be applied to the installed app.
enum class AppRestriction {
  kUnknown,
  // Installed app is not available for the user.
  kBlocked,
  // Daily time limit is enforced. Installed app will become unavailable for
  // the user after time limit is reached on a given day.
  kTimeLimit,
};

// State of the app. Used for activity recording and status reporting. The enum
// values are persisted in user pref service. Existing values should never be
// deleted or reordered. New states should be appended at the end.
enum class AppState {
  // App is available for the user.
  kAvailable = 0,
  // App cannot be restricted. Used for important system apps.
  kAlwaysAvailable = 1,
  // App is not available for the user because of being blocked.
  kBlocked = 2,
  // App is not available for the user because daily time limit was reached.
  kLimitReached = 3,
  // App is uninstalled. Activity might still be preserved and reported for
  // recently uninstalled apps.
  kUninstalled = 4,
};

// Type of notification to show the child user.
enum class AppNotification {
  kUnknown,

  // Five minutes left before the application's time limit is reached.
  kFiveMinutes,

  // One minjute left before the application's time limit is reached.
  kOneMinute,

  // Application's time limit reached.
  kTimeLimitReached,

  // Application's time limit has been updated by parents.
  kTimeLimitChanged,

  // Application is blocked.
  kBlocked,

  // Application is unblocked.
  kAvailable
};

enum class ChromeAppActivityState {
  // The browser is active and hosts urls in its active tab which are not
  // allowlisted.
  kActive,

  // Same as |kActive| except the urls the browser hosts are allowlisted.
  kActiveAllowlisted,

  // The browser window is not active.
  kInactive,
};

// Identifies an app for app time limits.
// Different types of use different identifier format. ARC++ apps are identified
// by Android package name. Other types of apps use 32 character long Chrome
// specific app id.
class AppId {
 public:
  AppId(apps::AppType app_type, const std::string& app_id);
  AppId(const AppId&);
  AppId& operator=(const AppId&);
  AppId(AppId&&);
  AppId& operator=(AppId&&);
  ~AppId();

  apps::AppType app_type() const { return app_type_; }
  const std::string& app_id() const { return app_id_; }

  bool operator==(const AppId&) const;
  bool operator!=(const AppId&) const;
  bool operator<(const AppId&) const;
  friend std::ostream& operator<<(std::ostream&, const AppId&);

 private:
  apps::AppType app_type_ = apps::AppType::kUnknown;

  // Package name for |ARC| apps, 32 character long Chrome specific app id
  // otherwise.
  std::string app_id_;
};

struct PauseAppInfo {
  PauseAppInfo(const AppId& app, base::TimeDelta limit, bool show_dialog);

  AppId app_id;
  base::TimeDelta daily_limit;
  bool show_pause_dialog = true;
};

// Represents restriction that can be applied to an installed app.
class AppLimit {
 public:
  // Creates AppLimit.
  // |daily_limit| can only be set when |restriction| is kTimeLimit.
  // |daily_limit| needs to be in range of [0, 24] hours.
  AppLimit(AppRestriction restriction,
           std::optional<base::TimeDelta> daily_limit,
           base::Time last_updated);
  AppLimit(const AppLimit&);
  AppLimit& operator=(const AppLimit&);
  AppLimit(AppLimit&&);
  AppLimit& operator=(AppLimit&&);
  ~AppLimit();

  AppRestriction restriction() const { return restriction_; }
  base::Time last_updated() const { return last_updated_; }
  const std::optional<base::TimeDelta>& daily_limit() const {
    return daily_limit_;
  }

 private:
  // Usage restriction applied to the app.
  AppRestriction restriction_ = AppRestriction::kUnknown;

  // Daily usage limit. Only set |restriction| is kTimeLimit.
  // Has to be between 0 and 24 hours.
  std::optional<base::TimeDelta> daily_limit_;

  // UTC timestamp for the last time the limit was updated.
  base::Time last_updated_;
};

// Contains information about app usage.
class AppActivity {
 public:
  class ActiveTime {
   public:
    static const base::TimeDelta kActiveTimeMergePrecision;

    // If |t1| and |t2| overlap or are within |kActiveTimeMergePrecision| of
    // each other, this static method creates a new ActiveTime with the earlier
    // of |t1|'s or |t2|'s |active_from| and the later of |t1|'s or |t2|'s
    // |active_to_|.
    static std::optional<ActiveTime> Merge(const ActiveTime& t1,
                                           const ActiveTime& t2);

    ActiveTime(base::Time start, base::Time end);
    ActiveTime(const ActiveTime& rhs);
    ActiveTime& operator=(const ActiveTime& rhs);

    bool operator==(const ActiveTime&) const;
    bool operator!=(const ActiveTime&) const;

    // Returns whether |timestamp| is included in this time period.
    bool Contains(base::Time timestamp) const;

    // Returns whether |timestamp| is earlier than this time period's start.
    bool IsEarlierThan(base::Time timestamp) const;

    // Returns whether |timestamp| is later than this time period's end.
    bool IsLaterThan(base::Time timestamp) const;

    base::Time active_from() const { return active_from_; }
    void set_active_from(base::Time active_from);
    base::Time active_to() const { return active_to_; }
    void set_active_to(base::Time active_to);

   private:
    base::Time active_from_;
    base::Time active_to_;
  };

  // Creates AppActivity and sets current |app_state_|.
  explicit AppActivity(AppState app_state);
  AppActivity(AppState app_state, base::TimeDelta running_active_time);
  AppActivity(const AppActivity&);
  AppActivity& operator=(const AppActivity&);
  AppActivity(AppActivity&&);
  AppActivity& operator=(AppActivity&&);
  ~AppActivity();

  void SetAppState(AppState app_state);
  void SetAppActive(base::Time timestamp);
  void SetAppInactive(base::Time timestamp);

  // Called when reset time has been reached.
  // Resets |running_active_time_|.
  // If the application is currently running, uses |timestamp| as current time
  // to log activity.
  void ResetRunningActiveTime(base::Time timestamp);

  base::TimeDelta RunningActiveTime() const;

  // Updates |active_times_| to include the current activity. If the app is
  // active, it saves the activitity until |timestamp|.
  void CaptureOngoingActivity(base::Time timestamp);

  // Caller takes ownership of |active_times_| i.e. |active_times_| is moved and
  // thus becomes empty after this method is called. Called from
  // AppActivityRegistry::SaveAppActivity when the app activity is going to be
  // saved in user preference.
  std::vector<ActiveTime> TakeActiveTimes();

  bool is_active() const { return is_active_; }
  AppState app_state() const { return app_state_; }
  const std::vector<ActiveTime>& active_times() const { return active_times_; }
  AppNotification last_notification() const { return last_notification_; }

  void set_last_notification(AppNotification notification) {
    last_notification_ = notification;
  }

  // Chrome and web apps share the same time limit. Therefore, we need to have a
  // consistent |running_active_time_| across all web apps and chrome.
  void set_running_active_time(base::TimeDelta time) {
    DCHECK(!is_active_);
    running_active_time_ = time;
  }

 private:
  // boolean to specify if the application is active.
  bool is_active_ = false;

  AppNotification last_notification_ = AppNotification::kUnknown;

  // Current state of the app.
  // There might be relevant activity recoded for app that was uninstalled
  // recently.
  AppState app_state_ = AppState::kAvailable;

  // Keeps the sum of the active times since the last reset.
  base::TimeDelta running_active_time_;

  // The time app was active.
  std::vector<ActiveTime> active_times_;

  // Time tick for the last time the activity was updated.
  base::TimeTicks last_updated_time_ticks_;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_TYPES_H_
