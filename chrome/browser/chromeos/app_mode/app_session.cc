// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session.h"

#include <errno.h>
#include <signal.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/chromeos/app_mode/kiosk_mode_idle_app_name_notification.h"
#include "chrome/browser/chromeos/app_mode/kiosk_session_plugin_handler.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
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

using extensions::AppWindow;
using extensions::AppWindowRegistry;

namespace chromeos {

namespace {

bool IsPepperPlugin(const base::FilePath& plugin_path) {
  content::WebPluginInfo plugin_info;
  return content::PluginService::GetInstance()->GetPluginInfoByPath(
             plugin_path, &plugin_info) &&
         plugin_info.is_pepper_plugin();
}

void RebootDevice() {
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER, "kiosk app session");
}

// Sends a SIGFPE signal to plugin subprocesses that matches |child_ids|
// to trigger a dump.
void DumpPluginProcessOnIOThread(const std::set<int>& child_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

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
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI}, base::BindOnce(&RebootDevice),
      base::TimeDelta::FromSeconds(dump_requested ? kDumpWaitSeconds : 0));
}

}  // namespace

class AppSession::AppWindowHandler : public AppWindowRegistry::Observer {
 public:
  explicit AppWindowHandler(AppSession* app_session)
      : app_session_(app_session) {}
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

    if (DemoAppLauncher::IsDemoAppSession(user_manager::UserManager::Get()
                                              ->GetActiveUser()
                                              ->GetAccountId())) {
      // If we were in demo mode, we disabled all our network technologies,
      // re-enable them.
      NetworkHandler::Get()->network_state_handler()->SetTechnologyEnabled(
          NetworkTypePattern::Physical(), true,
          chromeos::network_handler::ErrorCallback());
    }

    app_session_->OnLastAppWindowClosed();
    window_registry_->RemoveObserver(this);
  }

  AppSession* const app_session_;
  AppWindowRegistry* window_registry_ = nullptr;
  std::string app_id_;
  bool app_window_created_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppWindowHandler);
};

class AppSession::BrowserWindowHandler : public BrowserListObserver {
 public:
  BrowserWindowHandler() {
    BrowserList::AddObserver(this);
  }
  ~BrowserWindowHandler() override { BrowserList::RemoveObserver(this); }

 private:
  void HandleBrowser(Browser* browser) {
    content::WebContents* active_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    std::string url_string =
        active_tab ? active_tab->GetURL().spec() : std::string();
    LOG(WARNING) << "Browser opened in kiosk session"
                 << ", url=" << url_string;

    browser->window()->Close();
  }

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserWindowHandler::HandleBrowser,
                       base::Unretained(this),  // LazyInstance, always valid
                       browser));
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowHandler);
};

AppSession::AppSession() {}
AppSession::~AppSession() {}

void AppSession::Init(Profile* profile, const std::string& app_id) {
  app_window_handler_.reset(new AppWindowHandler(this));
  app_window_handler_->Init(profile, app_id);

  browser_window_handler_.reset(new BrowserWindowHandler);

  plugin_handler_.reset(new KioskSessionPluginHandler(this));

  // For a demo app, we don't need to either setup the update service or
  // the idle app name notification.
  if (DemoAppLauncher::IsDemoAppSession(
          user_manager::UserManager::Get()->GetActiveUser()->GetAccountId()))
    return;

  // Set the app_id for the current instance of KioskAppUpdateService.
  KioskAppUpdateService* update_service =
      KioskAppUpdateServiceFactory::GetForProfile(profile);
  DCHECK(update_service);
  if (update_service)
    update_service->Init(app_id);

  // Start to monitor external update from usb stick.
  KioskAppManager::Get()->MonitorKioskExternalUpdate();

  // If the device is not enterprise managed, set prefs to reboot after update
  // and create a user security message which shows the user the application
  // name and author after some idle timeout.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kRebootAfterUpdate, true);
    KioskModeIdleAppNameNotification::Initialize();
  }
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

  chrome::AttemptUserExit();
}

bool AppSession::ShouldHandlePlugin(const base::FilePath& plugin_path) const {
  // Note that BrowserChildProcessHostIterator in DumpPluginProcessOnIOThread
  // also needs to be updated when adding more plugin types here.
  return IsPepperPlugin(plugin_path);
}

void AppSession::OnPluginCrashed(const base::FilePath& plugin_path) {
  if (is_shutting_down_)
    return;
  is_shutting_down_ = true;

  LOG(ERROR) << "Reboot due to plugin crash, path=" << plugin_path.value();
  RebootDevice();
}

void AppSession::OnPluginHung(const std::set<int>& hung_plugins) {
  if (is_shutting_down_)
    return;
  is_shutting_down_ = true;

  LOG(ERROR) << "Plugin hung detected. Dump and reboot.";
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&DumpPluginProcessOnIOThread, hung_plugins));
}

}  // namespace chromeos
