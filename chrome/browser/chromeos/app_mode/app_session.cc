// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session.h"

#include <errno.h>
#include <signal.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/app_session_browser_window_handler.h"
#include "chrome/browser/chromeos/app_mode/app_session_metrics_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/power_manager_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler_delegate.h"
#include "content/public/browser/plugin_service.h"
#endif

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace chromeos {

namespace {

#if BUILDFLAG(ENABLE_PLUGINS)
bool IsPepperPlugin(const base::FilePath& plugin_path) {
  content::WebPluginInfo plugin_info;
  return content::PluginService::GetInstance()->GetPluginInfoByPath(
             plugin_path, &plugin_info) &&
         plugin_info.is_pepper_plugin();
}
#endif

void RebootDevice() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "kiosk app session");
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  KioskSessionServiceLacros::Get()->RestartDevice("kiosk app session");
#endif
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

  const raw_ptr<AppSession> app_session_;
  raw_ptr<AppWindowRegistry> window_registry_ = nullptr;
  std::string app_id_;
  bool app_window_created_ = false;
};

#if BUILDFLAG(ENABLE_PLUGINS)
class AppSession::PluginHandlerDelegateImpl
    : public KioskSessionPluginHandlerDelegate {
 public:
  explicit PluginHandlerDelegateImpl(AppSession* owner) : owner_(owner) {}
  PluginHandlerDelegateImpl(const PluginHandlerDelegateImpl&) = delete;
  PluginHandlerDelegateImpl& operator=(const PluginHandlerDelegateImpl&) =
      delete;
  ~PluginHandlerDelegateImpl() override = default;

  // KioskSessionPluginHandlerDelegate:
  bool ShouldHandlePlugin(const base::FilePath& plugin_path) const override {
    // Note that BrowserChildProcessHostIterator in DumpPluginProcess also needs
    // to be updated when adding more plugin types here.
    return IsPepperPlugin(plugin_path);
  }
  void OnPluginCrashed(const base::FilePath& plugin_path) override {
    if (owner_->is_shutting_down_)
      return;
    owner_->metrics_service_->RecordKioskSessionPluginCrashed();
    owner_->is_shutting_down_ = true;

    LOG(ERROR) << "Reboot due to plugin crash, path=" << plugin_path.value();
    RebootDevice();
  }

  void OnPluginHung(const std::set<int>& hung_plugins) override {
    if (owner_->is_shutting_down_)
      return;
    owner_->metrics_service_->RecordKioskSessionPluginHung();
    owner_->is_shutting_down_ = true;

    LOG(ERROR) << "Plugin hung detected. Dump and reboot.";
    DumpPluginProcess(hung_plugins);
  }

 private:
  AppSession* const owner_;
};
#endif

AppSession::AppSession()
    :
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_handler_delegate_(
          std::make_unique<PluginHandlerDelegateImpl>(this)),
#endif
      attempt_user_exit_(base::BindOnce(chrome::AttemptUserExit)),
      metrics_service_(std::make_unique<AppSessionMetricsService>(
          g_browser_process->local_state())) {
}

AppSession::AppSession(base::OnceClosure attempt_user_exit,
                       PrefService* local_state)
    :
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_handler_delegate_(
          std::make_unique<PluginHandlerDelegateImpl>(this)),
#endif
      attempt_user_exit_(std::move(attempt_user_exit)),
      metrics_service_(
          std::make_unique<AppSessionMetricsService>(local_state)) {
}

AppSession::~AppSession() {
  if (!is_shutting_down_)
    metrics_service_->RecordKioskSessionStopped();
}

void AppSession::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kKioskMetrics);
}

void AppSession::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kNewWindowsInKioskAllowed, false);
}

void AppSession::Init(Profile* profile, const std::string& app_id) {
  SetProfile(profile);
  app_window_handler_ = std::make_unique<AppWindowHandler>(this);
  app_window_handler_->Init(profile, app_id);
  CreateBrowserWindowHandler(nullptr);
#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_handler_ = std::make_unique<KioskSessionPluginHandler>(
      plugin_handler_delegate_.get());
#endif
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

KioskSessionPluginHandlerDelegate*
AppSession::GetPluginHandlerDelegateForTesting() {
  return plugin_handler_delegate_.get();
}

void AppSession::SetProfile(Profile* profile) {
  profile_ = profile;
}

void AppSession::CreateBrowserWindowHandler(Browser* browser) {
  browser_window_handler_ = std::make_unique<AppSessionBrowserWindowHandler>(
      profile_, browser,
      base::BindRepeating(&AppSession::OnHandledNewBrowserWindow,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&AppSession::OnLastAppWindowClosed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void AppSession::OnHandledNewBrowserWindow() {
  if (on_handle_browser_callback_)
    on_handle_browser_callback_.Run();
}

void AppSession::OnAppWindowAdded(AppWindow* app_window) {
  if (is_shutting_down_)
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_handler_->Observe(app_window->web_contents());
#endif
}

void AppSession::OnGuestAdded(content::WebContents* guest_web_contents) {
  // Bail if the session is shutting down.
  if (is_shutting_down_)
    return;

  // Bail if the guest is not a WebViewGuest.
  if (!extensions::WebViewGuest::FromWebContents(guest_web_contents))
    return;

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_handler_->Observe(guest_web_contents);
#endif
}

void AppSession::OnLastAppWindowClosed() {
  if (is_shutting_down_)
    return;
  is_shutting_down_ = true;
  metrics_service_->RecordKioskSessionStopped();

  std::move(attempt_user_exit_).Run();
}

Browser* AppSession::GetSettingsBrowserForTesting() {
  if (browser_window_handler_) {
    return browser_window_handler_->GetSettingsBrowserForTesting();  // IN-TEST
  }
  return nullptr;
}

}  // namespace chromeos
