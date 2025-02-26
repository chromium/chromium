// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_desktop.h"

#include "base/strings/escape.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "google_apis/google_api_keys.h"

namespace safe_browsing {
namespace {

const char kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

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

bool DownloadProtectionDelegateDesktop::ShouldCheckClientDownload(
    download::DownloadItem* item) const {
  return IsSafeBrowsingEnabledForDownloadProfile(item);
}

bool DownloadProtectionDelegateDesktop::IsSupportedDownload(
    const download::DownloadItem& item,
    const base::FilePath& target_path) const {
  DownloadCheckResultReason ignored_reason = REASON_MAX;
  // TODO(nparker): Remove the CRX check here once can support
  // UNKNOWN types properly.  http://crbug.com/581044
  return CheckClientDownloadRequest::IsSupportedDownload(item, target_path,
                                                         &ignored_reason) &&
         download_type_util::GetDownloadType(target_path) !=
             ClientDownloadRequest::CHROME_EXTENSION;
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

}  // namespace safe_browsing
