// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session.h"

#include <errno.h>
#include <signal.h>

#include "base/bind.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace chromeos {

const char kKioskMetrics[] = "kiosk-metrics";

const char kKioskSessionStateHistogram[] = "Kiosk.SessionState";
const char kKioskSessionCountPerDayHistogram[] = "Kiosk.Session.CountPerDay";
const char kKioskSessionDurationNormalHistogram[] =
    "Kiosk.SessionDuration.Normal";
const char kKioskSessionDurationInDaysNormalHistogram[] =
    "Kiosk.SessionDurationInDays.Normal";
const char kKioskSessionDurationCrashedHistogram[] =
    "Kiosk.SessionDuration.Crashed";
const char kKioskSessionDurationInDaysCrashedHistogram[] =
    "Kiosk.SessionDurationInDays.Crashed";
const char kKioskSessionLastDayList[] = "last-day-sessions";
const char kKioskSessionStartTime[] = "session-start-time";

const int kKioskHistogramBucketCount = 100;
const base::TimeDelta kKioskSessionDurationHistogramLimit = base::Days(1);

namespace {

bool IsPepperPlugin(const base::FilePath& plugin_path) {
  content::WebPluginInfo plugin_info;
  return content::PluginService::GetInstance()->GetPluginInfoByPath(
             plugin_path, &plugin_info) &&
         plugin_info.is_pepper_plugin();
}

void RebootDevice() {
  // TODO (anqing): a new crosapi needs to be built to notify the reboot from
  // lacros to ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "kiosk app session");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Sends a SIGFPE signal to plugin subprocesses that matches |child_ids|
// to trigger a dump.
void DumpPluginProcess(const std::set<int>& child_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool dump_requested = false;

  content::BrowserChildProcessHostIterator iter(
      content::PROCESS_TYPE_PPAPI_PLUGIN);
  while (!iter.Done()) {
    const content::ChildProcessData& data = iter.GetData();
    if (child_ids.count(data.id) == 1) {
      // Send a signal to dump the plugin process.
      if (kill(data.GetProcess().Handle(), SIGFPE) == 0) {
        dump_requested = true;
      } else {
        PLOG(WARNING) << "Failed to send SIGFPE to plugin process"
                      << ", pid=" << data.GetProcess().Pid()
                      << ", type=" << data.process_type
                      << ", name=" << data.name;
      }
    }
    ++iter;
  }

  // Wait a bit to let dump finish (if requested) before rebooting the device.
  const int kDumpWaitSeconds = 10;
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RebootDevice),
      base::Seconds(dump_requested ? kDumpWaitSeconds : 0));
}

}  // namespace

class AppSessionMetricsService {
 public:
  explicit AppSessionMetricsService(PrefService* prefs) : prefs_(prefs) {}
  AppSessionMetricsService(AppSessionMetricsService&) = delete;
  AppSessionMetricsService& operator=(const AppSessionMetricsService&) = delete;
  ~AppSessionMetricsService() = default;

  void RecordKioskSessionStarted() {
    RecordPreviousKioskSessionCrashIfAny();
    RecordKioskSessionState(KioskSessionState::kStarted);
    RecordKioskSessionCountPerDay();
  }

  void RecordKioskSessionWebStarted() {
    RecordPreviousKioskSessionCrashIfAny();
    RecordKioskSessionState(KioskSessionState::kWebStarted);
    RecordKioskSessionCountPerDay();
  }

  void RecordKioskSessionStopped() {
    RecordKioskSessionState(KioskSessionState::kStopped);
    RecordKioskSessionDuration(kKioskSessionDurationNormalHistogram,
                               kKioskSessionDurationInDaysNormalHistogram);
  }

  void RecordKioskSessionCrashed() {
    RecordKioskSessionState(KioskSessionState::kCrashed);
    RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                               kKioskSessionDurationInDaysCrashedHistogram);
  }

  void RecordKioskSessionPluginCrashed() {
    RecordKioskSessionState(KioskSessionState::kPluginCrashed);
    RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                               kKioskSessionDurationInDaysCrashedHistogram);
  }

  void RecordKioskSessionPluginHung() {
    RecordKioskSessionState(KioskSessionState::kPluginHung);
    RecordKioskSessionDuration(kKioskSessionDurationCrashedHistogram,
                               kKioskSessionDurationInDaysCrashedHistogram);
  }

 private:
  void RecordKioskSessionState(KioskSessionState state) const {
    base::UmaHistogramEnumeration(kKioskSessionStateHistogram, state);
  }

  void RecordKioskSessionCountPerDay() {
    base::UmaHistogramCounts100(kKioskSessionCountPerDayHistogram,
                                RetrieveLastDaySessionCount(base::Time::Now()));
  }

  void RecordKioskSessionDuration(
      const std::string& kiosk_session_duration_histogram,
      const std::string& kiosk_session_duration_in_days_histogram) {
    if (start_time_.is_null())
      return;
    base::TimeDelta duration = base::Time::Now() - start_time_;
    if (duration >= kKioskSessionDurationHistogramLimit) {
      base::UmaHistogramCounts100(kiosk_session_duration_in_days_histogram,
                                  std::min(100, duration.InDays()));
      duration = kKioskSessionDurationHistogramLimit;
    }
    base::UmaHistogramCustomTimes(
        kiosk_session_duration_histogram, duration, base::Seconds(1),
        kKioskSessionDurationHistogramLimit, kKioskHistogramBucketCount);
    ClearStartTime();
  }

  void RecordPreviousKioskSessionCrashIfAny() {
    const auto* metrics_value = prefs_->GetDictionary(kKioskMetrics);

    if (!metrics_value)
      return;
    const auto* metrics_dict = metrics_value->GetIfDict();
    DCHECK(metrics_dict);

    const auto* previous_start_time_value =
        metrics_dict->Find(kKioskSessionStartTime);
    if (!previous_start_time_value)
      return;
    auto previous_start_time = base::ValueToTime(previous_start_time_value);
    if (!previous_start_time.has_value())
      return;
    // Setup |start_time_| to the previous not correctly completed session's
    // start time. |start_time_| will be cleared once the crash session metrics
    // are recorded.
    start_time_ = previous_start_time.value();
    RecordKioskSessionCrashed();
  }

  size_t RetrieveLastDaySessionCount(base::Time session_start_time) {
    const auto* metrics_value = prefs_->GetDictionary(kKioskMetrics);
    const base::Value::List* previous_times = nullptr;
    if (metrics_value) {
      const auto* metrics_dict = metrics_value->GetIfDict();
      DCHECK(metrics_dict);

      const auto* times_value = metrics_dict->Find(kKioskSessionLastDayList);
      if (times_value) {
        previous_times = times_value->GetIfList();
        DCHECK(previous_times);
      }
    }

    base::Value::List times;
    if (previous_times) {
      for (const auto& time : *previous_times) {
        if (base::ValueToTime(time).has_value() &&
            session_start_time - base::ValueToTime(time).value() <=
                base::Days(1)) {
          times.Append(time.Clone());
        }
      }
    }
    times.Append(base::TimeToValue(session_start_time));
    size_t result = times.size();

    start_time_ = session_start_time;

    base::Value::Dict result_value;
    result_value.Set(kKioskSessionLastDayList, std::move(times));
    result_value.Set(kKioskSessionStartTime, base::TimeToValue(start_time_));
    prefs_->SetDict(kKioskMetrics, std::move(result_value));
    return result;
  }

  void ClearStartTime() {
    start_time_ = base::Time();
    const auto* metrics_value = prefs_->GetDictionary(kKioskMetrics);
    if (!metrics_value)
      return;
    const auto* metrics_dict = metrics_value->GetIfDict();
    DCHECK(metrics_dict);

    base::Value::Dict new_metrics_dict = metrics_dict->Clone();
    DCHECK(new_metrics_dict.Remove(kKioskSessionStartTime));

    prefs_->SetDict(kKioskMetrics, std::move(new_metrics_dict));
  }

  PrefService* prefs_;
  // Initialized once the kiosk session is started or during recording of the
  // previously crashed kiosk session metrics.
  // Cleared once the session's duration metric is recorded:
  // either the session is successfully finished or crashed or on the next
  // session startup.
  base::Time start_time_;
};

class AppSession::AppWindowHandler : public AppWindowRegistry::Observer {
 public:
  explicit AppWindowHandler(AppSession* app_session)
      : app_session_(app_session) {}
  AppWindowHandler(const AppWindowHandler&) = delete;
  AppWindowHandler& operator=(const AppWindowHandler&) = delete;
  ~AppWindowHandler() override {}

  void Init(Profile* profile, const std::string& app_id) {
    DCHECK(!window_registry_);
    window_registry_ = AppWindowRegistry::Get(profile);
    if (window_registry_)
      window_registry_->AddObserver(this);
    app_id_ = app_id;
  }

 private:
  // extensions::AppWindowRegistry::Observer overrides:
  void OnAppWindowAdded(AppWindow* app_window) override {
    if (app_window->extension_id() != app_id_)
      return;

    app_session_->OnAppWindowAdded(app_window);
    app_window_created_ = true;
  }

  void OnAppWindowRemoved(AppWindow* app_window) override {
    if (!app_window_created_ ||
        !window_registry_->GetAppWindowsForApp(app_id_).empty()) {
      return;
    }

    app_session_->OnLastAppWindowClosed();
    window_registry_->RemoveObserver(this);
  }

  AppSession* const app_session_;
  AppWindowRegistry* window_registry_ = nullptr;
  std::string app_id_;
  bool app_window_created_ = false;
};

class AppSession::BrowserWindowHandler : public BrowserListObserver {
 public:
  BrowserWindowHandler(AppSession* app_session, Browser* browser)
      : app_session_(app_session), browser_(browser) {
    BrowserList::AddObserver(this);
  }
  BrowserWindowHandler(const BrowserWindowHandler&) = delete;
  BrowserWindowHandler& operator=(const BrowserWindowHandler&) = delete;
  ~BrowserWindowHandler() override { BrowserList::RemoveObserver(this); }

 private:
  void HandleBrowser(Browser* browser) {
    content::WebContents* active_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    std::string url_string =
        active_tab ? active_tab->GetURL().spec() : std::string();

    if (KioskSettingsNavigationThrottle::IsSettingsPage(url_string)) {
      bool app_browser = browser->is_type_app() ||
                         browser->is_type_app_popup() ||
                         browser->is_type_popup();
      // If this browser is not an app browser or another settings browser
      // exists, close this one and navigate to |url_string| in the old browser
      // or create a new app browser if none yet exists.
      if (!app_browser || app_session_->settings_browser_) {
        browser->window()->Close();
        if (!app_session_->settings_browser_) {
          // Create a new app browser.
          NavigateParams nav_params(
              app_session_->profile_, GURL(url_string),
              ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);
          nav_params.disposition = WindowOpenDisposition::NEW_POPUP;
          Navigate(&nav_params);
        } else {
          // Navigate in the existing browser.
          NavigateParams nav_params(
              app_session_->settings_browser_, GURL(url_string),
              ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);

          Navigate(&nav_params);
        }
      } else {
        app_session_->settings_browser_ = browser;
        // We have to first call Restore() because the window was created as a
        // fullscreen window, having no prior bounds.
        // TODO(crbug.com/1015383): Figure out how to do it more cleanly.
        browser->window()->Restore();
        browser->window()->Maximize();
      }
    } else {
      LOG(WARNING) << "Browser opened in kiosk session"
                   << ", url=" << url_string;
      browser->window()->Close();
    }
    // Call the callback to notify tests that browser was handled.
    if (app_session_->on_handle_browser_callback_)
      app_session_->on_handle_browser_callback_.Run();
  }

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserWindowHandler::HandleBrowser,
                       base::Unretained(this),  // LazyInstance, always valid
                       browser));
  }

  // Called when a Browser is removed from the list.
  void OnBrowserRemoved(Browser* browser) override {
    // The app browser was removed.
    if (browser == browser_) {
      app_session_->OnLastAppWindowClosed();
    }

    if (browser == app_session_->settings_browser_) {
      app_session_->settings_browser_ = nullptr;
    }
  }

  AppSession* const app_session_;
  Browser* const browser_;
};

AppSession::AppSession()
    : attempt_user_exit_(base::BindOnce(chrome::AttemptUserExit)),
      metrics_service_(std::make_unique<AppSessionMetricsService>(
          g_browser_process->local_state())) {}
AppSession::AppSession(base::OnceClosure attempt_user_exit,
                       PrefService* local_state)
    : attempt_user_exit_(std::move(attempt_user_exit)),
      metrics_service_(
          std::make_unique<AppSessionMetricsService>(local_state)) {}
AppSession::~AppSession() {
  if (!is_shutting_down_)
    metrics_service_->RecordKioskSessionCrashed();
}

void AppSession::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kKioskMetrics);
}

void AppSession::Init(Profile* profile, const std::string& app_id) {
  SetProfile(profile);
  app_window_handler_ = std::make_unique<AppWindowHandler>(this);
  app_window_handler_->Init(profile, app_id);
  CreateBrowserWindowHandler(nullptr);
  plugin_handler_ = std::make_unique<KioskSessionPluginHandler>(this);
  metrics_service_->RecordKioskSessionStarted();
}

void AppSession::InitForWebKiosk(Browser* browser) {
  SetProfile(browser->profile());
  CreateBrowserWindowHandler(browser);
  metrics_service_->RecordKioskSessionWebStarted();
}

void AppSession::SetAttemptUserExitForTesting(base::OnceClosure closure) {
  attempt_user_exit_ = std::move(closure);
}

void AppSession::SetOnHandleBrowserCallbackForTesting(
    base::RepeatingClosure closure) {
  on_handle_browser_callback_ = std::move(closure);
}

void AppSession::SetProfile(Profile* profile) {
  profile_ = profile;
}

void AppSession::CreateBrowserWindowHandler(Browser* browser) {
  browser_window_handler_ =
      std::make_unique<BrowserWindowHandler>(this, browser);
}

void AppSession::OnAppWindowAdded(AppWindow* app_window) {
  if (is_shutting_down_)
    return;

  plugin_handler_->Observe(app_window->web_contents());
}

void AppSession::OnGuestAdded(content::WebContents* guest_web_contents) {
  // Bail if the session is shutting down.
  if (is_shutting_down_)
    return;

  // Bail if the guest is not a WebViewGuest.
  if (!extensions::WebViewGuest::FromWebContents(guest_web_contents))
    return;

  plugin_handler_->Observe(guest_web_contents);
}

void AppSession::OnLastAppWindowClosed() {
  if (is_shutting_down_)
    return;
  is_shutting_down_ = true;
  metrics_service_->RecordKioskSessionStopped();

  std::move(attempt_user_exit_).Run();
}

bool AppSession::ShouldHandlePlugin(const base::FilePath& plugin_path) const {
  // Note that BrowserChildProcessHostIterator in DumpPluginProcess also needs
  // to be updated when adding more plugin types here.
  return IsPepperPlugin(plugin_path);
}

void AppSession::OnPluginCrashed(const base::FilePath& plugin_path) {
  if (is_shutting_down_)
    return;
  metrics_service_->RecordKioskSessionPluginCrashed();
  is_shutting_down_ = true;

  LOG(ERROR) << "Reboot due to plugin crash, path=" << plugin_path.value();
  RebootDevice();
}

void AppSession::OnPluginHung(const std::set<int>& hung_plugins) {
  if (is_shutting_down_)
    return;
  metrics_service_->RecordKioskSessionPluginHung();
  is_shutting_down_ = true;

  LOG(ERROR) << "Plugin hung detected. Dump and reboot.";
  DumpPluginProcess(hung_plugins);
}

}  // namespace chromeos
