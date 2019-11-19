// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"

#include <string>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/download/quarantine/quarantine.h"
#include "url/gurl.h"
#endif  // defined(OS_WIN)

namespace {

#if defined(OS_WIN)
base::File::Error ScanFile(const base::FilePath& dest_platform_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

    download::QuarantineFileResult quarantine_result = download::QuarantineFile(
        dest_platform_path, GURL(), GURL(), std::string());

    return quarantine_result == download::QuarantineFileResult::OK
               ? base::File::FILE_OK
               : base::File::FILE_ERROR_SECURITY;
}
#endif  // defined(OS_WIN)

}  // namespace

AVScanningFileValidator::~AVScanningFileValidator() {}

void AVScanningFileValidator::StartPostWriteValidation(
    const base::FilePath& dest_platform_path,
    const ResultCallback& result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

#if defined(OS_WIN)
  base::PostTaskAndReplyWithResult(
      base::CreateCOMSTATaskRunner({base::ThreadPool(), base::MayBlock(),
                                    base::TaskPriority::USER_VISIBLE})
          .get(),
      FROM_HERE, base::Bind(&ScanFile, dest_platform_path), result_callback);
#else
  result_callback.Run(base::File::FILE_OK);
#endif
}

AVScanningFileValidator::AVScanningFileValidator() {
}
