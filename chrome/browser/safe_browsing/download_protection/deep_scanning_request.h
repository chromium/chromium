// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_

#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"

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
  // Enum representing the type of constructor that initiated scanning.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeepScanType {
    // Scanning was initiated by a normal download from a web page.
    NORMAL = 0,

    // Scanning was initiated by a save package being saved on disk.
    SAVE_PACKAGE = 1,

    kMaxValue = SAVE_PACKAGE,
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the DeepScanningRequest finishes.
    virtual void OnFinish(DeepScanningRequest* request) {}
  };

  // Checks the current policies to determine whether files must be uploaded by
  // policy. Returns the settings to apply to this analysis if it should happen
  // or std::nullopt if no analysis should happen.
  static std::optional<enterprise_connectors::AnalysisSettings>
  ShouldUploadBinary(download::DownloadItem* item);

  // Scan the given `item`, with the given `trigger`. The result of the scanning
  // will be provided through `callback`. Take a references to the owning
  // `download_service`.
  DeepScanningRequest(download::DownloadItem* item,
                      DownloadItemWarningData::DeepScanTrigger trigger,
                      DownloadCheckResult pre_scan_download_check_result,
                      CheckDownloadRepeatingCallback callback,
                      DownloadProtectionService* download_service,
                      enterprise_connectors::AnalysisSettings settings,
                      base::optional_ref<const std::string> password);

  // Scan the given `item` that corresponds to a save package, with
  // `save_package_page` mapping every currently on-disk file part of that
  // package to their final target path. The result of the scanning is provided
  // through `callback` once every file has been scanned, and the given result
  // is the highest severity one. Takes a reference to the owning
  // `download_service`.
  DeepScanningRequest(
      download::DownloadItem* item,
      DownloadCheckResult pre_scan_download_check_result,
      CheckDownloadRepeatingCallback callback,
      DownloadProtectionService* download_service,
      enterprise_connectors::AnalysisSettings settings,
      base::flat_map<base::FilePath, base::FilePath> save_package_files);

  ~DeepScanningRequest() override;

  // Begin the deep scanning request. This must be called on the UI thread.
  void Start();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadDestroyed(download::DownloadItem* download) override;

 private:
  // Starts the deep scanning request when there is a one-to-one mapping from
  // the download item to a file.
  void StartSingleFileScan();

  // Starts the deep scanning requests when there is a one-to-many mapping from
  // the download item to multiple files being scanned as a part of a save
  // package.
  void StartSavePackageScan();

  // Callback when the |download_request_maker_| is finished assembling the
  // download metadata request.
  void OnDownloadRequestReady(
      const base::FilePath& current_path,
      std::unique_ptr<FileAnalysisRequest> deep_scan_request,
      std::unique_ptr<ClientDownloadRequest> download_request);

  // Callbacks for when |binary_upload_service_| finishes uploading.
  void OnScanComplete(const base::FilePath& current_path,
                      BinaryUploadService::Result result,
                      enterprise_connectors::ContentAnalysisResponse response);
  void OnConsumerScanComplete(
      const base::FilePath& current_path,
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);
  void OnEnterpriseScanComplete(
      const base::FilePath& current_path,
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  // Called when a single file scanning request has completed. Calls
  // FinishRequest if it was the last required one.
  void MaybeFinishRequest(DownloadCheckResult result);

  // Finishes the request, providing the result through |callback_| and
  // notifying |download_service_|.
  void FinishRequest(DownloadCheckResult result);

  // Called to attempt to show the modal dialog for scan failure. Returns
  // whether the dialog was successfully shown.
  bool MaybeShowDeepScanFailureModalDialog(base::OnceClosure accept_callback,
                                           base::OnceClosure cancel_callback,
                                           base::OnceClosure close_callback,
                                           base::OnceClosure open_now_callback);

  // Called to verify if `result` is considered as a failure and the scan should
  // end early.
  bool ShouldTerminateEarly(BinaryUploadService::Result result);

  // Called to open the download. This is triggered by the timeout modal dialog.
  void OpenDownload();

  // Populates a request's proto fields with the appropriate data.
  void PopulateRequest(FileAnalysisRequest* request,
                       Profile* profile,
                       const base::FilePath& path);

  // Creates a ClientDownloadRequest asynchronously to attach to
  // `deep_scan_request`. Once it is obtained, OnDownloadRequestReady is called
  // to upload the request for deep scanning.
  void PrepareClientDownloadRequest(
      const base::FilePath& current_path,
      std::unique_ptr<FileAnalysisRequest> deep_scan_request);

  // Callback invoked in `StartSingleFileScan` to check if `data` has been
  // successfully fetched and ready for deep scanning if needed.
  void OnGetFileRequestData(const base::FilePath& file_path,
                            std::unique_ptr<FileAnalysisRequest> request,
                            BinaryUploadService::Result result,
                            BinaryUploadService::Request::Data data);

  // Callback invoked in `StartSavePackageScan` to check if `data` of a file in
  // package has been successfully fetched and ready for deep scanning if
  // needed.
  void OnGetPackageFileRequestData(const base::FilePath& final_path,
                                   const base::FilePath& current_path,
                                   std::unique_ptr<FileAnalysisRequest> request,
                                   BinaryUploadService::Result result,
                                   BinaryUploadService::Request::Data data);

  // Helper function to simplify checking if the report-only feature is set in
  // conjunction with the corresponding policy value.
  bool ReportOnlyScan();

  // Acknowledge the request's handling to the service provider.
  void AcknowledgeRequest(EventResult event_result);

  bool IsEnterpriseTriggered() const;
  bool IsConsumerTriggered() const;

  // Callback for when deobfuscation of the file is completed.
  void OnDeobfuscationComplete(
      DownloadCheckResult download_result,
      base::expected<void, enterprise_obfuscation::Error> result);

  // Provides scan result to `callback_` and clean up.
  void CallbackAndCleanup(DownloadCheckResult result);

  // The download item to scan. This is unowned, and could become nullptr if the
  // download is destroyed.
  raw_ptr<download::DownloadItem> item_;

  // The reason for deep scanning.
  DownloadItemWarningData::DeepScanTrigger trigger_;

  // The callback to provide the scan result to.
  CheckDownloadRepeatingCallback callback_;

  // The download protection service that initiated this upload. The
  // |download_service_| owns this class.
  raw_ptr<DownloadProtectionService> download_service_;

  // The time when uploading starts. Keyed with the file's current path.
  base::flat_map<base::FilePath, base::TimeTicks> upload_start_times_;

  // The settings to apply to this scan.
  enterprise_connectors::AnalysisSettings analysis_settings_;

  // Used to assemble the download metadata.
  std::unique_ptr<DownloadRequestMaker> download_request_maker_;

  // This list of observers of this request.
  base::ObserverList<Observer> observers_;

  // Stores a mapping of temporary paths to final paths for save package files.
  // This is empty on non-page save scanning requests.
  base::flat_map<base::FilePath, base::FilePath> save_package_files_;

  // Stores a mapping of a file's current path to its metadata so it can be used
  // in reporting events. This is populated from opening the file for save
  // package scans, or populated from `item_` for single file scans.
  base::flat_map<base::FilePath, enterprise_connectors::FileMetadata>
      file_metadata_;

  // Owner of the FileOpeningJob used to safely open multiple files in parallel
  // for save package scans. Always nullptr for non-save package scans.
  std::unique_ptr<FileOpeningJob> file_opening_job_;

  // The total number of files beings scanned for which OnScanComplete hasn't
  // been called. Once this is 0, FinishRequest should be called and `this`
  // should be destroyed.
  size_t pending_scan_requests_;

  // The highest precedence DownloadCheckResult obtained from scanning verdicts.
  // This should be updated when a scan completes.
  DownloadCheckResult download_check_result_ =
      DownloadCheckResult::DEEP_SCANNED_SAFE;

  // Cached SB result for the download to be used if deep scanning fails.
  DownloadCheckResult pre_scan_download_check_result_;

  // Cached danger type for the download to be used by reporting in case
  // scanning is skipped for any reason.
  download::DownloadDangerType pre_scan_danger_type_;

  // Set to true when StartSingleFileScan or StartSavePackageScan is called and
  // that scanning has started. This is used so that calls to OnDownloadUpdated
  // only ever start the scanning process once.
  bool scanning_started_ = false;

  // Cached callbacks to report scanning results until the final `event_result_`
  // is known. The callbacks in this list should be called in FinishRequest.
  base::OnceCallbackList<void(EventResult result)> report_callbacks_;

  // The request tokens of all the requests that make up the user action
  // represented by this ContentAnalysisDelegate instance.
  std::vector<std::string> request_tokens_;

  // Password for the file, if it's an archive.
  std::optional<std::string> password_;

  // Reason the scanning took place. Used to populate enterprise requests to
  // give more context on what user action lead to a scan.
  enterprise_connectors::ContentAnalysisRequest::Reason reason_ =
      enterprise_connectors::ContentAnalysisRequest::UNKNOWN;

  base::WeakPtrFactory<DeepScanningRequest> weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DEEP_SCANNING_REQUEST_H_
