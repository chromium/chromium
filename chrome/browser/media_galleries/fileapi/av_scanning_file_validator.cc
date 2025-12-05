// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/av_scanning_file_validator.h"

#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/public/browser/browser_thread.h"

AVScanningFileValidator::~AVScanningFileValidator() = default;

void AVScanningFileValidator::StartPostWriteValidation(
    const base::FilePath& dest_platform_path,
    ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(result_callback).Run(base::File::FILE_OK);
}

AVScanningFileValidator::AVScanningFileValidator(
    download::QuarantineConnectionCallback quarantine_connection_callback)
    : quarantine_connection_callback_(quarantine_connection_callback) {}
