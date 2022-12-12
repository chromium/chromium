// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_

#include <string>

#include "base/files/file_path.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

struct DownloadTargetInfo {
  DownloadTargetInfo();
  ~DownloadTargetInfo();

  // Final full target path of the download. Must be non-empty for the remaining
  // fields to be considered valid. The path is a local file system path. Any
  // existing file at this path should be overwritten.
  base::FilePath target_path;

  // Disposition. This will be TARGET_DISPOSITION_PROMPT if the user was
  // prompted during the process of determining the download target. Otherwise
  // it will be TARGET_DISPOSITION_OVERWRITE.
  // TODO(asanka): This should be has_user_confirmation or somesuch that
  // indicates that the user has seen and confirmed the download path.
  download::DownloadItem::TargetDisposition target_disposition;

  // Danger type of the download.
  download::DownloadDangerType danger_type;

  // The danger type of the download could be set to MAYBE_DANGEROUS_CONTENT if
  // the file type is handled by SafeBrowsing. However, if the SafeBrowsing
  // service is unable to verify whether the file is safe or not, we are on our
  // own. The value of |danger_level| indicates whether the download should be
  // considered dangerous if SafeBrowsing returns an unknown verdict.
  //
  // Note that some downloads (e.g. "Save link as" on a link to a binary) would
  // not be considered 'Dangerous' even if SafeBrowsing came back with an
  // unknown verdict. So we can't always show a warning when SafeBrowsing fails.
  //
  // The value of |danger_level| should be interpreted as follows:
  //
  //   NOT_DANGEROUS : Unless flagged by SafeBrowsing, the file should be
  //       considered safe.
  //
  //   ALLOW_ON_USER_GESTURE : If SafeBrowsing claims the file is safe, then the
  //       file is safe. An UNKOWN verdict results in the file being marked as
  //       DANGEROUS_FILE.
  //
  //   DANGEROUS : This type of file shouldn't be allowed to download witout any
  //       user action. Hence, if SafeBrowsing marks the file as SAFE, or
  //       UNKONWN, the file will still be conisdered a DANGEROUS_FILE. However,
  //       SafeBrowsing may flag the file as being malicious, in which case the
  //       malicious classification should take precedence.
  safe_browsing::DownloadFileType::DangerLevel danger_level;

  // Suggested intermediate path. The downloaded bytes should be written to this
  // path until all the bytes are available and the user has accepted a
  // dangerous download. At that point, the download can be renamed to
  // |target_path|.
  base::FilePath intermediate_path;

  // MIME type based on the file type of the download. This may be different
  // from DownloadItem::GetMimeType() since the latter is based on the server
  // response, and this one is based on the filename.
  std::string mime_type;

  // Whether the |target_path| would be handled safely by the browser if it were
  // to be opened with a file:// URL. This can be used later to decide how file
  // opens should be handled. The file is considered to be handled safely if the
  // filetype is supported by the renderer or a sandboxed plugin.
  bool is_filetype_handled_safely;

  // Result of the download target determination.
  download::DownloadInterruptReason result;

  // What sort of blocking should be used if the download is insecure.
  download::DownloadItem::InsecureDownloadStatus insecure_download_status = 
          download::DownloadItem::InsecureDownloadStatus::UNKNOWN;

  // Display name of the file.
  base::FilePath display_name;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_INFO_H_
