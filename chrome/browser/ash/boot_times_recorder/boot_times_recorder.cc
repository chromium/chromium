// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace ash {

namespace {

using ::content::BrowserThread;
using ::content::NavigationController;
using ::content::RenderWidgetHost;
using ::content::RenderWidgetHostView;
using ::content::WebContents;

const std::string GetTabUrl(RenderWidgetHost* rwh) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    for (int i = 0, tab_count = browser->tab_strip_model()->count();
         i < tab_count;
         ++i) {
      WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(i);
      if (tab->GetPrimaryMainFrame()->GetRenderWidgetHost() == rwh) {
        return tab->GetLastCommittedURL().spec();
      }
    }
  }
  return std::string();
}

}  // namespace

#define FPL(value) FILE_PATH_LITERAL(value)

// Name of the time that Chrome's main() is called.
constexpr base::FilePath::CharType kChromeMain[] = FPL("chrome-main");

// Names of login stats files.
constexpr base::FilePath::CharType kChromeFirstRender[] =
    FPL("chrome-first-render");

// Names of login UMA values.
static const char kUmaLogin[] = "BootTime.Login2";
static const char kUmaLoginNewUser[] = "BootTime.LoginNewUser";
constexpr char kUmaLogout[] = "ShutdownTime.Logout";
constexpr char kUmaRestart[] = "ShutdownTime.Restart";

// Name of file collecting login times.
constexpr base::FilePath::CharType kLoginTimes[] = FPL("login-times");

// Name of file collecting logout times.
constexpr char kLogoutTimes[] = "logout-times";

static base::LazyInstance<BootTimesRecorder>::DestructorAtExit
    g_boot_times_recorder = LAZY_INSTANCE_INITIALIZER;

BootTimesRecorder::BootTimesRecorder() = default;

BootTimesRecorder::~BootTimesRecorder() = default;

// static
BootTimesRecorder* BootTimesRecorder::Get() {
  return g_boot_times_recorder.Pointer();
}

// static
BootTimesRecorder* BootTimesRecorder::GetIfCreated() {
  if (!g_boot_times_recorder.IsCreated()) {
    return nullptr;
  }
  return g_boot_times_recorder.Pointer();
}

void BootTimesRecorder::LoginDone(bool is_user_new) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (login_done_)
    return;

  login_done_ = true;
  login_started_ = false;
  AddLoginTimeMarker("LoginDone", false);
  RecordCurrentStats(kChromeFirstRender);
  LoginEventRecorder::Get()->ScheduleWriteLoginTimes(
      kLoginTimes, (is_user_new ? kUmaLoginNewUser : kUmaLogin),
      kUmaLoginPrefix);
}

void BootTimesRecorder::WriteLogoutTimes() {
  // Either we're on the browser thread, or (more likely) Chrome is in the
  // process of shutting down and we're on the main thread but the message loop
  // has already been terminated.
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  LoginEventRecorder::Get()->WriteLogoutTimes(
      kLogoutTimes, (restart_requested_ ? kUmaRestart : kUmaLogout),
      kUmaLogoutPrefix);
}

void BootTimesRecorder::AddLoginTimeMarkerWithURL(const char* marker_name,
                                                  const std::string& url) {
  LoginEventRecorder::Get()->AddLoginTimeMarkerWithURL(marker_name, url, false);
}

// static
void BootTimesRecorder::ClearLogoutStartedLastPreference() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kLogoutStartedLast);
}

void BootTimesRecorder::OnChromeProcessStart() {
  PrefService* local_state = g_browser_process->local_state();
  const std::string logout_started_last_str =
      local_state->GetString(prefs::kLogoutStartedLast);
  if (logout_started_last_str.empty())
    return;

  // Note that kLogoutStartedLast is not cleared on format error to stay in
  // logs in case of other fatal system errors.

  const LoginEventRecorder::Stats logout_started_last_stats =
      LoginEventRecorder::Stats::DeserializeFromString(logout_started_last_str);
  if (logout_started_last_stats.uptime().empty())
    return;

  double logout_started_last;
  double uptime;
  if (!logout_started_last_stats.UptimeDouble(&logout_started_last) ||
      !LoginEventRecorder::Stats::GetCurrentStats().UptimeDouble(&uptime)) {
    return;
  }

  if (logout_started_last >= uptime) {
    // Reboot happened.
    ClearLogoutStartedLastPreference();
    return;
  }

  // Write /run/bootstat/uptime-logout-started as well.
  constexpr char kLogoutStarted[] = "logout-started";
  logout_started_last_stats.RecordStatsWithCallback(
      kLogoutStarted, /*write_flag_file=*/true,
      base::BindOnce(&BootTimesRecorder::ClearLogoutStartedLastPreference));
}

void BootTimesRecorder::OnLogoutStarted(PrefService* state) {
  const std::string uptime =
      LoginEventRecorder::Stats::GetCurrentStats().SerializeToString();
  if (!uptime.empty())
    state->SetString(prefs::kLogoutStartedLast, uptime);
}

void BootTimesRecorder::RecordCurrentStats(const std::string& name) {
  LoginEventRecorder::Get()->RecordCurrentStats(name);
}

void BootTimesRecorder::SaveChromeMainStats() {
  chrome_main_stats_ = LoginEventRecorder::Stats::GetCurrentStats();
}

void BootTimesRecorder::RecordChromeMainStats() {
  chrome_main_stats_.RecordStats(kChromeMain, /*write_flag_file=*/false);
}

void BootTimesRecorder::RecordLoginAttempted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (login_done_)
    return;

  LoginEventRecorder::Get()->ClearLoginTimeMarkers();
  AddLoginTimeMarker("LoginStarted", false);
  login_started_ = true;
}

void BootTimesRecorder::AddLoginTimeMarker(const char* marker_name,
                                           bool send_to_uma) {
  LoginEventRecorder::Get()->AddLoginTimeMarker(marker_name, send_to_uma);
}

void BootTimesRecorder::AddLogoutTimeMarker(const char* marker_name,
                                            bool send_to_uma) {
  LoginEventRecorder::Get()->AddLogoutTimeMarker(marker_name, send_to_uma);
}

void BootTimesRecorder::TabLoadStart(WebContents* web_contents) {
  if (!login_started_ || login_done_) {
    return;
  }

  AddLoginTimeMarkerWithURL("TabLoad-Start",
                            web_contents->GetLastCommittedURL().spec());

  RenderWidgetHost* rwh =
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  if (render_widget_host_observations_.IsObservingSource(rwh)) {
    return;
  }

  render_widget_host_observations_.AddObservation(rwh);
}

void BootTimesRecorder::TabLoadEnd(WebContents* web_contents) {
  if (!login_started_ || login_done_) {
    return;
  }

  RenderWidgetHost* rwh =
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  if (!render_widget_host_observations_.IsObservingSource(rwh)) {
    return;
  }

  AddLoginTimeMarkerWithURL("TabLoad-End",
                            web_contents->GetLastCommittedURL().spec());
}

void BootTimesRecorder::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // `old_host` may be null when creating the initial RenderFrameHost in a new
  // tab.
  if (!old_host) {
    return;
  }

  RenderWidgetHost* old_rwh = old_host->GetRenderWidgetHost();
  if (render_widget_host_observations_.IsObservingSource(old_rwh)) {
    render_widget_host_observations_.RemoveObservation(old_rwh);
    render_widget_host_observations_.AddObservation(
        new_host->GetRenderWidgetHost());
  }
}

void BootTimesRecorder::RenderWidgetHostDidUpdateVisualProperties(
    content::RenderWidgetHost* widget_host) {
  AddLoginTimeMarkerWithURL("TabPaint", GetTabUrl(widget_host));
}

void BootTimesRecorder::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  render_widget_host_observations_.RemoveObservation(widget_host);
}

}  // namespace ash
