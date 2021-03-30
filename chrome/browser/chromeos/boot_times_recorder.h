// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOOT_TIMES_RECORDER_H_
#define CHROME_BROWSER_CHROMEOS_BOOT_TIMES_RECORDER_H_

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_multi_source_observation.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chromeos/login/auth/login_event_recorder.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"

class PrefService;

namespace chromeos {

// BootTimesRecorder is used to record times of boot, login, and logout.
class BootTimesRecorder : public content::NotificationObserver,
                          public content::RenderWidgetHostObserver,
                          public LoginEventRecorder::Delegate {
 public:
  BootTimesRecorder();
  ~BootTimesRecorder() override;

  static BootTimesRecorder* Get();

  // LoginEventRecorder::Delegate override.
  void AddLoginTimeMarker(const std::string& marker_name,
                          bool send_to_uma) override;
  void RecordAuthenticationSuccess() override;
  void RecordAuthenticationFailure() override;

  // Add a time marker for logout. A timeline will be dumped to
  // /tmp/logout-times-sent after logout is done. If |send_to_uma| is true
  // the time between this marker and the last will be sent to UMA with
  // the identifier ShutdownTime.|marker_name|.
  void AddLogoutTimeMarker(const std::string& marker_name, bool send_to_uma);

  // Records current uptime and disk usage for metrics use.
  // Posts task to file thread.
  // name will be used as part of file names in /tmp.
  // Existing stats files will not be overwritten.
  void RecordCurrentStats(const std::string& name);

  // Saves away the stats at main, so the can be recorded later. At main() time
  // the necessary threads don't exist yet for recording the data.
  void SaveChromeMainStats();

  // Records the data previously saved by SaveChromeMainStats(), using the
  // file thread. Existing stats files will not be overwritten.
  void RecordChromeMainStats();

  // Records the time that a login was attempted. This will overwrite any
  // previous login attempt times.
  void RecordLoginAttempted();

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Records "LoginDone" event.
  void LoginDone(bool is_user_new);

  // Writes the logout times to a /tmp/logout-times-sent. Unlike login
  // times, we manually call this function for logout times, as we cannot
  // rely on notification service to tell when the logout is done.
  void WriteLogoutTimes();

  // Mark that WriteLogoutTimes should handle restart.
  void set_restart_requested() { restart_requested_ = true; }

  // This is called on Chrome process startup to write saved logout stats.
  void OnChromeProcessStart();

  // This saves logout-started metric to Local State.
  void OnLogoutStarted(PrefService* state);

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDidUpdateVisualProperties(
      content::RenderWidgetHost* widget_host) override;
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

 private:
  class TimeMarker {
   public:
    TimeMarker(const std::string& name, bool send_to_uma)
        : name_(name),
          time_(base::Time::NowFromSystemTime()),
          send_to_uma_(send_to_uma) {}
    std::string name() const { return name_; }
    base::Time time() const { return time_; }
    bool send_to_uma() const { return send_to_uma_; }

    // comparitor for sorting
    bool operator<(const TimeMarker& other) const {
      return time_ < other.time_;
    }

   private:
    friend class std::vector<TimeMarker>;
    std::string name_;
    base::Time time_;
    bool send_to_uma_;
  };

  class Stats {
   public:
    // Initializes stats with current /proc values.
    static Stats GetCurrentStats();

    // Returns JSON representation.
    std::string SerializeToString() const;

    // Creates new object from JSON representation.
    static Stats DeserializeFromString(const std::string& value);

    const std::string& uptime() const { return uptime_; }
    const std::string& disk() const { return disk_; }

    // Writes "uptime in seconds" to result. (This is first field in uptime_.)
    // Returns true on successful conversion.
    bool UptimeDouble(double* result) const;

    void RecordStats(const std::string& name) const;
    void RecordStatsWithCallback(const std::string& name,
                                 base::OnceClosure callback) const;

   private:
    // Runs asynchronously when RecordStats(WithCallback) is called.
    void RecordStatsAsync(const std::string& name) const;

    std::string uptime_;
    std::string disk_;
  };

  static void WriteTimes(const std::string base_name,
                         const std::string uma_name,
                         const std::string uma_prefix,
                         std::vector<TimeMarker> login_times);
  static void AddMarker(std::vector<TimeMarker>* vector, TimeMarker marker);

  // Clear saved logout-started metric in Local State.
  // This method is called when logout-state was writen to file.
  static void ClearLogoutStartedLastPreference();

  // Used to hold the stats at main().
  Stats chrome_main_stats_;

  // Used to track notifications for login.
  content::NotificationRegistrar registrar_;
  base::AtomicSequenceNumber num_tabs_;
  bool have_registered_;

  std::vector<TimeMarker> login_time_markers_;
  std::vector<TimeMarker> logout_time_markers_;

  base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                     content::RenderWidgetHostObserver>
      render_widget_host_observations_{this};

  bool login_done_;

  bool restart_requested_;

  DISALLOW_COPY_AND_ASSIGN(BootTimesRecorder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BOOT_TIMES_RECORDER_H_
