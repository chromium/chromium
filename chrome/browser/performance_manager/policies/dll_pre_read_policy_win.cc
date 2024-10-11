// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/dll_pre_read_policy_win.h"

#include <atomic>
#include <optional>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/drive_info.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_features.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

namespace performance_manager {

namespace {

enum class ChromeDllOnSsd {
  // It is as of yet unknown whether the Chrome DLL is on an SSD.
  kPending,
  // It can positively be determined that the Chrome DLL is on a non-removable
  // SSD.
  kOnFixedSsd,
  // Either the Chrome DLL is on a spinning disk, a removable SSD, or whether
  // its drive is an SSD/is removable was unable to be determined.
  kNotOnFixedSsd,
};

// Seek penalty state. All reads/writes use std::memory_order_relaxed because
// there is no dependency between those and other memory operations.
std::atomic<ChromeDllOnSsd> g_chrome_dll_on_ssd = ChromeDllOnSsd::kPending;

}  // namespace

// Returns true if `drive_info` indicates a disk that has no seek penalty, is
// not removable, and is not connected via a USB bus. In other words, has the
// highest liklihood of exceedingly fast performance.
bool IsFixedSsd(const std::optional<base::DriveInfo>& drive_info) {
  return drive_info.has_value() &&
         !drive_info->has_seek_penalty.value_or(true) &&
         !drive_info->is_removable.value_or(true) &&
         !drive_info->is_usb.value_or(true);
}

void SetChromeDllOnSsdForTesting(bool on_fixed_ssd) {
  g_chrome_dll_on_ssd.store(on_fixed_ssd ? ChromeDllOnSsd::kOnFixedSsd
                                         : ChromeDllOnSsd::kNotOnFixedSsd);
}

void InitializeDllPrereadPolicy() {
  CHECK_EQ(g_chrome_dll_on_ssd.load(std::memory_order_relaxed),
           ChromeDllOnSsd::kPending);
  // This function uses atomics to coordinate whether the Chrome DLL is on a
  // fixed SSD because the query races with child process creation, and the
  // value would like to be known on the main thread as soon as possible.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce([]() {
        base::FilePath current_module_path;
        std::optional<base::DriveInfo> drive_info;
        if (base::PathService::Get(base::FILE_MODULE, &current_module_path)) {
          drive_info = base::GetFileDriveInfo(current_module_path);
        }
        g_chrome_dll_on_ssd.store(IsFixedSsd(drive_info)
                                      ? ChromeDllOnSsd::kOnFixedSsd
                                      : ChromeDllOnSsd::kNotOnFixedSsd);
      }));
}

bool StartupPrefetchTimeoutElapsed(base::TimeTicks now) {
  return (now - startup_metric_utils::GetCommon().MainEntryPointTicks()) >
         features::kNoPreReadMainDllStartup_StartupDuration.Get();
}

bool ShouldPreReadDllInChild() {
  if (base::FeatureList::IsEnabled(features::kNoPreReadMainDll)) {
    return false;
  }

  // If it hasn't been determined yet whether the Chrome binary has a seek
  // penalty, don't pre-read. In that state, the browser process has
  // likely pre-read the DLL recently and pre-reading again is likely not
  // needed, even if there is a seek penalty.
  if (g_chrome_dll_on_ssd.load(std::memory_order_relaxed) !=
          ChromeDllOnSsd::kNotOnFixedSsd &&
      base::FeatureList::IsEnabled(features::kNoPreReadMainDllIfSsd)) {
    return false;
  }

  // `NoPreReadMainDllStartup` may only select a group for users which are not
  // in `NoPreReadMainDll` AND (have a seek penalty OR are not in
  // `NoPreReadMainDllIfSsd`).
  if (base::FeatureList::IsEnabled(features::kNoPreReadMainDllStartup) &&
      !StartupPrefetchTimeoutElapsed(base::TimeTicks::Now())) {
    return false;
  }

  // TODO(crbug.com/331612431): Add a feature which skips pre-read if one was
  // completed recently.

  return true;
}

}  // namespace performance_manager
