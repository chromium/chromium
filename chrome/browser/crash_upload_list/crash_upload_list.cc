// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/upload_list/combining_upload_list.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/upload_list/text_log_upload_list.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/browser/crash_upload_list_crashpad.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/crash_upload_list/crash_upload_list_chromeos.h"
#endif

scoped_refptr<UploadList> CreateCrashUploadList() {
#if BUILDFLAG(IS_ANDROID)
  base::FilePath cache_dir;
  base::android::GetCacheDirectory(&cache_dir);
  base::FilePath upload_log_path =
      cache_dir.Append("Crash Reports")
          .AppendASCII(CrashUploadList::kReporterLogFilename);
  return new CrashUploadListAndroid(upload_log_path);
#else
  base::FilePath crash_dir_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);

#if BUILDFLAG(IS_CHROMEOS)
  return base::MakeRefCounted<CrashUploadListChromeOS>(upload_log_path);
#else
  // Crashpad keeps the records of C++ crashes (segfaults, etc) in its
  // internal database. The JavaScript error reporter writes JS error upload
  // records to the older text format. Combine the two to present a complete
  // list to the user.
  std::vector<scoped_refptr<UploadList>> uploaders = {
      base::MakeRefCounted<CrashUploadListCrashpad>(),
      base::MakeRefCounted<TextLogUploadList>(upload_log_path)};
  return base::MakeRefCounted<CombiningUploadList>(std::move(uploaders));
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_ANDROID)
}
