// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boot_times_recorder.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace ash {

namespace {

using ::content::BrowserThread;
using ::content::NavigationController;
using ::content::RenderWidgetHost;
using ::content::RenderWidgetHostView;
using ::content::WebContents;

RenderWidgetHost* GetRenderWidgetHost(NavigationController* tab) {
  WebContents* web_contents = tab->DeprecatedGetWebContents();
  if (web_contents) {
    RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return nullptr;
}

const std::string GetTabUrl(RenderWidgetHost* rwh) {
  // rwh is null during initialization and shut down.
  if (!rwh)
    return std::string();

  RenderWidgetHostView* rwhv = rwh->GetView();
  // rwhv is null if renderer has crashed.
  if (!rwhv)
    return std::string();

  for (auto* browser : *BrowserList::GetInstance()) {
    for (int i = 0, tab_count = browser->tab_strip_model()->count();
         i < tab_count;
         ++i) {
      WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(i);
      if (tab->GetRenderWidgetHostView() == rwhv) {
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
constexpr char kUmaLoginPrefix[] = "BootTime.";
constexpr char kUmaLogout[] = "ShutdownTime.Logout";
constexpr char kUmaLogoutPrefix[] = "ShutdownTime.";
constexpr char kUmaRestart[] = "ShutdownTime.Restart";

// Name of file collecting login times.
constexpr base::FilePath::CharType kLoginTimes[] = FPL("login-times");

// Name of file collecting logout times.
constexpr char kLogoutTimes[] = "logout-times";

static base::LazyInstance<BootTimesRecorder>::DestructorAtExit
    g_boot_times_recorder = LAZY_INSTANCE_INITIALIZER;

BootTimesRecorder::BootTimesRecorder()
    : have_registered_(false),
      login_done_(false),
      restart_requested_(false) {
}

BootTimesRecorder::~BootTimesRecorder() {
}

// static
BootTimesRecorder* BootTimesRecorder::Get() {
  return g_boot_times_recorder.Pointer();
}

void BootTimesRecorder::LoginDone(bool is_user_new) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (login_done_)
    return;

  login_done_ = true;
  AddLoginTimeMarker("LoginDone", false);
  RecordCurrentStats(kChromeFirstRender);
  if (have_registered_) {
    registrar_.Remove(this,
                      content::NOTIFICATION_LOAD_START,
                      content::NotificationService::AllSources());
    registrar_.Remove(this,
                      content::NOTIFICATION_LOAD_STOP,
                      content::NotificationService::AllSources());
    render_widget_host_observations_.RemoveAllObservations();
  }
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

  // Write /tmp/uptime-logout-started as well.
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
  if (!have_registered_) {
    have_registered_ = true;
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
  }
}

void BootTimesRecorder::AddLoginTimeMarker(const char* marker_name,
                                           bool send_to_uma) {
  LoginEventRecorder::Get()->AddLoginTimeMarker(marker_name, send_to_uma);
}

void BootTimesRecorder::AddLogoutTimeMarker(const char* marker_name,
                                            bool send_to_uma) {
  LoginEventRecorder::Get()->AddLogoutTimeMarker(marker_name, send_to_uma);
}

void BootTimesRecorder::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* rwh = GetRenderWidgetHost(tab);
      DCHECK(rwh);
      AddLoginTimeMarkerWithURL("TabLoad-Start", GetTabUrl(rwh));
      if (!render_widget_host_observations_.IsObservingSource(rwh))
        render_widget_host_observations_.AddObservation(rwh);
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* rwh = GetRenderWidgetHost(tab);
      if (render_widget_host_observations_.IsObservingSource(rwh)) {
        AddLoginTimeMarkerWithURL("TabLoad-End", GetTabUrl(rwh));
      }
      break;
    }
    default:
      break;
  }
}

void BootTimesRecorder::RenderWidgetHostDidUpdateVisualProperties(
    content::RenderWidgetHost* widget_host) {
  DCHECK(have_registered_);
  AddLoginTimeMarkerWithURL("TabPaint", GetTabUrl(widget_host));
  LoginDone(user_manager::UserManager::Get()->IsCurrentUserNew());
}

void BootTimesRecorder::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  render_widget_host_observations_.RemoveObservation(widget_host);
}

}  // namespace ash
