// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_process_manager_delegate.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace extensions {

ChromeProcessManagerDelegate::ChromeProcessManagerDelegate() {
  BrowserList::AddObserver(this);
  DCHECK(g_browser_process);
  // The profile manager can be null in unit tests.
  if (g_browser_process->profile_manager()) {
    g_browser_process->profile_manager()->AddObserver(this);
    // All profiles must be observed, so make sure none have been created that
    // we missed.
    DCHECK_EQ(0U,
              g_browser_process->profile_manager()->GetLoadedProfiles().size());
  }
}

ChromeProcessManagerDelegate::~ChromeProcessManagerDelegate() {
  // |this| is owned by the BrowserProcess and outlives the ProfileManager, so
  // we don't call RemoveObserver. The |g_browser_process| pointer is already
  // set to null so we can't directly verify that the profile manager is already
  // destroyed.
  DCHECK(!g_browser_process)
      << "ChromeProcessManagerDelegate expects to be shut down during "
         "BrowserProcess shutdown, after |g_browser_process| is set to null";
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
    // policy or that are explicitly allowlisted.
    return login_screen_apps_list->HasKey(extension.id()) ||
           IsComponentExtensionAllowlistedForSignInProfile(extension.id());
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
  // Background hosts will be loaded later via OnProfileAdded.
  // http://crbug.com/222473
  // Unit tests may not have a profile manager.
  return (g_browser_process->profile_manager() &&
          !g_browser_process->profile_manager()->IsValidProfile(profile));
}

void ChromeProcessManagerDelegate::OnBrowserAdded(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);

  // Inform the process manager for this profile that the window is ready.
  // We continue to observe the notification in case browser windows open for
  // a related incognito profile or other regular profiles.
  ProcessManager::Get(profile)->MaybeCreateStartupBackgroundHosts();

  // For incognito profiles also inform the original profile's process manager
  // that the window is ready. This will usually be a no-op because the
  // original profile's process manager should have been informed when the
  // non-incognito window opened.
  if (profile->IsOffTheRecord()) {
    ProcessManager::Get(profile->GetOriginalProfile())
        ->MaybeCreateStartupBackgroundHosts();
  }
}

void ChromeProcessManagerDelegate::OnProfileAdded(Profile* profile) {
  observed_profiles_.Add(profile);

  // The profile might have been initialized asynchronously (in parallel with
  // extension system startup). Now that initialization is complete the
  // ProcessManager can load deferred background pages.
  ProcessManager::Get(profile)->MaybeCreateStartupBackgroundHosts();
}

void ChromeProcessManagerDelegate::OnOffTheRecordProfileCreated(
    Profile* off_the_record_profile) {
  observed_profiles_.Add(off_the_record_profile);
}

void ChromeProcessManagerDelegate::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.Remove(profile);

  // Close background hosts when the last profile is closed so that they
  // have time to shutdown various objects on different threads. The
  // KeyedService::Shutdown override is called too late in the shutdown
  // sequence. http://crbug.com/15708
  auto close_background_hosts = [](Profile* profile) {
    ProcessManager* manager =
        ProcessManagerFactory::GetForBrowserContextIfExists(profile);
    if (manager)
      manager->CloseBackgroundHosts();
  };

  close_background_hosts(profile);

  // If this profile owns an incognito profile, but it is destroyed before the
  // incognito profile is destroyed, then close the incognito background hosts
  // as well. This happens in a few tests. http://crbug.com/138843
  if (!profile->IsOffTheRecord() && profile->HasPrimaryOTRProfile()) {
    Profile* otr = profile->GetPrimaryOTRProfile();
    close_background_hosts(otr);
    if (observed_profiles_.IsObserving(otr))
      observed_profiles_.Remove(otr);
  }
}

}  // namespace extensions
