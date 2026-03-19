// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_
#define CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"

// Each download is recorded with two histograms.
// This histogram always summarizes the type of download. See
// InsecureDownloadSecurityStatus.
inline constexpr char kInsecureDownloadHistogramName[] =
    "Download.InsecureBlocking.Totals";
// Suffixes for histogram names.
inline constexpr char kInsecureDownloadHistogramTargetSecure[] =
    "DownloadSecure";
inline constexpr char kInsecureDownloadHistogramTargetInsecure[] =
    "DownloadInsecure";

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused.  Please keep in sync with
// "InsecureDownloadSecurityStatus" in src/tools/metrics/histograms/enums.xml.
enum class InsecureDownloadSecurityStatus {
  kInitiatorUnknownFileSecure = 0,
  kInitiatorUnknownFileInsecure = 1,
  kInitiatorSecureFileSecure = 2,
  kInitiatorSecureFileInsecure = 3,
  kInitiatorInsecureFileSecure = 4,
  kInitiatorInsecureFileInsecure = 5,
  kInitiatorInferredSecureFileSecure = 6,
  kInitiatorInferredSecureFileInsecure = 7,
  kInitiatorInferredInsecureFileSecure = 8,
  kInitiatorInferredInsecureFileInsecure = 9,
  kDownloadIgnored = 10,
  kInitiatorInsecureNonUniqueFileSecure = 11,
  kInitiatorInsecureNonUniqueFileInsecure = 12,
  kMaxValue = kInitiatorInsecureNonUniqueFileInsecure,
};

// Returns the correct insecure download blocking behavior for the given
// |item| saved to |path|.  Controlled by kTreatUnsafeDownloadsAsActive.
download::DownloadItem::InsecureDownloadStatus
GetInsecureDownloadStatusForDownload(Profile* profile,
                                     const base::FilePath& path,
                                     const download::DownloadItem* item);

#endif  // CHROME_BROWSER_DOWNLOAD_INSECURE_DOWNLOAD_BLOCKING_H_
