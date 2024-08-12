// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_H_
#define CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_H_

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_multi_source_observation.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"

class PrefService;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ash {

// BootTimesRecorder is used to record times of boot, login, and logout.
class BootTimesRecorder : public content::RenderWidgetHostObserver {
 public:
  BootTimesRecorder();
  BootTimesRecorder(const BootTimesRecorder&) = delete;
  BootTimesRecorder& operator=(const BootTimesRecorder&) = delete;
  ~BootTimesRecorder() override;

  static BootTimesRecorder* Get();

  // Like Get(), but does not create an instance if it doesn't yet exist.
  static BootTimesRecorder* GetIfCreated();

  // TODO(oshima): Deprecate following 3 methods and just use
  // LoginEventRecorder.
  void AddLoginTimeMarker(const char* marker_name, bool send_to_uma);
  void AddLogoutTimeMarker(const char* marker_name, bool send_to_uma);

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

  // Records "LoginDone" event.
  void LoginDone(bool is_user_new);

  // Returns whether `LoginDone` has already been called.
  bool is_login_done() const { return login_done_; }

  // Records "TabLoad-Start" event.
  void TabLoadStart(content::WebContents* web_contents);

  // Records "TabLoad-End" event.
  void TabLoadEnd(content::WebContents* web_contents);

  // If needed, updates an observed RenderWidgetHost in response to a
  // RenderFrameHost swap.
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host);

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
  // Adds optional URL to the marker.
  void AddLoginTimeMarkerWithURL(const char* marker_name,
                                 const std::string& url);

  // Clear saved logout-started metric in Local State.
  // This method is called when logout-state was writen to file.
  static void ClearLogoutStartedLastPreference();

  // Used to hold the stats at mai().
  LoginEventRecorder::Stats chrome_main_stats_;

  base::ScopedMultiSourceObservation<content::RenderWidgetHost,
                                     content::RenderWidgetHostObserver>
      render_widget_host_observations_{this};

  bool login_started_ = false;

  bool login_done_ = false;

  bool restart_requested_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_H_
