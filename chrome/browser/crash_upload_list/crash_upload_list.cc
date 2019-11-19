// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list.h"

#include "build/build_config.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "chrome/browser/crash_upload_list/crash_upload_list_crashpad.h"
#else
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/crash_upload_list/crash_upload_list_crashpad.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/app/crashpad.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/upload_list/text_log_upload_list.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"
#endif

scoped_refptr<UploadList> CreateCrashUploadList() {
#if defined(OS_MACOSX) || defined(OS_WIN)
  return new CrashUploadListCrashpad();
#elif defined(OS_ANDROID)
  base::FilePath cache_dir;
  base::android::GetCacheDirectory(&cache_dir);
  base::FilePath upload_log_path =
      cache_dir.Append("Crash Reports")
          .AppendASCII(CrashUploadList::kReporterLogFilename);
  return new CrashUploadListAndroid(upload_log_path);
#else

// ChromeOS uses crash_sender as its uploader even when Crashpad is enabled,
// which isn't compatible with CrashUploadListCrashpad. crash_sender continues
// to log uploads in CrashUploadList::kReporterLogFilename.
#if !defined(OS_CHROMEOS)
  if (crash_reporter::IsCrashpadEnabled()) {
    return new CrashUploadListCrashpad();
  }
#endif

  base::FilePath crash_dir_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  return new TextLogUploadList(upload_log_path);
#endif  // defined(OS_MACOSX) || defined(OS_WIN)
}
