// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if defined(OS_MAC) || defined(OS_WIN)
#include "components/crash/core/browser/crash_upload_list_crashpad.h"
#else
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/core/app/crashpad.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/upload_list/text_log_upload_list.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"
#endif

#if !(BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS))
#include "components/crash/core/browser/crash_upload_list_crashpad.h"
#endif

#if defined(OS_LINUX)
#include "components/upload_list/combining_upload_list.h"
#endif

scoped_refptr<UploadList> CreateCrashUploadList() {
#if defined(OS_MAC) || defined(OS_WIN)
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
// Linux is handled below.
#if !(BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) || \
      defined(OS_LINUX))
  if (crash_reporter::IsCrashpadEnabled()) {
    return new CrashUploadListCrashpad();
  }
#endif

  base::FilePath crash_dir_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  scoped_refptr<UploadList> result =
      base::MakeRefCounted<TextLogUploadList>(upload_log_path);

#if defined(OS_LINUX)
  if (crash_reporter::IsCrashpadEnabled()) {
    // Crashpad keeps the records of C++ crashes (segfaults, etc) in its
    // internal database. The JavaScript error reporter writes JS error upload
    // records to the older text format. Combine the two to present a complete
    // list to the user.
    std::vector<scoped_refptr<UploadList>> uploaders = {
        base::MakeRefCounted<CrashUploadListCrashpad>(), std::move(result)};
    result = base::MakeRefCounted<CombiningUploadList>(std::move(uploaders));
  }
#endif
  return result;
#endif  // defined(OS_MAC) || defined(OS_WIN)
}
