// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/lazy_task_runner.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/app/crashpad.h"

namespace {

base::LazySequencedTaskRunner g_collect_stats_consent_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

base::LazyInstance<std::string>::Leaky g_posix_client_id =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky g_posix_client_id_lock =
    LAZY_INSTANCE_INITIALIZER;

// File name used in the user data dir to indicate consent.
const char kConsentToSendStats[] = "Consent To Send Stats";

void SetConsentFilePermissionIfNeeded(const base::FilePath& consent_file) {
#if defined(OS_CHROMEOS)
  // The consent file needs to be world readable. See http://crbug.com/383003
  int permissions;
  if (base::GetPosixFilePermissions(consent_file, &permissions) &&
      (permissions & base::FILE_PERMISSION_READ_BY_OTHERS) == 0) {
    permissions |= base::FILE_PERMISSION_READ_BY_OTHERS;
    base::SetPosixFilePermissions(consent_file, permissions);
  }
#endif
}

}  // namespace

// static
base::SequencedTaskRunner*
GoogleUpdateSettings::CollectStatsConsentTaskRunner() {
  // TODO(fdoray): Use LazySequencedTaskRunner::GetRaw() here instead of
  // .Get().get() when it's added to the API, http://crbug.com/730170.
  return g_collect_stats_consent_task_runner.Get().get();
}

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  base::FilePath consent_file;
  base::PathService::Get(chrome::DIR_USER_DATA, &consent_file);
  consent_file = consent_file.Append(kConsentToSendStats);

  if (!base::DirectoryExists(consent_file.DirName()))
    return false;

  std::string tmp_guid;
  bool consented = base::ReadFileToString(consent_file, &tmp_guid);
  if (consented) {
    SetConsentFilePermissionIfNeeded(consent_file);

    base::AutoLock lock(g_posix_client_id_lock.Get());
    g_posix_client_id.Get().assign(tmp_guid);
  }
  return consented;
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
#if defined(OS_MACOSX)
  crash_reporter::SetUploadConsent(consented);
#elif defined(OS_LINUX)
  if (crash_reporter::IsCrashpadEnabled()) {
    crash_reporter::SetUploadConsent(consented);
  }
#endif

  base::FilePath consent_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!base::DirectoryExists(consent_dir))
    return false;

  base::AutoLock lock(g_posix_client_id_lock.Get());

  base::FilePath consent_file = consent_dir.AppendASCII(kConsentToSendStats);
  if (!consented) {
    g_posix_client_id.Get().clear();
    return base::DeleteFile(consent_file, false);
  }

  const std::string& client_id = g_posix_client_id.Get();
  if (base::PathExists(consent_file) && client_id.empty())
    return true;

  int size = client_id.size();
  bool write_success =
      base::WriteFile(consent_file, client_id.c_str(), size) == size;
  if (write_success)
    SetConsentFilePermissionIfNeeded(consent_file);
  return write_success;
}

// static
// TODO(gab): Implement storing/loading for all ClientInfo fields on POSIX.
std::unique_ptr<metrics::ClientInfo>
GoogleUpdateSettings::LoadMetricsClientInfo() {
  std::unique_ptr<metrics::ClientInfo> client_info(new metrics::ClientInfo);

  base::AutoLock lock(g_posix_client_id_lock.Get());
  if (g_posix_client_id.Get().empty())
    return nullptr;
  client_info->client_id = g_posix_client_id.Get();

  return client_info;
}

// static
// TODO(gab): Implement storing/loading for all ClientInfo fields on POSIX.
void GoogleUpdateSettings::StoreMetricsClientInfo(
    const metrics::ClientInfo& client_info) {
  // Make sure that user has consented to send crashes.
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return;

  {
    // Since user has consented, write the metrics id to the file.
    base::AutoLock lock(g_posix_client_id_lock.Get());
    g_posix_client_id.Get() = client_info.client_id;
  }
  GoogleUpdateSettings::SetCollectStatsConsent(true);
}

// GetLastRunTime and SetLastRunTime are not implemented for posix. Their
// current return values signal failure which the caller is designed to
// handle.

// static
int GoogleUpdateSettings::GetLastRunTime() {
  return -1;
}

// static
bool GoogleUpdateSettings::SetLastRunTime() {
  return false;
}
