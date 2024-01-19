// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"

struct DownloadTargetInfo {
  DownloadTargetInfo();
  ~DownloadTargetInfo();

  DownloadTargetInfo(const DownloadTargetInfo& other);
  DownloadTargetInfo& operator=(const DownloadTargetInfo& other);
  DownloadTargetInfo(DownloadTargetInfo&& other);
  DownloadTargetInfo& operator=(DownloadTargetInfo&& other);

  // Final full target path of the download. Must be non-empty for the remaining
  // fields to be considered valid. The path is a local file system path. Any
  // existing file at this path should be overwritten.
  base::FilePath target_path;

  // Suggested intermediate path. The downloaded bytes should be written to this
  // path until all the bytes are available and the user has accepted a
  // dangerous download. At that point, the download can be renamed to
  // |target_path|.
  base::FilePath intermediate_path;

  // Display name of the file.
  base::FilePath display_name;

  // MIME type based on the file type of the download. This may be different
  // from DownloadItem::GetMimeType() since the latter is based on the server
  // response, and this one is based on the filename.
  std::string mime_type;

  // Whether the |target_path| would be handled safely by the browser if it were
  // to be opened with a file:// URL. This can be used later to decide how file
  // opens should be handled. The file is considered to be handled safely if the
  // filetype is supported by the renderer or a sandboxed plugin.
  bool is_filetype_handled_safely = false;

  // Disposition. This will be TARGET_DISPOSITION_PROMPT if the user was
  // prompted during the process of determining the download target. Otherwise
  // it will be TARGET_DISPOSITION_OVERWRITE.
  // TODO(asanka): This should be has_user_confirmation or somesuch that
  // indicates that the user has seen and confirmed the download path.
  download::DownloadItem::TargetDisposition target_disposition =
      download::DownloadItem::TARGET_DISPOSITION_OVERWRITE;

  // Danger type of the download.
  download::DownloadDangerType danger_type =
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

  // Result of the download target determination.
  download::DownloadInterruptReason result =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;

  // What sort of blocking should be used if the download is insecure.
  download::DownloadItem::InsecureDownloadStatus insecure_download_status =
      download::DownloadItem::InsecureDownloadStatus::UNKNOWN;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
