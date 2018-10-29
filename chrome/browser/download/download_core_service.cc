// Copyright 2015 The Chromium Authors. All rights reserved.
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
int DownloadCoreService::NonMaliciousDownloadCountAllProfiles() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());

  int count = 0;
  for (auto it = profiles.begin(); it < profiles.end(); ++it) {
    count += DownloadCoreServiceFactory::GetForBrowserContext(*it)
                 ->NonMaliciousDownloadCount();
    if ((*it)->HasOffTheRecordProfile())
      count += DownloadCoreServiceFactory::GetForBrowserContext(
                   (*it)->GetOffTheRecordProfile())
                   ->NonMaliciousDownloadCount();
  }

  return count;
}

// static
void DownloadCoreService::CancelAllDownloads() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (auto it = profiles.begin(); it < profiles.end(); ++it) {
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(*it);
    service->CancelDownloads();
  }
}
