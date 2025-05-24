// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_desktop.h"

#include "base/strings/escape.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/file_system_access_write_item.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"

namespace safe_browsing {
namespace {

const char kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

// Content-Type HTTP header field for the request.
const char kCheckClientDownloadRequestContentType[] =
    "application/octet-stream";

// We sample 1% of allowlisted downloads to still send out download pings.
const double kAllowlistDownloadSampleRate = 0.01;

// The API key should not change so we can construct this GURL once and cache
// it.
GURL ConstructDownloadRequestUrl() {
  GURL url(kDownloadRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    url = url.Resolve("?key=" +
                      base::EscapeQueryParamValue(api_key, /*use_plus=*/true));
  }
  return url;
}

bool IsSafeBrowsingEnabledForDownloadProfile(download::DownloadItem* item) {
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  return profile && IsSafeBrowsingEnabled(*profile->GetPrefs());
}

}  // namespace

DownloadProtectionDelegateDesktop::DownloadProtectionDelegateDesktop()
    : download_request_url_(ConstructDownloadRequestUrl()) {
  CHECK(download_request_url_.is_valid());
}

DownloadProtectionDelegateDesktop::~DownloadProtectionDelegateDesktop() =
    default;

bool DownloadProtectionDelegateDesktop::ShouldCheckDownloadUrl(
    download::DownloadItem* item) const {
  return IsSafeBrowsingEnabledForDownloadProfile(item);
}

bool DownloadProtectionDelegateDesktop::MayCheckClientDownload(
    download::DownloadItem* item) const {
  if (!IsSafeBrowsingEnabledForDownloadProfile(item)) {
    return false;
  }
  return IsSupportedDownload(*item, item->GetTargetFilePath()) !=
         MayCheckDownloadResult::kMayNotCheckDownload;
}

bool DownloadProtectionDelegateDesktop::MayCheckFileSystemAccessWrite(
    content::FileSystemAccessWriteItem* item) const {
  Profile* profile = Profile::FromBrowserContext(item->browser_context);
  if (!profile || !IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return false;
  }
  DownloadCheckResultReason ignored_reason = REASON_MAX;
  return CheckFileSystemAccessWriteRequest::IsSupportedDownload(
             item->target_file_path, &ignored_reason) !=
         MayCheckDownloadResult::kMayNotCheckDownload;
}

MayCheckDownloadResult DownloadProtectionDelegateDesktop::IsSupportedDownload(
    download::DownloadItem& item,
    const base::FilePath& target_path) const {
  // TODO(nparker): Remove the CRX check here once can support
  // UNKNOWN types properly.  http://crbug.com/581044
  if (download_type_util::GetDownloadType(target_path) ==
      ClientDownloadRequest::CHROME_EXTENSION) {
    return MayCheckDownloadResult::kMayNotCheckDownload;
  }
  DownloadCheckResultReason ignored_reason = REASON_MAX;
  return CheckClientDownloadRequest::IsSupportedDownload(item, target_path,
                                                         &ignored_reason);
}

void DownloadProtectionDelegateDesktop::FinalizeResourceRequest(
    network::ResourceRequest& resource_request) {
  resource_request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                     kCheckClientDownloadRequestContentType);
}

const GURL& DownloadProtectionDelegateDesktop::GetDownloadRequestUrl() const {
  return download_request_url_;
}

net::NetworkTrafficAnnotationTag DownloadProtectionDelegateDesktop::
    CompleteClientDownloadRequestTrafficAnnotation(
        const net::PartialNetworkTrafficAnnotationTag&
            partial_traffic_annotation) const {
  return net::BranchedCompleteNetworkTrafficAnnotation(
      "client_download_request_desktop", "client_download_request_for_platform",
      partial_traffic_annotation, R"(
          semantics {
            description:
              "Chromium checks whether a given download is likely to be "
              "dangerous by sending this client download request to Google's "
              "Safe Browsing servers. Safe Browsing server will respond to "
              "this request by sending back a verdict, indicating if this "
              "download is safe or the danger type of this download (e.g. "
              "dangerous content, uncommon content, potentially harmful, etc)."
            trigger:
              "This request is triggered when a download is about to complete, "
              "the download is not allowlisted, and its file extension is "
              "supported by download protection service (e.g. executables, "
              "archives). Please refer to https://cs.chromium.org/chromium/src/"
              "chrome/browser/resources/safe_browsing/"
              "download_file_types.asciipb for the complete list of supported "
              "files."
            data:
              "URL of the file to be downloaded, its referrer chain, digest "
              "and other features extracted from the downloaded file. Refer to "
              "ClientDownloadRequest message in https://cs.chromium.org/"
              "chromium/src/components/safe_browsing/csd.proto for all "
              "submitted features."
            user_data {
              type: SENSITIVE_URL
              type: WEB_CONTENT
            }
            last_reviewed: "2025-02-25"
          })");
}

float DownloadProtectionDelegateDesktop::GetAllowlistedDownloadSampleRate()
    const {
  return kAllowlistDownloadSampleRate;
}

float DownloadProtectionDelegateDesktop::GetUnsupportedFileSampleRate(
    const base::FilePath& filename) const {
  // If this extension is specifically marked as SAMPLED_PING (as are
  // all "unknown" extensions), we may want to sample it. Sampling it means
  // we'll send a "light ping" with private info removed, and we won't
  // use the verdict.
  const FileTypePolicies* policies = FileTypePolicies::GetInstance();
  return policies->PingSettingForFile(filename) ==
                 DownloadFileType::SAMPLED_PING
             ? policies->SampledPingProbability()
             : 0.0;
}

}  // namespace safe_browsing
