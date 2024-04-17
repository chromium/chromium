// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_core_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

DownloadCoreService::DownloadCoreService() = default;

DownloadCoreService::~DownloadCoreService() = default;

// static
int DownloadCoreService::BlockingShutdownCountAllProfiles() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());

  int count = 0;
  for (auto it = profiles.begin(); it < profiles.end(); ++it) {
    // The download core service might not be available for some irregular
    // profiles, like the System Profile.
    if (DownloadCoreService* service =
            DownloadCoreServiceFactory::GetForBrowserContext(*it)) {
      count += service->BlockingShutdownCount();
    }

    std::vector<Profile*> otr_profiles = (*it)->GetAllOffTheRecordProfiles();
    for (Profile* otr : otr_profiles) {
      // The download core service might not be available for some irregular
      // profiles, like the System Profile.
      if (DownloadCoreService* otr_service =
              DownloadCoreServiceFactory::GetForBrowserContext(otr)) {
        count += otr_service->BlockingShutdownCount();
      }
    }
  }

  return count;
}

// static
void DownloadCoreService::CancelAllDownloads(CancelDownloadsTrigger trigger) {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (auto it = profiles.begin(); it < profiles.end(); ++it) {
    // The download core service might not be available for some irregular
    // profiles, like the System Profile.
    if (DownloadCoreService* service =
            DownloadCoreServiceFactory::GetForBrowserContext(*it)) {
      service->CancelDownloads(trigger);
    }
  }
}
