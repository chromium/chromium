// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/supports_user_data.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/download_manager_delegate.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"  // nogncheck crbug.com/40147906
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace enterprise_connectors {

// User data to persist a save package's final callback allowing/denying
// completion. This is used since the callback can be called either when
// scanning completes on a block/allow verdict, when the user cancels the scan,
// or when the user bypasses scanning.
struct SavePackageScanningData : public base::SupportsUserData::Data {
  explicit SavePackageScanningData(
      content::SavePackageAllowedCallback callback);
  ~SavePackageScanningData() override;
  static const char kKey[];

  content::SavePackageAllowedCallback callback;
};

policy::BrowserPolicyConnector* GetBrowserPolicyConnector();

// Checks `item` for a SavePackageScanningData, and run it's callback with
// `allowed` if there is one.
void RunSavePackageScanningCallback(download::DownloadItem* item, bool allowed);

// Returns whether device info should be reported for the profile.
bool IncludeDeviceInfo(Profile* profile, bool per_profile);

// Returns the email address of the unconsented account signed in to the profile
// or an empty string if no account is signed in.  If `profile` is null then the
// empty string is returned.
std::string GetProfileEmail(Profile* profile);

// Returns the list of URLs from the current frame all the way to the outermost
// frame URL. Above the `kMaxFrameUrls` limit, we skip the rest of the chain and
// take the outermost URL for performance considerations.
google::protobuf::RepeatedPtrField<std::string> CollectFrameUrls(
    content::WebContents* web_contents,
    DeepScanAccessPoint access_point);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)

// Returns the appropriate BinaryUploadService for the given `profile` and
// `settings`. This can be a cloud or local service.
BinaryUploadService* GetBinaryUploadServiceForConnector(
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& settings);

#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
// Returns whether the download danger type implies the user should be allowed
// to review the download.
bool ShouldPromptReviewForDownload(Profile* profile,
                                   const download::DownloadItem* download_item);

// Shows the review dialog after a user has clicked the "Review" button
// corresponding to a download.
void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
