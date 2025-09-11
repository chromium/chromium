// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/download/public/background_service/background_download_service.h"

namespace optimization_guide {

ChromeProfileDownloadServiceTracker::ChromeProfileDownloadServiceTracker() {
  auto* profile_manager = g_browser_process->profile_manager();
  profile_manager_observation_.Observe(profile_manager);
  // Start tracking any profiles that already exist.
  for (auto* profile : profile_manager->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
}

ChromeProfileDownloadServiceTracker::~ChromeProfileDownloadServiceTracker() =
    default;

void ChromeProfileDownloadServiceTracker::OnProfileAdded(Profile* profile) {
  if (profile->IsOffTheRecord() || profile->IsSystemProfile() ||
      !profile->AllowsBrowserWindows()) {
    return;
  }
  active_profile_observers_.AddObservation(profile);
}

void ChromeProfileDownloadServiceTracker::OnProfileWillBeDestroyed(
    Profile* profile) {
  if (active_profile_observers_.IsObservingSource(profile)) {
    active_profile_observers_.RemoveObservation(profile);
  }
}

download::BackgroundDownloadService*
ChromeProfileDownloadServiceTracker::GetBackgroundDownloadService() {
  // Pick the first profile in the list of active profiles.
  return active_profile_observers_.IsObservingAnySource()
             ? BackgroundDownloadServiceFactory::GetForKey(
                   active_profile_observers_.sources().front()->GetProfileKey())
             : nullptr;
}

}  // namespace optimization_guide
