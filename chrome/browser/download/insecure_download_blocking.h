// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_
#define CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"

// Returns the correct insecure download blocking behavior for the given
// |item| saved to |path|.  Controlled by kTreatUnsafeDownloadsAsActive.
download::DownloadItem::InsecureDownloadStatus
GetInsecureDownloadStatusForDownload(Profile* profile,
                                     const base::FilePath& path,
                                     const download::DownloadItem* item);

#endif  // CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_
