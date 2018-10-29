// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"

#include <utility>

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/launcher.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/base/cocoa/focus_window_set.h"
#include "ui/base/ui_base_features.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;
using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

typedef AppWindowRegistry::AppWindowList AppWindowList;

void ProfileLoadedCallback(base::Callback<void(Profile*)> callback,
                           Profile* profile,
                           Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    callback.Run(profile);
    return;
  }

  // This should never get an error since it only loads existing profiles.
  DCHECK_EQ(Profile::CREATE_STATUS_CREATED, status);
}

void SetAppHidden(Profile* profile, const std::string& app_id, bool hidden) {
  AppWindowList windows =
      AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id);
  for (AppWindowList::const_reverse_iterator it = windows.rbegin();
       it != windows.rend();
       ++it) {
    if (hidden)
      (*it)->GetBaseWindow()->HideWithApp();
    else
      (*it)->GetBaseWindow()->ShowWithApp();
  }
}

bool FocusWindows(const AppWindowList& windows) {
  if (windows.empty())
    return false;

  std::set<gfx::NativeWindow> native_windows;
  for (AppWindowList::const_iterator it = windows.begin(); it != windows.end();
       ++it) {
    native_windows.insert((*it)->GetNativeWindow());
  }
  // Allow workspace switching. For the browser process, we can reasonably rely
  // on OS X to switch spaces for us and honor relevant user settings. But shims
  // don't have windows, so we have to do it ourselves.
  ui::FocusWindowSet(native_windows);
  return true;
}

bool FocusHostedAppWindows(const std::set<Browser*>& browsers) {
  if (browsers.empty())
    return false;

  // If the NSWindows for the app are in the app shim process, then don't steal
  // focus from the app shim.
  if (features::HostWindowsInAppShimProcess())
    return true;

  std::set<gfx::NativeWindow> native_windows;
  for (const Browser* browser : browsers)
    native_windows.insert(browser->window()->GetNativeWindow());

  ui::FocusWindowSet(native_windows);
  return true;
}

// Attempts to launch a packaged app, prompting the user to enable it if
// necessary. The prompt is shown in its own window.
// This class manages its own lifetime.
class EnableViaPrompt : public ExtensionEnableFlowDelegate {
 public:
  EnableViaPrompt(Profile* profile,
                  const std::string& extension_id,
                  const base::Callback<void()>& callback)
      : profile_(profile),
        extension_id_(extension_id),
        callback_(callback) {
  }

  ~EnableViaPrompt() override {}

  void Run() {
    flow_.reset(new ExtensionEnableFlow(profile_, extension_id_, this));
    flow_->Start();
  }

 private:
  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override {
    callback_.Run();
    delete this;
  }

  void ExtensionEnableFlowAborted(bool user_initiated) override {
    callback_.Run();
    delete this;
  }

  Profile* profile_;
  std::string extension_id_;
  base::Callback<void()> callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;

  DISALLOW_COPY_AND_ASSIGN(EnableViaPrompt);
};

}  // namespace

namespace apps {

bool ExtensionAppShimHandler::Delegate::ProfileExistsForPath(
    const base::FilePath& path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Check for the profile name in the profile info cache to ensure that we
  // never access any directory that isn't a known profile.
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  ProfileAttributesEntry* entry;
  return profile_manager->GetProfileAttributesStorage().
      GetProfileAttributesWithPath(full_path, &entry);
}

Profile* ExtensionAppShimHandler::Delegate::ProfileForPath(
    const base::FilePath& path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  Profile* profile = profile_manager->GetProfileByPath(full_path);

  // Use IsValidProfile to check if the profile has been created.
  return profile && profile_manager->IsValidProfile(profile) ? profile : NULL;
}

void ExtensionAppShimHandler::Delegate::LoadProfileAsync(
    const base::FilePath& path,
    base::Callback<void(Profile*)> callback) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  profile_manager->CreateProfileAsync(
      full_path,
      base::Bind(&ProfileLoadedCallback, callback),
      base::string16(), std::string());
}

bool ExtensionAppShimHandler::Delegate::IsProfileLockedForPath(
    const base::FilePath& path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath full_path = profile_manager->user_data_dir().Append(path);
  return profiles::IsProfileLocked(full_path);
}

AppWindowList ExtensionAppShimHandler::Delegate::GetWindows(
    Profile* profile,
    const std::string& extension_id) {
  return AppWindowRegistry::Get(profile)->GetAppWindowsForApp(extension_id);
}

const Extension* ExtensionAppShimHandler::Delegate::MaybeGetAppExtension(
    content::BrowserContext* context,
    const std::string& extension_id) {
  return ExtensionAppShimHandler::MaybeGetAppExtension(context, extension_id);
}

void ExtensionAppShimHandler::Delegate::EnableExtension(
    Profile* profile,
    const std::string& extension_id,
    const base::Callback<void()>& callback) {
  (new EnableViaPrompt(profile, extension_id, callback))->Run();
}

void ExtensionAppShimHandler::Delegate::LaunchApp(
    Profile* profile,
    const Extension* extension,
    const std::vector<base::FilePath>& files) {
  extensions::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_CMD_LINE_APP, extension->GetType());
  if (extension->is_hosted_app()) {
    OpenApplication(CreateAppLaunchParamsUserContainer(
        profile, extension, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        extensions::SOURCE_COMMAND_LINE));
    return;
  }
  if (files.empty()) {
    apps::LaunchPlatformApp(
        profile, extension, extensions::SOURCE_COMMAND_LINE);
  } else {
    for (std::vector<base::FilePath>::const_iterator it = files.begin();
         it != files.end(); ++it) {
      apps::LaunchPlatformAppWithPath(profile, extension, *it);
    }
  }
}

void ExtensionAppShimHandler::Delegate::LaunchShim(Profile* profile,
                                                   const Extension* extension) {
  web_app::MaybeLaunchShortcut(
      web_app::ShortcutInfoForExtensionAndProfile(extension, profile));
}

void ExtensionAppShimHandler::Delegate::LaunchUserManager() {
  UserManager::Show(base::FilePath(),
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

void ExtensionAppShimHandler::Delegate::MaybeTerminate() {
  AppShimHandler::MaybeTerminate();
}

ExtensionAppShimHandler::ExtensionAppShimHandler()
    : delegate_(new Delegate),
      weak_factory_(this) {
  // This is instantiated in BrowserProcessImpl::PreMainMessageLoopRun with
  // AppShimHostManager. Since PROFILE_CREATED is not fired until
  // ProfileManager::GetLastUsedProfile/GetLastOpenedProfiles, this should catch
  // notifications for all profiles.
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  BrowserList::AddObserver(this);
}

ExtensionAppShimHandler::~ExtensionAppShimHandler() {
  BrowserList::RemoveObserver(this);
}

AppShimHandler::Host* ExtensionAppShimHandler::FindHost(
    Profile* profile,
    const std::string& app_id) {
  HostMap::iterator it = hosts_.find(make_pair(profile, app_id));
  return it == hosts_.end() ? NULL : it->second;
}

AppShimHandler::Host* ExtensionAppShimHandler::FindHostForBrowser(
    Browser* browser) {
  const Extension* extension =
      apps::ExtensionAppShimHandler::MaybeGetAppForBrowser(browser);
  if (extension) {
    return FindHost(Profile::FromBrowserContext(browser->profile()),
                    extension->id());
  }
  return nullptr;
}

void ExtensionAppShimHandler::SetHostedAppHidden(Profile* profile,
                                                 const std::string& app_id,
                                                 bool hidden) {
  const AppBrowserMap::iterator it = app_browser_windows_.find(app_id);
  if (it == app_browser_windows_.end())
    return;

  for (const Browser* browser : it->second) {
    if (web_app::GetAppIdFromApplicationName(browser->app_name()) != app_id)
      continue;

    if (hidden)
      browser->window()->Hide();
    else
      browser->window()->Show();
  }
}

// static
const Extension* ExtensionAppShimHandler::MaybeGetAppExtension(
    content::BrowserContext* context,
    const std::string& extension_id) {
  if (!context)
    return NULL;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  return extension &&
                 (extension->is_platform_app() || extension->is_hosted_app())
             ? extension
             : NULL;
}

// static
const Extension* ExtensionAppShimHandler::MaybeGetAppForBrowser(
    Browser* browser) {
  if (!browser || !browser->is_app())
    return NULL;

  return MaybeGetAppExtension(
      browser->profile(),
      web_app::GetAppIdFromApplicationName(browser->app_name()));
}

void ExtensionAppShimHandler::QuitAppForWindow(AppWindow* app_window) {
  Host* host =
      FindHost(Profile::FromBrowserContext(app_window->browser_context()),
               app_window->extension_id());
  if (host) {
    OnShimQuit(host);
  } else {
    // App shims might be disabled or the shim is still starting up.
    AppWindowRegistry::Get(
        Profile::FromBrowserContext(app_window->browser_context()))
        ->CloseAllAppWindowsForApp(app_window->extension_id());
  }
}

void ExtensionAppShimHandler::QuitHostedAppForWindow(
    Profile* profile,
    const std::string& app_id) {
  Host* host = FindHost(Profile::FromBrowserContext(profile), app_id);
  if (host)
    OnShimQuit(host);
  else
    CloseBrowsersForApp(app_id);
}

void ExtensionAppShimHandler::HideAppForWindow(AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  Host* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppHide();
  else
    SetAppHidden(profile, app_window->extension_id(), true);
}

void ExtensionAppShimHandler::HideHostedApp(Profile* profile,
                                            const std::string& app_id) {
  Host* host = FindHost(profile, app_id);
  if (host)
    host->OnAppHide();
  else
    SetHostedAppHidden(profile, app_id, true);
}

void ExtensionAppShimHandler::FocusAppForWindow(AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  const std::string& app_id = app_window->extension_id();
  Host* host = FindHost(profile, app_id);
  if (host) {
    OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, std::vector<base::FilePath>());
  } else {
    FocusWindows(AppWindowRegistry::Get(profile)->GetAppWindowsForApp(app_id));
  }
}

void ExtensionAppShimHandler::UnhideWithoutActivationForWindow(
    AppWindow* app_window) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  Host* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppUnhideWithoutActivation();
}

void ExtensionAppShimHandler::RequestUserAttentionForWindow(
    AppWindow* app_window,
    AppShimAttentionType attention_type) {
  Profile* profile = Profile::FromBrowserContext(app_window->browser_context());
  Host* host = FindHost(profile, app_window->extension_id());
  if (host)
    host->OnAppRequestUserAttention(attention_type);
}

void ExtensionAppShimHandler::OnChromeWillHide() {
  // Send OnAppHide to all the shims so that they go into the hidden state.
  // This is necessary so that when the shim is next focused, it will know to
  // unhide.
  for (auto& entry : hosts_)
    entry.second->OnAppHide();
}

void ExtensionAppShimHandler::OnShimLaunch(
    Host* host,
    AppShimLaunchType launch_type,
    const std::vector<base::FilePath>& files) {
  const std::string& app_id = host->GetAppId();
  DCHECK(crx_file::id_util::IdIsValid(app_id));

  const base::FilePath& profile_path = host->GetProfilePath();
  DCHECK(!profile_path.empty());

  if (!delegate_->ProfileExistsForPath(profile_path)) {
    // User may have deleted the profile this shim was originally created for.
    // TODO(jackhou): Add some UI for this case and remove the LOG.
    LOG(ERROR) << "Requested directory is not a known profile '"
               << profile_path.value() << "'.";
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND);
    return;
  }

  if (delegate_->IsProfileLockedForPath(profile_path)) {
    LOG(WARNING) << "Requested profile is locked.  Showing User Manager.";
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_PROFILE_LOCKED);
    delegate_->LaunchUserManager();
    return;
  }

  Profile* profile = delegate_->ProfileForPath(profile_path);

  if (profile) {
    OnProfileLoaded(host, launch_type, files, profile);
    return;
  }

  // If the profile is not loaded, this must have been a launch by the shim.
  // Load the profile asynchronously, the host will be registered in
  // OnProfileLoaded.
  DCHECK_EQ(APP_SHIM_LAUNCH_NORMAL, launch_type);
  delegate_->LoadProfileAsync(
      profile_path,
      base::Bind(&ExtensionAppShimHandler::OnProfileLoaded,
                 weak_factory_.GetWeakPtr(),
                 host, launch_type, files));

  // Return now. OnAppLaunchComplete will be called when the app is activated.
}

// static
ExtensionAppShimHandler* ExtensionAppShimHandler::Get() {
  // This will only return nullptr in certain unit tests that do not initialize
  // the app shim host manager.
  auto* shim_host_manager =
      g_browser_process->platform_part()->app_shim_host_manager();
  if (shim_host_manager)
    return shim_host_manager->extension_app_shim_handler();
  return nullptr;
}

const Extension* ExtensionAppShimHandler::MaybeGetExtensionOrCloseHost(
    Host* host,
    Profile** profile_out) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const Extension* extension =
      delegate_->MaybeGetAppExtension(profile, host->GetAppId());
  if (!extension) {
    // Extensions may have been uninstalled or disabled since the shim
    // started.
    host->OnAppClosed();
  }

  if (profile_out)
    *profile_out = profile;
  return extension;
}

void ExtensionAppShimHandler::CloseBrowsersForApp(const std::string& app_id) {
  AppBrowserMap::iterator it = app_browser_windows_.find(app_id);
  if (it == app_browser_windows_.end())
    return;

  for (const Browser* browser : it->second)
    browser->window()->Close();
}

void ExtensionAppShimHandler::OnProfileLoaded(
    Host* host,
    AppShimLaunchType launch_type,
    const std::vector<base::FilePath>& files,
    Profile* profile) {
  const std::string& app_id = host->GetAppId();

  // The first host to claim this (profile, app_id) becomes the main host.
  // For any others, focus or relaunch the app.
  if (!hosts_.insert(make_pair(make_pair(profile, app_id), host)).second) {
    OnShimFocus(host,
                launch_type == APP_SHIM_LAUNCH_NORMAL ?
                    APP_SHIM_FOCUS_REOPEN : APP_SHIM_FOCUS_NORMAL,
                files);
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_DUPLICATE_HOST);
    return;
  }

  if (launch_type != APP_SHIM_LAUNCH_NORMAL) {
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_SUCCESS);
    return;
  }

  // TODO(jeremya): Handle the case that launching the app fails. Probably we
  // need to watch for 'app successfully launched' or at least 'background page
  // exists/was created' and time out with failure if we don't see that sign of
  // life within a certain window.
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (extension) {
    delegate_->LaunchApp(profile, extension, files);
    // If it's a hosted app that opens in a tab, let the shim terminate
    // immediately.
    if (extension->is_hosted_app() &&
        extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile),
                                  extension) ==
            extensions::LAUNCH_TYPE_REGULAR) {
      host->OnAppLaunchComplete(APP_SHIM_LAUNCH_DUPLICATE_HOST);
    }
    return;
  }

  delegate_->EnableExtension(
      profile, app_id,
      base::Bind(&ExtensionAppShimHandler::OnExtensionEnabled,
                 weak_factory_.GetWeakPtr(),
                 host->GetProfilePath(), app_id, files));
}

void ExtensionAppShimHandler::OnExtensionEnabled(
    const base::FilePath& profile_path,
    const std::string& app_id,
    const std::vector<base::FilePath>& files) {
  Profile* profile = delegate_->ProfileForPath(profile_path);
  if (!profile)
    return;

  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (!extension || !delegate_->ProfileExistsForPath(profile_path)) {
    // If !extension, the extension doesn't exist, or was not re-enabled.
    // If the profile doesn't exist, it may have been deleted during the enable
    // prompt. In this case, NOTIFICATION_PROFILE_DESTROYED may not be fired
    // until later, so respond to the host now.
    Host* host = FindHost(profile, app_id);
    if (host)
      host->OnAppLaunchComplete(APP_SHIM_LAUNCH_APP_NOT_FOUND);
    return;
  }

  delegate_->LaunchApp(profile, extension, files);
}


void ExtensionAppShimHandler::OnShimClose(Host* host) {
  // This might be called when shutting down. Don't try to look up the profile
  // since profile_manager might not be around.
  for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
    HostMap::iterator current = it++;
    if (current->second == host)
      hosts_.erase(current);
  }
}

void ExtensionAppShimHandler::OnShimFocus(
    Host* host,
    AppShimFocusType focus_type,
    const std::vector<base::FilePath>& files) {
  Profile* profile;
  const Extension* extension = MaybeGetExtensionOrCloseHost(host, &profile);
  if (!extension)
    return;

  bool windows_focused;
  const std::string& app_id = host->GetAppId();
  if (extension->is_hosted_app()) {
    AppBrowserMap::iterator it = app_browser_windows_.find(app_id);
    if (it == app_browser_windows_.end())
      return;

    windows_focused = FocusHostedAppWindows(it->second);
  } else {
    const AppWindowList windows =
        delegate_->GetWindows(profile, host->GetAppId());
    windows_focused = FocusWindows(windows);
  }

  if (focus_type == APP_SHIM_FOCUS_NORMAL ||
      (focus_type == APP_SHIM_FOCUS_REOPEN && windows_focused)) {
    return;
  }

  delegate_->LaunchApp(profile, extension, files);
}

void ExtensionAppShimHandler::OnShimSetHidden(Host* host, bool hidden) {
  Profile* profile;
  const Extension* extension = MaybeGetExtensionOrCloseHost(host, &profile);
  if (!extension)
    return;

  if (extension->is_hosted_app())
    SetHostedAppHidden(profile, host->GetAppId(), hidden);
  else
    SetAppHidden(profile, host->GetAppId(), hidden);
}

void ExtensionAppShimHandler::OnShimQuit(Host* host) {
  DCHECK(delegate_->ProfileExistsForPath(host->GetProfilePath()));
  Profile* profile = delegate_->ProfileForPath(host->GetProfilePath());

  const std::string& app_id = host->GetAppId();
  const Extension* extension = delegate_->MaybeGetAppExtension(profile, app_id);
  if (!extension)
    return;

  if (extension->is_hosted_app()) {
    CloseBrowsersForApp(app_id);
  } else {
    const AppWindowList windows = delegate_->GetWindows(profile, app_id);
    for (AppWindowRegistry::const_iterator it = windows.begin();
         it != windows.end(); ++it) {
      (*it)->GetBaseWindow()->Close();
    }
  }
  // Once the last window closes, flow will end up in OnAppDeactivated via
  // AppLifetimeMonitor.
  // Otherwise, once the last window closes for a hosted app, OnBrowserRemoved
  // will call OnAppDeactivated.
}

void ExtensionAppShimHandler::set_delegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void ExtensionAppShimHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->IsOffTheRecord())
        return;

      AppLifetimeMonitorFactory::GetForBrowserContext(profile)->AddObserver(
          this);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile->IsOffTheRecord())
        return;

      AppLifetimeMonitorFactory::GetForBrowserContext(profile)->RemoveObserver(
          this);
      // Shut down every shim associated with this profile.
      for (HostMap::iterator it = hosts_.begin(); it != hosts_.end(); ) {
        // Increment the iterator first as OnAppClosed may call back to
        // OnShimClose and invalidate the iterator.
        HostMap::iterator current = it++;
        if (profile->IsSameProfile(current->first.first)) {
          Host* host = current->second;
          host->OnAppClosed();
        }
      }
      break;
    }
    case chrome::NOTIFICATION_BROWSER_OPENED: {
      Browser* browser = content::Source<Browser>(source).ptr();
      // Don't keep track of browsers that are not associated with an app.
      const Extension* extension = MaybeGetAppForBrowser(browser);
      if (!extension)
        return;

      BrowserSet& browsers = app_browser_windows_[extension->id()];
      browsers.insert(browser);
      if (browsers.size() == 1)
        OnAppActivated(browser->profile(), extension->id());

      break;
    }
    default: {
      NOTREACHED();  // Unexpected notification.
      break;
    }
  }
}

void ExtensionAppShimHandler::OnAppStart(content::BrowserContext* context,
                                         const std::string& app_id) {}

void ExtensionAppShimHandler::OnAppActivated(content::BrowserContext* context,
                                             const std::string& app_id) {
  const Extension* extension = delegate_->MaybeGetAppExtension(context, app_id);
  if (!extension)
    return;

  Profile* profile = static_cast<Profile*>(context);
  Host* host = FindHost(profile, app_id);
  if (host) {
    host->OnAppLaunchComplete(APP_SHIM_LAUNCH_SUCCESS);
    OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, std::vector<base::FilePath>());
    return;
  }

  delegate_->LaunchShim(profile, extension);
}

void ExtensionAppShimHandler::OnAppDeactivated(content::BrowserContext* context,
                                               const std::string& app_id) {
  Host* host = FindHost(static_cast<Profile*>(context), app_id);
  if (host)
    host->OnAppClosed();

  if (hosts_.empty())
    delegate_->MaybeTerminate();
}

void ExtensionAppShimHandler::OnAppStop(content::BrowserContext* context,
                                        const std::string& app_id) {}

// The BrowserWindow may be NULL when this is called.
// Therefore we listen for the notification
// chrome::NOTIFICATION_BROWSER_OPENED and then call OnAppActivated.
// If this notification is removed, check that OnBrowserAdded is called after
// the BrowserWindow is ready.
void ExtensionAppShimHandler::OnBrowserAdded(Browser* browser) {}

void ExtensionAppShimHandler::OnBrowserRemoved(Browser* browser) {
  const Extension* extension = MaybeGetAppForBrowser(browser);
  if (!extension)
    return;

  AppBrowserMap::iterator it = app_browser_windows_.find(extension->id());
  if (it != app_browser_windows_.end()) {
    BrowserSet& browsers = it->second;
    browsers.erase(browser);
    if (browsers.empty())
      OnAppDeactivated(browser->profile(), extension->id());
  }
}

}  // namespace apps
