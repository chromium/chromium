// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_start_util.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

void StartSyncOnUIThread(const base::FilePath& profile, syncer::DataType type) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager) {
    // Can happen in tests.
    DVLOG(2) << "No ProfileManager, can't start sync.";
    return;
  }

  Profile* p = profile_manager->GetProfileByPath(profile);
  if (!p) {
    DVLOG(2) << "Profile not found, can't start sync.";
    return;
  }

  syncer::SyncService* service = SyncServiceFactory::GetForProfile(p);
  if (!service) {
    DVLOG(2) << "No SyncService for profile, can't start sync.";
    return;
  }
  service->OnDataTypeRequestsSyncStartup(type);
}

void StartSyncProxy(const base::FilePath& profile, syncer::DataType type) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&StartSyncOnUIThread, profile, type));
}

}  // namespace

namespace sync_start_util {

syncer::SyncableService::StartSyncFlare GetFlareForSyncableService(
    const base::FilePath& profile_path) {
  return base::BindRepeating(&StartSyncProxy, profile_path);
}

}  // namespace sync_start_util
