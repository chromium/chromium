// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/downgrade_manager_delegate_impl.h"

#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/downgrade/snapshot_file_collector.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/prefs/pref_service.h"

namespace downgrade {

int DowngradeManagerDelegateImpl::GetMaxNumberOfSnapshots() const {
  return g_browser_process->local_state()->GetInteger(
      prefs::kUserDataSnapshotRetentionLimit);
}

bool DowngradeManagerDelegateImpl::UserDataSnapshotEnabled() const {
  return
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      base::IsEnterpriseDevice() ||
#endif
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid();
}

base::FilePath DowngradeManagerDelegateImpl::GetDiskCacheDir() const {
  base::FilePath disk_cache_dir =
      g_browser_process->local_state()->GetFilePath(prefs::kDiskCacheDir);
  if (disk_cache_dir.ReferencesParent()) {
    return base::MakeAbsoluteFilePath(disk_cache_dir);
  }
  return disk_cache_dir;
}

std::vector<SnapshotItemDetails>
DowngradeManagerDelegateImpl::GetUserDataSnapshotItems() const {
  return CollectUserDataItems();
}

std::vector<SnapshotItemDetails>
DowngradeManagerDelegateImpl::GetProfileSnapshotItems() const {
  return CollectProfileItems();
}

}  // namespace downgrade
