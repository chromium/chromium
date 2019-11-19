// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_process_manager_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/one_shot_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace extensions {

ChromeProcessManagerDelegate::ChromeProcessManagerDelegate() {
  BrowserList::AddObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

ChromeProcessManagerDelegate::~ChromeProcessManagerDelegate() {
  BrowserList::RemoveObserver(this);
}

bool ChromeProcessManagerDelegate::AreBackgroundPagesAllowedForContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  bool is_normal_session = !profile->IsGuestSession() &&
                           !profile->IsSystemProfile();

  // Disallow if the current session is a Guest mode session or login screen but
  // the current browser context is *not* off-the-record. Such context is
  // artificial and background page shouldn't be created in it.
  // http://crbug.com/329498
  return is_normal_session || profile->IsOffTheRecord();
}

bool ChromeProcessManagerDelegate::IsExtensionBackgroundPageAllowed(
    content::BrowserContext* context,
    const Extension& extension) const {
#if defined(OS_CHROMEOS)
  Profile* profile = Profile::FromBrowserContext(context);

  const bool is_signin_profile =
      chromeos::ProfileHelper::IsSigninProfile(profile) &&
      !profile->IsOffTheRecord();

  if (is_signin_profile) {
    // Check for flag.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kDisableLoginScreenApps)) {
      return false;
    }

    // Get login screen apps installed by policy.
    std::unique_ptr<base::DictionaryValue> login_screen_apps_list =
        ExtensionManagementFactory::GetForBrowserContext(context)
            ->GetForceInstallList();

    // For the ChromeOS login profile, only allow apps installed by device
    // policy or that are explicitly whitelisted.
    return login_screen_apps_list->HasKey(extension.id()) ||
           IsComponentExtensionWhitelistedForSignInProfile(extension.id());
  }

  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile) &&
      !profile->IsOffTheRecord()) {
    return extension.permissions_data()->HasAPIPermission(
        APIPermission::kLockScreen);
  }
#endif

  return AreBackgroundPagesAllowedForContext(context);
}

bool ChromeProcessManagerDelegate::DeferCreatingStartupBackgroundHosts(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // The profile may not be valid yet if it is still being initialized.
  // In that case, defer loading, since it depends on an initialized profile.
  // Background hosts will be loaded later via NOTIFICATION_PROFILE_CREATED.
  // http://crbug.com/222473
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return true;

  // There are no browser windows open and the browser process was
  // started to show the app launcher. Background hosts will be loaded later
  // via OnBrowserAdded(). http://crbug.com/178260
  return chrome::GetBrowserCount(profile) == 0 &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kShowAppList);
}

void ChromeProcessManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      OnProfileCreated(profile);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      OnProfileDestroyed(profile);
      break;
    }
    default:
      NOTREACHED();
  }
}

void ChromeProcessManagerDelegate::OnBrowserAdded(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);

  // If the extension system isn't ready yet the background hosts will be
  // created automatically when it is.
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system->ready().is_signaled())
    return;

  // Inform the process manager for this profile that the window is ready.
  // We continue to observe the notification in case browser windows open for
  // a related incognito profile or other regular profiles.
  ProcessManager* manager = ProcessManager::Get(profile);
  DCHECK(manager);
  DCHECK_EQ(profile, manager->browser_context());
  manager->MaybeCreateStartupBackgroundHosts();

  // For incognito profiles also inform the original profile's process manager
  // that the window is ready. This will usually be a no-op because the
  // original profile's process manager should have been informed when the
  // non-incognito window opened.
  if (profile->IsOffTheRecord()) {
    Profile* original_profile = profile->GetOriginalProfile();
    ProcessManager* original_manager = ProcessManager::Get(original_profile);
    DCHECK(original_manager);
    DCHECK_EQ(original_profile, original_manager->browser_context());
    original_manager->MaybeCreateStartupBackgroundHosts();
  }
}

void ChromeProcessManagerDelegate::OnProfileCreated(Profile* profile) {
  // Incognito profiles are handled by their original profile.
  if (profile->IsOffTheRecord())
    return;

  // The profile can be created before the extension system is ready.
  if (!ExtensionSystem::Get(profile)->ready().is_signaled())
    return;

  // The profile might have been initialized asynchronously (in parallel with
  // extension system startup). Now that initialization is complete the
  // ProcessManager can load deferred background pages.
  ProcessManager::Get(profile)->MaybeCreateStartupBackgroundHosts();
}

void ChromeProcessManagerDelegate::OnProfileDestroyed(Profile* profile) {
  // Close background hosts when the last profile is closed so that they
  // have time to shutdown various objects on different threads. The
  // ProfileManager destructor is called too late in the shutdown sequence.
  // http://crbug.com/15708
  ProcessManager* manager =
      ProcessManagerFactory::GetForBrowserContextIfExists(profile);
  if (manager) {
    manager->CloseBackgroundHosts();
  }

  // If this profile owns an incognito profile, but it is destroyed before the
  // incognito profile is destroyed, then close the incognito background hosts
  // as well. This happens in a few tests. http://crbug.com/138843
  if (!profile->IsOffTheRecord() && profile->HasOffTheRecordProfile()) {
    ProcessManager* incognito_manager =
        ProcessManagerFactory::GetForBrowserContextIfExists(
            profile->GetOffTheRecordProfile());
    if (incognito_manager) {
      incognito_manager->CloseBackgroundHosts();
    }
  }
}

}  // namespace extensions
