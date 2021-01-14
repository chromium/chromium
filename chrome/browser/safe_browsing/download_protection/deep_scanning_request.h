// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/enterprise/common/proto/connectors.pb.h"

namespace download {
class DownloadItem;
}

namespace safe_browsing {

class DownloadProtectionService;
class DownloadRequestMaker;

// This class encapsulates the process of uploading a file to Safe Browsing for
// deep scanning and reporting the result.
class DeepScanningRequest : public download::DownloadItem::Observer {
 public:
  // Enum representing the trigger of the scan request.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeepScanTrigger {
    // The trigger is unknown.
    TRIGGER_UNKNOWN = 0,

    // The trigger is the prompt in the download shelf, shown for Advanced
    // Protection users.
    TRIGGER_APP_PROMPT = 1,

    // The trigger is the enterprise policy.
    TRIGGER_POLICY = 2,

    kMaxValue = TRIGGER_POLICY,
  };

  // Checks the current policies to determine whether files must be uploaded by
  // policy. Returns the settings to apply to this analysis if it should happen
  // or base::nullopt if no analysis should happen.
  static base::Optional<enterprise_connectors::AnalysisSettings>
  ShouldUploadBinary(download::DownloadItem* item);

  // Scan the given |item|, with the given |trigger|. The result of the scanning
  // will be provided through |callback|. Take references to the owning
  // |download_service| and the |binary_upload_service| to upload to.
  DeepScanningRequest(download::DownloadItem* item,
                      DeepScanTrigger trigger,
                      CheckDownloadRepeatingCallback callback,
                      DownloadProtectionService* download_service,
                      enterprise_connectors::AnalysisSettings settings);

  ~DeepScanningRequest() override;

  // Begin the deep scanning request. This must be called on the UI thread.
  void Start();

 private:
  // Callback when the |download_request_maker_| is finished assembling the
  // download metadata request.
  void OnDownloadRequestReady(
      std::unique_ptr<FileAnalysisRequest> deep_scan_request,
      std::unique_ptr<ClientDownloadRequest> download_request);

  // Callbacks for when |binary_upload_service_| finishes uploading.
  void OnScanComplete(BinaryUploadService::Result result,
                      enterprise_connectors::ContentAnalysisResponse response);

  // Finishes the request, providing the result through |callback_| and
  // notifying |download_service_|.
  void FinishRequest(DownloadCheckResult result);

  // Callback when |item_| is destroyed.
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Called to attempt to show the modal dialog for scan failure. Returns
  // whether the dialog was successfully shown.
  bool MaybeShowDeepScanFailureModalDialog(base::OnceClosure accept_callback,
                                           base::OnceClosure cancel_callback,
                                           base::OnceClosure open_now_callback);

  // Called to open the download. This is triggered by the timeout modal dialog.
  void OpenDownload();

  // Populates a request with the appropriate data depending on the used proto.
  void PrepareRequest(std::unique_ptr<FileAnalysisRequest> deep_scan_request,
                      Profile* profile);

  // The download item to scan. This is unowned, and could become nullptr if the
  // download is destroyed.
  download::DownloadItem* item_;

  // The reason for deep scanning.
  DeepScanTrigger trigger_;

  // The callback to provide the scan result to.
  CheckDownloadRepeatingCallback callback_;

  // The download protection service that initiated this upload. The
  // |download_service_| owns this class.
  DownloadProtectionService* download_service_;

  // The time when uploading starts.
  base::TimeTicks upload_start_time_;

  // The settings to apply to this scan.
  enterprise_connectors::AnalysisSettings analysis_settings_;

  // Used to assemble the download metadata.
  std::unique_ptr<DownloadRequestMaker> download_request_maker_;

  base::WeakPtrFactory<DeepScanningRequest> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
