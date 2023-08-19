// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/extension_system.h"

namespace lock_screen_apps {

LockScreenProfileCreatorImpl::LockScreenProfileCreatorImpl(
    Profile* primary_profile,
    const base::TickClock* tick_clock)
    : primary_profile_(primary_profile), tick_clock_(tick_clock) {}

LockScreenProfileCreatorImpl::~LockScreenProfileCreatorImpl() {}

void LockScreenProfileCreatorImpl::OnAvailableNoteTakingAppsUpdated() {}

void LockScreenProfileCreatorImpl::OnPreferredNoteTakingAppUpdated(
    Profile* profile) {
  if (profile != primary_profile_)
    return;

  ash::NoteTakingHelper* helper = ash::NoteTakingHelper::Get();
  std::string app_id = helper->GetPreferredAppId(primary_profile_);
  // Lock screen apps service should always exist on the primary profile.
  DCHECK(ash::LockScreenAppsFactory::IsSupportedProfile(primary_profile_));
  ash::LockScreenAppSupport support =
      ash::LockScreenApps::GetSupport(primary_profile_, app_id);

  if (support != ash::LockScreenAppSupport::kEnabled)
    return;

  // Lock screen profile creation should be attempted only once - stop observing
  // note taking apps status so profile creation is not attempted again if lock
  // screen note availability changes.
  note_taking_helper_observation_.Reset();

  OnLockScreenProfileCreateStarted();

  g_browser_process->profile_manager()->CreateProfileAsync(
      ash::ProfileHelper::GetLockScreenAppProfilePath(),
      /*initialized_callback=*/
      base::BindOnce(&LockScreenProfileCreatorImpl::OnProfileReady,
                     weak_ptr_factory_.GetWeakPtr(), tick_clock_->NowTicks()),
      /*created_callback=*/base::BindOnce([](Profile* profile) {
        // Disable safe browsing for the profile to avoid activating
        // SafeBrowsingService when the user has safe browsing disabled
        // (reasoning similar to http://crbug.com/461493).
        // TODO(tbarzic): Revisit this if webviews get enabled for lock screen
        // apps.
        profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
        profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
      }));
}

void LockScreenProfileCreatorImpl::InitializeImpl() {
  // Make sure that extension system is ready (and extension regustry is loaded)
  // before testing for lock screen enabled app existence.
  extensions::ExtensionSystem::Get(primary_profile_)
      ->ready()
      .Post(
          FROM_HERE,
          base::BindOnce(&LockScreenProfileCreatorImpl::OnExtensionSystemReady,
                         weak_ptr_factory_.GetWeakPtr()));
}

void LockScreenProfileCreatorImpl::OnExtensionSystemReady() {
  note_taking_helper_observation_.Observe(ash::NoteTakingHelper::Get());

  // Determine the current note taking state.
  OnPreferredNoteTakingAppUpdated(primary_profile_);
}

void LockScreenProfileCreatorImpl::OnProfileReady(
    const base::TimeTicks& start_time,
    Profile* profile) {
  // On error, bail out - this will cause the lock screen apps to remain
  // unavailable on the device.
  if (!profile) {
    OnLockScreenProfileCreated(nullptr);
    return;
  }

  profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

  OnLockScreenProfileCreated(profile);
}

}  // namespace lock_screen_apps
