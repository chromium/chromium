// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/lock_screen_profile_creator_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/one_shot_event.h"
#include "base/strings/string16.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "extensions/browser/extension_system.h"

namespace lock_screen_apps {

LockScreenProfileCreatorImpl::LockScreenProfileCreatorImpl(
    Profile* primary_profile,
    const base::TickClock* tick_clock)
    : primary_profile_(primary_profile),
      tick_clock_(tick_clock),
      note_taking_helper_observer_(this) {}

LockScreenProfileCreatorImpl::~LockScreenProfileCreatorImpl() {}

void LockScreenProfileCreatorImpl::OnAvailableNoteTakingAppsUpdated() {}

void LockScreenProfileCreatorImpl::OnPreferredNoteTakingAppUpdated(
    Profile* profile) {
  if (profile != primary_profile_)
    return;

  std::unique_ptr<chromeos::NoteTakingAppInfo> note_taking_app =
      chromeos::NoteTakingHelper::Get()->GetPreferredChromeAppInfo(
          primary_profile_);

  if (!note_taking_app || !note_taking_app->preferred ||
      note_taking_app->lock_screen_support !=
          chromeos::NoteTakingLockScreenSupport::kEnabled) {
    return;
  }

  // Lock screen profile creation should be attempted only once - stop observing
  // note taking apps status so profile creation is not attempted again if lock
  // screen note availability changes.
  note_taking_helper_observer_.RemoveAll();

  OnLockScreenProfileCreateStarted();

  g_browser_process->profile_manager()->CreateProfileAsync(
      chromeos::ProfileHelper::GetLockScreenAppProfilePath(),
      base::Bind(&LockScreenProfileCreatorImpl::OnProfileReady,
                 weak_ptr_factory_.GetWeakPtr(), tick_clock_->NowTicks()),
      base::string16() /* name */, "" /* icon_url*/);
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
  note_taking_helper_observer_.Add(chromeos::NoteTakingHelper::Get());

  // Determine the current note taking state.
  OnPreferredNoteTakingAppUpdated(primary_profile_);
}

void LockScreenProfileCreatorImpl::OnProfileReady(
    const base::TimeTicks& start_time,
    Profile* profile,
    Profile::CreateStatus status) {
  // Ignore CREATED status - wait for profile to be initialized before
  // continuing.
  if (status == Profile::CREATE_STATUS_CREATED) {
    // Disable safe browsing for the profile to avoid activating
    // SafeBrowsingService when the user has safe browsing disabled (reasoning
    // similar to http://crbug.com/461493).
    // TODO(tbarzic): Revisit this if webviews get enabled for lock screen apps.
    profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Apps.LockScreen.AppsProfile.Creation.Success",
                        status == Profile::CREATE_STATUS_INITIALIZED);

  // On error, bail out - this will cause the lock screen apps to remain
  // unavailable on the device.
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    OnLockScreenProfileCreated(nullptr);
    return;
  }

  profile->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

  UMA_HISTOGRAM_TIMES("Apps.LockScreen.AppsProfile.Creation.Duration",
                      tick_clock_->NowTicks() - start_time);

  OnLockScreenProfileCreated(profile);
}

}  // namespace lock_screen_apps
