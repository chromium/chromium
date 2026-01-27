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
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
}

ChromeProfileDownloadServiceTracker::~ChromeProfileDownloadServiceTracker() {
  for (Profile* profile : observed_profiles_) {
    profile->RemoveObserver(this);
  }
  observed_profiles_.clear();
}

void ChromeProfileDownloadServiceTracker::OnProfileAdded(Profile* profile) {
  if (profile->IsOffTheRecord() || profile->IsSystemProfile() ||
      !profile->AllowsBrowserWindows()) {
    return;
  }
  observed_profiles_.push_back(profile);
  profile->AddObserver(this);
}

void ChromeProfileDownloadServiceTracker::OnProfileWillBeDestroyed(
    Profile* profile) {
  if (auto iter = std::ranges::find(observed_profiles_, profile);
      iter != observed_profiles_.end()) {
    observed_profiles_.erase(iter);
    profile->RemoveObserver(this);
  }
}

download::BackgroundDownloadService*
ChromeProfileDownloadServiceTracker::GetBackgroundDownloadService() {
  // Pick the first profile in the list of active profiles.
  return !observed_profiles_.empty()
             ? BackgroundDownloadServiceFactory::GetForKey(
                   observed_profiles_.front()->GetProfileKey())
             : nullptr;
}

}  // namespace optimization_guide
