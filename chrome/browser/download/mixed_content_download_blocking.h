// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_DOWNLOAD_MIXED_CONTENT_DOWNLOAD_BLOCKING_H_
#define CHROME_BROWSER_DOWNLOAD_MIXED_CONTENT_DOWNLOAD_BLOCKING_H_

#include "base/files/file_path.h"
#include "components/download/public/common/download_item.h"

// Each download is recorded with two histograms.
// This histogram always summarizes the type of download. See
// InsecureDownloadSecurityStatus.
const char* const kInsecureDownloadHistogramName =
    "Download.InsecureBlocking.Totals";
// These histograms record the extension of the download. Only one is used per
// download. See InsecureDownloadExtensions.
const char* const kInsecureDownloadHistogramInitiatorUnknownTargetSecure =
    "Download.InsecureBlocking.Extensions.InitiatorUnknown.DownloadSecure";
const char* const kInsecureDownloadHistogramInitiatorUnknownTargetInsecure =
    "Download.InsecureBlocking.Extensions.InitiatorUnknown.DownloadInsecure";
const char* const kInsecureDownloadHistogramInitiatorSecureTargetSecure =
    "Download.InsecureBlocking.Extensions.InitiatorSecure.DownloadSecure";
const char* const kInsecureDownloadHistogramInitiatorSecureTargetInsecure =
    "Download.InsecureBlocking.Extensions.InitiatorSecure.DownloadInsecure";
const char* const kInsecureDownloadHistogramInitiatorInsecureTargetSecure =
    "Download.InsecureBlocking.Extensions.InitiatorInsecure.DownloadSecure";
const char* const kInsecureDownloadHistogramInitiatorInsecureTargetInsecure =
    "Download.InsecureBlocking.Extensions.InitiatorInsecure.DownloadInsecure";

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
  kMaxValue = kInitiatorInsecureFileInsecure,
};

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused.  Please keep in sync with
// "InsecureDownloadExtensions" in src/tools/metrics/histograms/enums.xml.
enum class InsecureDownloadExtensions {
  kUnknown = 0,
  kNone = 1,
  kImage = 2,
  kArchive = 3,
  kAudio = 4,
  kVideo = 5,
  kMSExecutable = 6,
  kMacExecutable = 7,
  kWeb = 8,
  kText = 9,
  kMSOffice = 10,
  kPDF = 11,
  kCRX = 12,
  kAPK = 13,
  kBIN = 14,
  kSH = 15,
  kVB = 16,
  kSWF = 17,
  kICO = 18,
  kEPUB = 19,
  kICS = 20,
  kSVG = 21,
  kMaxValue = kSVG,
};

struct ExtensionMapping {
  const char* const extension;
  const InsecureDownloadExtensions value;
};

// Maps a file's extension to its enum bucket for metrics. Since these are
// logged to UMA, they shouldn't be changed unless there's an egregious error.
// This list aims to cover the common download cases. Extensions can be added as
// needed, but the list need not be comprehensive. It is used to track risky
// download types. Low-risk extensions are only categorized for completeness.
static const ExtensionMapping kExtensionsToEnum[] = {
    {"png", InsecureDownloadExtensions::kImage},
    {"jpg", InsecureDownloadExtensions::kImage},
    {"gif", InsecureDownloadExtensions::kImage},
    {"bmp", InsecureDownloadExtensions::kImage},
    {"webp", InsecureDownloadExtensions::kImage},

    {"gz", InsecureDownloadExtensions::kArchive},
    {"zip", InsecureDownloadExtensions::kArchive},
    {"bz2", InsecureDownloadExtensions::kArchive},
    {"7z", InsecureDownloadExtensions::kArchive},
    {"rar", InsecureDownloadExtensions::kArchive},
    {"tar", InsecureDownloadExtensions::kArchive},

    {"mp3", InsecureDownloadExtensions::kAudio},
    {"aac", InsecureDownloadExtensions::kAudio},
    {"oga", InsecureDownloadExtensions::kAudio},
    {"flac", InsecureDownloadExtensions::kAudio},
    {"wav", InsecureDownloadExtensions::kAudio},
    {"m4a", InsecureDownloadExtensions::kAudio},

    {"webm", InsecureDownloadExtensions::kVideo},
    {"mp4", InsecureDownloadExtensions::kVideo},
    {"m4p", InsecureDownloadExtensions::kVideo},
    {"m4v", InsecureDownloadExtensions::kVideo},
    {"mpg", InsecureDownloadExtensions::kVideo},
    {"mpeg", InsecureDownloadExtensions::kVideo},
    {"mpe", InsecureDownloadExtensions::kVideo},
    {"mpv", InsecureDownloadExtensions::kVideo},
    {"ogg", InsecureDownloadExtensions::kVideo},

    {"exe", InsecureDownloadExtensions::kMSExecutable},
    {"com", InsecureDownloadExtensions::kMSExecutable},
    {"scr", InsecureDownloadExtensions::kMSExecutable},
    {"msi", InsecureDownloadExtensions::kMSExecutable},

    {"dmg", InsecureDownloadExtensions::kMacExecutable},
    {"pkg", InsecureDownloadExtensions::kMacExecutable},

    {"html", InsecureDownloadExtensions::kWeb},
    {"htm", InsecureDownloadExtensions::kWeb},
    {"css", InsecureDownloadExtensions::kWeb},
    {"js", InsecureDownloadExtensions::kWeb},
    {"xml", InsecureDownloadExtensions::kWeb},

    {"txt", InsecureDownloadExtensions::kText},
    {"json", InsecureDownloadExtensions::kText},
    {"csv", InsecureDownloadExtensions::kText},
    {"tsv", InsecureDownloadExtensions::kText},
    {"sql", InsecureDownloadExtensions::kText},

    {"doc", InsecureDownloadExtensions::kMSOffice},
    {"dot", InsecureDownloadExtensions::kMSOffice},
    {"wbk", InsecureDownloadExtensions::kMSOffice},
    {"docx", InsecureDownloadExtensions::kMSOffice},
    {"docm", InsecureDownloadExtensions::kMSOffice},
    {"dotx", InsecureDownloadExtensions::kMSOffice},
    {"dotm", InsecureDownloadExtensions::kMSOffice},
    {"docb", InsecureDownloadExtensions::kMSOffice},
    {"xls", InsecureDownloadExtensions::kMSOffice},
    {"xlt", InsecureDownloadExtensions::kMSOffice},
    {"xlm", InsecureDownloadExtensions::kMSOffice},
    {"xlsx", InsecureDownloadExtensions::kMSOffice},
    {"xlsm", InsecureDownloadExtensions::kMSOffice},
    {"xltx", InsecureDownloadExtensions::kMSOffice},
    {"xltm", InsecureDownloadExtensions::kMSOffice},
    {"xlsb", InsecureDownloadExtensions::kMSOffice},
    {"xll", InsecureDownloadExtensions::kMSOffice},
    {"xlw", InsecureDownloadExtensions::kMSOffice},
    {"ppt", InsecureDownloadExtensions::kMSOffice},
    {"pot", InsecureDownloadExtensions::kMSOffice},
    {"pps", InsecureDownloadExtensions::kMSOffice},
    {"pptx", InsecureDownloadExtensions::kMSOffice},
    {"pptm", InsecureDownloadExtensions::kMSOffice},
    {"potx", InsecureDownloadExtensions::kMSOffice},
    {"potm", InsecureDownloadExtensions::kMSOffice},
    {"ppam", InsecureDownloadExtensions::kMSOffice},
    {"ppsx", InsecureDownloadExtensions::kMSOffice},
    {"ppsm", InsecureDownloadExtensions::kMSOffice},
    {"sldx", InsecureDownloadExtensions::kMSOffice},
    {"sldm", InsecureDownloadExtensions::kMSOffice},

    {"pdf", InsecureDownloadExtensions::kPDF},
    {"crx", InsecureDownloadExtensions::kCRX},
    {"apk", InsecureDownloadExtensions::kAPK},
    {"bin", InsecureDownloadExtensions::kBIN},
    {"sh", InsecureDownloadExtensions::kSH},
    {"vb", InsecureDownloadExtensions::kVB},
    {"swf", InsecureDownloadExtensions::kSWF},
    {"ico", InsecureDownloadExtensions::kICO},
    {"epub", InsecureDownloadExtensions::kEPUB},
    {"ics", InsecureDownloadExtensions::kICS},
    {"svg", InsecureDownloadExtensions::kSVG},
};

// When enabled (via kTreatUnsafeDownloadsAsActive), block unsafe downloads
// that are requested by secure sources but are served insecurely.
bool ShouldBlockFileAsMixedContent(const base::FilePath& path,
                                   const download::DownloadItem& item);

#endif  // CHROME_BROWSER_DOWNLOAD_MIXED_CONTENT_DOWNLOAD_BLOCKING_H_
