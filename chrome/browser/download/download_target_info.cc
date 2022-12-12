// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_target_info.h"

#include "components/safe_browsing/content/common/file_type_policies.h"

DownloadTargetInfo::DownloadTargetInfo()
    : target_disposition(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE),
      danger_type(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      danger_level(safe_browsing::DownloadFileType::NOT_DANGEROUS),
      is_filetype_handled_safely(false),
      result(download::DOWNLOAD_INTERRUPT_REASON_NONE) {}

DownloadTargetInfo::~DownloadTargetInfo() {}
