// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing download protection code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_UTIL_H_

#include "base/callback_list.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/file_system_access_write_item.h"
#include "net/cert/x509_certificate.h"

namespace safe_browsing {

// Enum to keep track why a particular download verdict was chosen.
// Used for UMA metrics. Do not reorder.
//
// The UMA enum is called SBClientDownloadCheckDownloadStats.
enum DownloadCheckResultReason {
  REASON_INVALID_URL = 0,
  REASON_SB_DISABLED = 1,
  REASON_ALLOWLISTED_URL = 2,
  REASON_ALLOWLISTED_REFERRER = 3,
  REASON_INVALID_REQUEST_PROTO = 4,
  REASON_SERVER_PING_FAILED = 5,
  REASON_INVALID_RESPONSE_PROTO = 6,
  REASON_NOT_BINARY_FILE = 7,
  REASON_REQUEST_CANCELED = 8,
  REASON_DOWNLOAD_DANGEROUS = 9,
  REASON_DOWNLOAD_SAFE = 10,
  REASON_EMPTY_URL_CHAIN = 11,
  DEPRECATED_REASON_HTTPS_URL = 12,
  REASON_PING_DISABLED = 13,
  REASON_TRUSTED_EXECUTABLE = 14,
  REASON_OS_NOT_SUPPORTED = 15,
  REASON_DOWNLOAD_UNCOMMON = 16,
  REASON_DOWNLOAD_NOT_SUPPORTED = 17,
  REASON_INVALID_RESPONSE_VERDICT = 18,
  REASON_ARCHIVE_WITHOUT_BINARIES = 19,
  REASON_DOWNLOAD_DANGEROUS_HOST = 20,
  REASON_DOWNLOAD_POTENTIALLY_UNWANTED = 21,
  REASON_UNSUPPORTED_URL_SCHEME = 22,
  REASON_MANUAL_BLOCKLIST = 23,
  REASON_LOCAL_FILE = 24,
  REASON_REMOTE_FILE = 25,
  REASON_SAMPLED_UNSUPPORTED_FILE = 26,
  REASON_VERDICT_UNKNOWN = 27,
  REASON_DOWNLOAD_DESTROYED = 28,
  REASON_BLOCKED_PASSWORD_PROTECTED = 29,
  REASON_BLOCKED_TOO_LARGE = 30,
  REASON_SENSITIVE_CONTENT_WARNING = 31,
  REASON_SENSITIVE_CONTENT_BLOCK = 32,
  REASON_DEEP_SCANNED_SAFE = 33,
  REASON_DEEP_SCAN_PROMPT = 34,
  REASON_BLOCKED_UNSUPPORTED_FILE_TYPE = 35,
  REASON_DOWNLOAD_DANGEROUS_ACCOUNT_COMPROMISE = 36,
  REASON_LOCAL_DECRYPTION_PROMPT = 37,
  REASON_LOCAL_DECRYPTION_FAILED = 38,
  REASON_IMMEDIATE_DEEP_SCAN = 39,
  REASON_MAX  // Always add new values before this one.
};

// Enumerate for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
// be mixed together based on their values).
enum SBStatsType {
  DOWNLOAD_URL_CHECKS_TOTAL,
  DEPRECATED_DOWNLOAD_URL_CHECKS_CANCELED,
  DOWNLOAD_URL_CHECKS_MALWARE,

  DEPRECATED_DOWNLOAD_HASH_CHECKS_TOTAL,
  DEPRECATED_DOWNLOAD_HASH_CHECKS_MALWARE,

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_CHECKS_MAX
};

enum AllowlistType {
  NO_ALLOWLIST_MATCH,
  URL_ALLOWLIST,
  SIGNATURE_ALLOWLIST,
  ALLOWLIST_TYPE_MAX
};

// Enum for events related to the deep scanning of a download. These values
// are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DeepScanEvent {
  kPromptShown = 0,
  kPromptBypassed = 1,
  kPromptAccepted = 2,
  kScanCanceled = 3,
  kScanCompleted = 4,
  kScanFailed = 5,
  kScanDeleted = 6,
  kPromptAcceptedFromWebUI = 7,
  kIncorrectPassword = 8,
  kMaxValue = kIncorrectPassword,
};
void LogDeepScanEvent(download::DownloadItem* item, DeepScanEvent event);
void LogLocalDecryptionEvent(DeepScanEvent event);

// Callback type which is invoked once the download request is done.
typedef base::OnceCallback<void(DownloadCheckResult)> CheckDownloadCallback;

// Callback type which is invoked once the download request is done. This is
// used in cases where asynchronous scanning is allowed, so the callback is
// triggered multiple times (once when asynchronous scanning begins, once when
// the final result is ready).
typedef base::RepeatingCallback<void(DownloadCheckResult)>
    CheckDownloadRepeatingCallback;

// Callbacks run on the main thread when a ClientDownloadRequest has
// been formed for a download, or when one has not been formed for a supported
// download.
using ClientDownloadRequestCallbackList =
    base::RepeatingCallbackList<void(download::DownloadItem*,
                                     const ClientDownloadRequest*)>;
using ClientDownloadRequestCallback =
    ClientDownloadRequestCallbackList::CallbackType;

// Callbacks run on the main thread when a FileSystemAccessWriteRequest has been
// formed for a write operation.
using FileSystemAccessWriteRequestCallbackList =
    base::RepeatingCallbackList<void(const ClientDownloadRequest*)>;
using FileSystemAccessWriteRequestCallback =
    FileSystemAccessWriteRequestCallbackList::CallbackType;

// Callbacks run on the main thread when a PPAPI ClientDownloadRequest has been
// formed for a download.
using PPAPIDownloadRequestCallbackList =
    base::RepeatingCallbackList<void(const ClientDownloadRequest*)>;
using PPAPIDownloadRequestCallback =
    PPAPIDownloadRequestCallbackList::CallbackType;

// Given a certificate and its immediate issuer certificate, generates the
// list of strings that need to be checked against the download allowlist to
// determine whether the certificate is allowlisted.
void GetCertificateAllowlistStrings(
    const net::X509Certificate& certificate,
    const net::X509Certificate& issuer,
    std::vector<std::string>* allowlist_strings);

GURL GetFileSystemAccessDownloadUrl(const GURL& frame_url);

// Determine which entries from `src_binaries` should be sent in the download
// ping.
google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
SelectArchiveEntries(const google::protobuf::RepeatedPtrField<
                     ClientDownloadRequest::ArchivedBinary>& src_binaries);

// Identify referrer chain info of a download. This function also
// records UMA stats of download attribution result. The referrer chain
// will include at most `user_gesture_limit` user gestures.
std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
    const download::DownloadItem& item,
    int user_gesture_limit);

// Identify referrer chain info of a File System Access write. This
// function also records UMA stats of download attribution result. The
// referrer chain will include at most `user_gesture_limit` user
// gestures.
std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
    const content::FileSystemAccessWriteItem& item,
    int user_gesture_limit);

#if BUILDFLAG(FULL_SAFE_BROWSING)
// Returns true if dangerous download report should be sent.
bool ShouldSendDangerousDownloadReport(
    download::DownloadItem* item,
    ClientSafeBrowsingReportRequest::ReportType report_type);
#endif

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_UTIL_H_
