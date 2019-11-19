// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/boot_times_recorder.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::NavigationController;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace {

const char kUptime[] = "uptime";
const char kDisk[] = "disk";

RenderWidgetHost* GetRenderWidgetHost(NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents) {
    RenderWidgetHostView* render_widget_host_view =
        web_contents->GetRenderWidgetHostView();
    if (render_widget_host_view)
      return render_widget_host_view->GetRenderWidgetHost();
  }
  return NULL;
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

// Appends the given buffer into the file. Returns the number of bytes
// written, or -1 on error.
// TODO(satorux): Move this to file_util.
int AppendFile(const base::FilePath& file_path, const char* data, int size) {
  // Appending boot times to (probably) a symlink in /tmp is a security risk for
  // developers with chromeos=1 builds.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return -1;

  FILE* file = base::OpenFile(file_path, "a");
  if (!file)
    return -1;

  const int num_bytes_written = fwrite(data, 1, size, file);
  base::CloseFile(file);
  return num_bytes_written;
}

}  // namespace

namespace chromeos {

#define FPL(value) FILE_PATH_LITERAL(value)

// Dir uptime & disk logs are located in.
static const base::FilePath::CharType kLogPath[] = FPL("/tmp");
// Dir log{in,out} logs are located in.
static const base::FilePath::CharType kLoginLogPath[] =
    FPL("/home/chronos/user");
// Prefix for the time measurement files.
static const base::FilePath::CharType kUptimePrefix[] = FPL("uptime-");
// Prefix for the disk usage files.
static const base::FilePath::CharType kDiskPrefix[] = FPL("disk-");
// Name of the time that Chrome's main() is called.
static const base::FilePath::CharType kChromeMain[] = FPL("chrome-main");
// Delay in milliseconds before writing the login times to disk.
static const int64_t kLoginTimeWriteDelayMs = 3000;

// Names of login stats files.
static const base::FilePath::CharType kLoginSuccess[] = FPL("login-success");
static const base::FilePath::CharType kChromeFirstRender[] =
    FPL("chrome-first-render");

// Names of login UMA values.
static const char kUmaLogin[] = "BootTime.Login";
static const char kUmaLoginNewUser[] = "BootTime.LoginNewUser";
static const char kUmaLoginPrefix[] = "BootTime.";
static const char kUmaLogout[] = "ShutdownTime.Logout";
static const char kUmaLogoutPrefix[] = "ShutdownTime.";
static const char kUmaRestart[] = "ShutdownTime.Restart";

// Name of file collecting login times.
static const base::FilePath::CharType kLoginTimes[] = FPL("login-times");

// Name of file collecting logout times.
static const char kLogoutTimes[] = "logout-times";

static base::LazyInstance<BootTimesRecorder>::DestructorAtExit
    g_boot_times_recorder = LAZY_INSTANCE_INITIALIZER;

// static
BootTimesRecorder::Stats BootTimesRecorder::Stats::GetCurrentStats() {
  const base::FilePath kProcUptime(FPL("/proc/uptime"));
  const base::FilePath kDiskStat(FPL("/sys/block/sda/stat"));
  Stats stats;
  // Callers of this method expect synchronous behavior.
  // It's safe to allow IO here, because only virtual FS are accessed.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ReadFileToString(kProcUptime, &stats.uptime_);
  base::ReadFileToString(kDiskStat, &stats.disk_);
  return stats;
}

std::string BootTimesRecorder::Stats::SerializeToString() const {
  if (uptime_.empty() && disk_.empty())
    return std::string();
  base::DictionaryValue dictionary;
  dictionary.SetString(kUptime, uptime_);
  dictionary.SetString(kDisk, disk_);

  std::string result;
  if (!base::JSONWriter::Write(dictionary, &result)) {
    LOG(WARNING) << "BootTimesRecorder::Stats::SerializeToString(): failed.";
    return std::string();
  }

  return result;
}

// static
BootTimesRecorder::Stats BootTimesRecorder::Stats::DeserializeFromString(
    const std::string& source) {
  if (source.empty())
    return Stats();

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(source);
  base::DictionaryValue* dictionary;
  if (!value || !value->GetAsDictionary(&dictionary)) {
    LOG(ERROR) << "BootTimesRecorder::Stats::DeserializeFromString(): not a "
                  "dictionary: '" << source << "'";
    return Stats();
  }

  Stats result;
  if (!dictionary->GetString(kUptime, &result.uptime_) ||
      !dictionary->GetString(kDisk, &result.disk_)) {
    LOG(ERROR)
        << "BootTimesRecorder::Stats::DeserializeFromString(): format error: '"
        << source << "'";
    return Stats();
  }

  return result;
}

bool BootTimesRecorder::Stats::UptimeDouble(double* result) const {
  std::string uptime = uptime_;
  const size_t space_at = uptime.find_first_of(' ');
  if (space_at == std::string::npos)
    return false;

  uptime.resize(space_at);

  if (base::StringToDouble(uptime, result))
    return true;

  return false;
}

void BootTimesRecorder::Stats::RecordStats(const std::string& name) const {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&BootTimesRecorder::Stats::RecordStatsAsync,
                     base::Owned(new Stats(*this)), name));
}

void BootTimesRecorder::Stats::RecordStatsWithCallback(
    const std::string& name,
    const base::Closure& callback) const {
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&BootTimesRecorder::Stats::RecordStatsAsync,
                 base::Owned(new Stats(*this)), name),
      callback);
}

void BootTimesRecorder::Stats::RecordStatsAsync(
    const base::FilePath::StringType& name) const {
  const base::FilePath log_path(kLogPath);
  const base::FilePath uptime_output =
      log_path.Append(base::FilePath(kUptimePrefix + name));
  const base::FilePath disk_output =
      log_path.Append(base::FilePath(kDiskPrefix + name));

  // Append numbers to the files.
  AppendFile(uptime_output, uptime_.data(), uptime_.size());
  AppendFile(disk_output, disk_.data(), disk_.size());
}

BootTimesRecorder::BootTimesRecorder()
    : have_registered_(false),
      login_done_(false),
      restart_requested_(false) {
  login_time_markers_.reserve(30);
  logout_time_markers_.reserve(30);
}

BootTimesRecorder::~BootTimesRecorder() {
}

// static
BootTimesRecorder* BootTimesRecorder::Get() {
  return g_boot_times_recorder.Pointer();
}

// static
void BootTimesRecorder::WriteTimes(const std::string base_name,
                                   const std::string uma_name,
                                   const std::string uma_prefix,
                                   std::vector<TimeMarker> login_times) {
  const int kMinTimeMillis = 1;
  const int kMaxTimeMillis = 30000;
  const int kNumBuckets = 100;
  const base::FilePath log_path(kLoginLogPath);

  // Need to sort by time since the entries may have been pushed onto the
  // vector (on the UI thread) in a different order from which they were
  // created (potentially on other threads).
  std::sort(login_times.begin(), login_times.end());

  base::Time first = login_times.front().time();
  base::Time last = login_times.back().time();
  base::TimeDelta total = last - first;
  base::HistogramBase* total_hist = base::Histogram::FactoryTimeGet(
      uma_name,
      base::TimeDelta::FromMilliseconds(kMinTimeMillis),
      base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
      kNumBuckets,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  total_hist->AddTime(total);
  std::string output =
      base::StringPrintf("%s: %.2f", uma_name.c_str(), total.InSecondsF());
  base::Time prev = first;
  for (unsigned int i = 0; i < login_times.size(); ++i) {
    TimeMarker tm = login_times[i];
    base::TimeDelta since_first = tm.time() - first;
    base::TimeDelta since_prev = tm.time() - prev;
    std::string name;

    if (tm.send_to_uma()) {
      name = uma_prefix + tm.name();
      base::HistogramBase* prev_hist = base::Histogram::FactoryTimeGet(
          name,
          base::TimeDelta::FromMilliseconds(kMinTimeMillis),
          base::TimeDelta::FromMilliseconds(kMaxTimeMillis),
          kNumBuckets,
          base::HistogramBase::kUmaTargetedHistogramFlag);
      prev_hist->AddTime(since_prev);
    } else {
      name = tm.name();
    }
    output +=
        base::StringPrintf(
            "\n%.2f +%.4f %s",
            since_first.InSecondsF(),
            since_prev.InSecondsF(),
            name.data());
    prev = tm.time();
  }
  output += '\n';

  base::WriteFile(log_path.Append(base_name), output.data(), output.size());
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
    registrar_.Remove(this,
                      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                      content::NotificationService::AllSources());
    registrar_.Remove(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        content::NotificationService::AllSources());
  }
  // Don't swamp the background thread right away.
  base::PostDelayedTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&WriteTimes, kLoginTimes,
                     (is_user_new ? kUmaLoginNewUser : kUmaLogin),
                     kUmaLoginPrefix, login_time_markers_),
      base::TimeDelta::FromMilliseconds(kLoginTimeWriteDelayMs));
}

void BootTimesRecorder::WriteLogoutTimes() {
  // Either we're on the browser thread, or (more likely) Chrome is in the
  // process of shutting down and we're on the main thread but the message loop
  // has already been terminated.
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));

  WriteTimes(kLogoutTimes,
             (restart_requested_ ? kUmaRestart : kUmaLogout),
             kUmaLogoutPrefix,
             logout_time_markers_);
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

  const Stats logout_started_last_stats =
      Stats::DeserializeFromString(logout_started_last_str);
  if (logout_started_last_stats.uptime().empty())
    return;

  double logout_started_last;
  double uptime;
  if (!logout_started_last_stats.UptimeDouble(&logout_started_last) ||
      !Stats::GetCurrentStats().UptimeDouble(&uptime)) {
    return;
  }

  if (logout_started_last >= uptime) {
    // Reboot happened.
    ClearLogoutStartedLastPreference();
    return;
  }

  // Write /tmp/uptime-logout-started as well.
  const char kLogoutStarted[] = "logout-started";
  logout_started_last_stats.RecordStatsWithCallback(
      kLogoutStarted,
      base::Bind(&BootTimesRecorder::ClearLogoutStartedLastPreference));
}

void BootTimesRecorder::OnLogoutStarted(PrefService* state) {
  const std::string uptime = Stats::GetCurrentStats().SerializeToString();
  if (!uptime.empty())
    state->SetString(prefs::kLogoutStartedLast, uptime);
}

void BootTimesRecorder::RecordCurrentStats(const std::string& name) {
  Stats::GetCurrentStats().RecordStats(name);
}

void BootTimesRecorder::SaveChromeMainStats() {
  chrome_main_stats_ = Stats::GetCurrentStats();
}

void BootTimesRecorder::RecordChromeMainStats() {
  chrome_main_stats_.RecordStats(kChromeMain);
}

void BootTimesRecorder::RecordLoginAttempted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (login_done_)
    return;

  login_time_markers_.clear();
  AddLoginTimeMarker("LoginStarted", false);
  if (!have_registered_) {
    have_registered_ = true;
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::NotificationService::AllSources());
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        content::NotificationService::AllSources());
  }
}

void BootTimesRecorder::AddLoginTimeMarker(const std::string& marker_name,
                                           bool send_to_uma) {
  AddMarker(&login_time_markers_, TimeMarker(marker_name, send_to_uma));
}

void BootTimesRecorder::AddLogoutTimeMarker(const std::string& marker_name,
                                            bool send_to_uma) {
  AddMarker(&logout_time_markers_, TimeMarker(marker_name, send_to_uma));
}

// static
void BootTimesRecorder::AddMarker(std::vector<TimeMarker>* vector,
                                  TimeMarker marker) {
  // The marker vectors can only be safely manipulated on the main thread.
  // If we're late in the process of shutting down (eg. as can be the case at
  // logout), then we have to assume we're on the main thread already.
  if (!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    vector->push_back(marker);
  } else {
    // Add the marker on the UI thread.
    // Note that it's safe to use an unretained pointer to the vector because
    // BootTimesRecorder's lifetime exceeds that of the UI thread message loop.
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&BootTimesRecorder::AddMarker,
                                  base::Unretained(vector), marker));
  }
}

void BootTimesRecorder::RecordAuthenticationSuccess() {
  AddLoginTimeMarker("Authenticate", true);
  RecordCurrentStats(kLoginSuccess);
}

void BootTimesRecorder::RecordAuthenticationFailure() {
  // Do nothing for now.
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
      AddLoginTimeMarker("TabLoad-Start: " + GetTabUrl(rwh), false);
      render_widget_hosts_loading_.insert(rwh);
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      NavigationController* tab =
          content::Source<NavigationController>(source).ptr();
      RenderWidgetHost* rwh = GetRenderWidgetHost(tab);
      if (render_widget_hosts_loading_.find(rwh) !=
          render_widget_hosts_loading_.end()) {
        AddLoginTimeMarker("TabLoad-End: " + GetTabUrl(rwh), false);
      }
      break;
    }
    case content::
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES: {
      RenderWidgetHost* rwh = content::Source<RenderWidgetHost>(source).ptr();
      if (render_widget_hosts_loading_.find(rwh) !=
          render_widget_hosts_loading_.end()) {
        AddLoginTimeMarker("TabPaint: " + GetTabUrl(rwh), false);
        LoginDone(user_manager::UserManager::Get()->IsCurrentUserNew());
      }
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      RenderWidgetHost* render_widget_host =
          GetRenderWidgetHost(&web_contents->GetController());
      render_widget_hosts_loading_.erase(render_widget_host);
      break;
    }
    default:
      break;
  }
}

}  // namespace chromeos
