// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_launcher.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_switches.h"

namespace {

// User data key for BrowserLauncher.
const void* const kBrowserLauncherKey = &kBrowserLauncherKey;

}  // namespace

BrowserLauncher::BrowserLauncher(Profile* profile) : profile_(profile) {}

// static.
BrowserLauncher* BrowserLauncher::GetForProfile(Profile* profile) {
  if (!profile)
    return nullptr;

  if (!profile->GetUserData(kBrowserLauncherKey)) {
    profile->SetUserData(kBrowserLauncherKey,
                         std::make_unique<BrowserLauncher>(profile));
  }
  return static_cast<BrowserLauncher*>(
      profile->GetUserData(kBrowserLauncherKey));
}

bool BrowserLauncher::LaunchForLastOpenedProfiles(bool skip_crash_restore,
                                                  bool restore_tabbed_browser) {
  DCHECK(!is_launching_for_last_opened_profiles_);

  // Launching for last opened profiles can only complete successfully if
  // ProfileManager has not yet updated the list of last opened profiles for the
  // current session.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager->has_updated_last_opened_profiles()) {
    LOG(ERROR) << "Cannot launch for last opened profiles because last opened "
                  "profiles have been updated for this session.";
    return false;
  }

  base::AutoReset<bool> resetter(&is_launching_for_last_opened_profiles_, true);

  // Ensure that we do not start with the profile picker.
  StartupProfileInfo profile_info{profile_.get(),
                                  StartupProfileMode::kBrowserWindow};

  // Get the last opened profiles from the last session. This is only valid
  // before any browsers have been opened for the current session as opening /
  // closing browsers will cause the last opened profiles to change.
  auto last_opened_profiles = profile_manager->GetLastOpenedProfiles();

  // Currently the kNoStartupWindow flag is set when lacros-chrome is launched
  // with crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow. The intention
  // is to prevent lacros-chrome from launching a window during startup. However
  // this flag remains set throughout the life of the browser process.
  // This leads to issues where browsers can no longer be opened by the startup
  // browser creator (such as below and in SessionService::RestoreIfNecessary).
  // As a temporary workaround remove the kNoStartupWindow switch from the
  // command line when launching for full restore. This is safe as by this point
  // the browser process has already been started in its windowless state and
  // the flag is no longer required.
  base::CommandLine* lacros_command_line =
      base::CommandLine::ForCurrentProcess();
  lacros_command_line->RemoveSwitch(switches::kNoStartupWindow);

  // Modify the command line to restore browser sessions.
  lacros_command_line->AppendSwitch(switches::kRestoreLastSession);

  if (skip_crash_restore)
    lacros_command_line->AppendSwitch(switches::kHideCrashRestoreBubble);

  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowserForLastProfiles(
      *lacros_command_line, base::FilePath(),
      chrome::startup::IsProcessStartup::kYes, chrome::startup::IsFirstRun::kNo,
      profile_info, last_opened_profiles, restore_tabbed_browser);

  return true;
}
