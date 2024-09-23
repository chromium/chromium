// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/supports_user_data.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

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

// Checks `item` for a SavePackageScanningData, and run it's callback with
// `allowed` if there is one.
void RunSavePackageScanningCallback(download::DownloadItem* item, bool allowed);

// Returns whether device info should be reported for the profile.
bool IncludeDeviceInfo(Profile* profile, bool per_profile);

// Returns whether the download danger type implies the user should be allowed
// to review the download.
bool ShouldPromptReviewForDownload(Profile* profile,
                                   const download::DownloadItem* download_item);

// Returns the email address of the unconsented account signed in to the profile
// or an empty string if no account is signed in.  If either `profile` or
// `identity_manager` is null then the empty string is returned.
std::string GetProfileEmail(Profile* profile);
std::string GetProfileEmail(signin::IdentityManager* identity_manager);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
// Shows the review dialog after a user has clicked the "Review" button
// corresponding to a download.
void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure);

// Calculates the result for the request handler based on the upload result and
// the analysis response.
RequestHandlerResult CalculateRequestHandlerResult(
    const AnalysisSettings& settings,
    safe_browsing::BinaryUploadService::Result upload_result,
    const ContentAnalysisResponse& response);

// Determines if a request result should be used to allow a data use or to
// block it.
bool ResultShouldAllowDataUse(
    const AnalysisSettings& settings,
    safe_browsing::BinaryUploadService::Result upload_result);

// Calculates the event result that is experienced by the user.
// If data is allowed to be accessed immediately, the result will indicate that
// the user was allowed to use the data independent of the scanning result.
safe_browsing::EventResult CalculateEventResult(
    const AnalysisSettings& settings,
    bool allowed_by_scan_result,
    bool should_warn);

// Returns true if the request will use the scotty resumable upload
// protocol for sending scans to the server.
bool IsResumableUpload(
    const safe_browsing::BinaryUploadService::Request& request);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a cloud-based multipart content analysis.
bool CloudMultipartResultIsFailure(
    safe_browsing::BinaryUploadService::Result result);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a cloud-based resumable content analysis.
bool CloudResumableResultIsFailure(
    safe_browsing::BinaryUploadService::Result result,
    bool block_large_files,
    bool block_password_protected_files);

// Returns true if `result` as returned by BinaryUploadService is considered a
// a failed result when attempting a local content analysis.
bool LocalResultIsFailure(safe_browsing::BinaryUploadService::Result result);

// Returns true if `result` as returned by BinaryUploadService is considered a
// fail-closed result, regardless of attempting a cloud-based or a local-based
// content analysis.
bool ResultIsFailClosed(safe_browsing::BinaryUploadService::Result result);
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfileLacros();
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Constants used to build the report of a data masking event.
inline constexpr char kKeyDetectorId[] = "detectorId";
inline constexpr char kKeyDisplayName[] = "displayName";
inline constexpr char kKeyDetectorType[] = "detectorType";
inline constexpr char kKeyMatchedDetectors[] = "matchedDetectors";

// Helper function to report events for the
// "chrome.enterprise.reportingPrivate.reportingDataMaskingEvent" extension
// API. It does nothing if reporting is not available.
void ReportDataMaskingEvent(
    content::BrowserContext* browser_context,
    extensions::api::enterprise_reporting_private::DataMaskingEvent
        data_masking_event);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_COMMON_H_
