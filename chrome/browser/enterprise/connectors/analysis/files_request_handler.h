// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILES_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILES_REQUEST_HANDLER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/file_access/scoped_file_access.h"

namespace safe_browsing {

class FileAnalysisRequest;
class FileOpeningJob;

}  // namespace safe_browsing

namespace enterprise_connectors {

// Handles deep scanning requests for multiple files which are specified by
// `paths_`. Files are scanned in parallel and piped to the BinaryUploadService
// `upload_service_`. On completion of the scan, `callback_` is called with the
// scanning results. After the scanning is complete, ReportWarningBypass can be
// called to report a warning bypass of all warned files.
class FilesRequestHandler : public RequestHandlerBase {
 public:
  // File information used as an input to event report functions.
  struct FileInfo {
    FileInfo();
    FileInfo(FileInfo&& other);
    ~FileInfo();

    // Hex-encoded SHA256 hash for the given file.
    std::string sha256;

    // File size in bytes. -1 represents an unknown size.
    uint64_t size = 0;

    // File mime type.
    std::string mime_type;
  };

  // Callback that informs caller of scanning verdicts for each file.
  using CompletionCallback =
      base::OnceCallback<void(std::vector<RequestHandlerResult>)>;

  // A factory function used in tests to create fake FilesRequestHandler
  // instances.
  using Factory = base::RepeatingCallback<std::unique_ptr<FilesRequestHandler>(
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& user_action_id,
      const std::string& tab_title,
      const std::string& content_transfer_method,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentAnalysisRequest::Reason reason,
      const std::vector<base::FilePath>& paths,
      CompletionCallback callback)>;

  // Create an instance of FilesRequestHandler. If a factory is set, it will be
  // used instead.
  // Note that `analysis_settings` is saved as const reference and not copied.
  // The calling side is responsible that `analysis_settings` is not destroyed
  // before scanning is completed.
  static std::unique_ptr<FilesRequestHandler> Create(
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& user_action_id,
      const std::string& tab_title,
      const std::string& content_transfer_method,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentAnalysisRequest::Reason reason,
      const std::vector<base::FilePath>& paths,
      CompletionCallback callback);

  // In tests, sets a factory function for creating fake FilesRequestHandlers.
  static void SetFactoryForTesting(Factory factory);
  static void ResetFactoryForTesting();

  ~FilesRequestHandler() override;

  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

 protected:
  FilesRequestHandler(
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& user_action_id,
      const std::string& tab_title,
      const std::string& content_transfer_method,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentAnalysisRequest::Reason reason,
      const std::vector<base::FilePath>& paths,
      CompletionCallback callback);

  bool UploadDataImpl() override;

  void FileRequestCallbackForTesting(
      base::FilePath path,
      safe_browsing::BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

 private:
  // Prepares an upload request for the file at `path`.  If the file
  // cannot be uploaded it will have a failure verdict added to `result_`.
  safe_browsing::FileAnalysisRequest* PrepareFileRequest(size_t index);

  // Called when the file info for `path` has been fetched. Also begins the
  // upload process.
  void OnGotFileInfo(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      size_t index,
      safe_browsing::BinaryUploadService::Result result,
      safe_browsing::BinaryUploadService::Request::Data data);

  // Called when a request is finished early without uploading it.
  // This is, e.g., called for encrypted files and responsible for posting the
  // required data to safe-browsing ui.
  void FinishRequestEarly(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      safe_browsing::BinaryUploadService::Result result);

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  // The `result` argument exists as an optimization to finish the request early
  // when the result is known in advance to avoid using the upload service.
  virtual void UploadFileForDeepScanning(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request);

  void FileRequestCallback(
      size_t index,
      safe_browsing::BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  void FileRequestStartCallback(
      size_t index,
      const safe_browsing::BinaryUploadService::Request& request);

  void MaybeCompleteScanRequest();

  void CreateFileOpeningJob(
      std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks,
      file_access::ScopedFileAccess file_access);

  // Owner of the FileOpeningJob responsible for opening files on parallel
  // threads. Always nullptr for non-file content scanning.
  std::unique_ptr<safe_browsing::FileOpeningJob> file_opening_job_;

  std::vector<base::FilePath> paths_;
  std::vector<FileInfo> file_info_;

  // The number of file scans that have completed. If more than one file is
  // requested for scanning in `data_`, each is scanned in parallel with
  // separate requests.
  size_t file_result_count_ = 0;

  std::vector<RequestHandlerResult> results_;

  // Scanning responses of files that got DLP warning verdicts.
  std::map<size_t, enterprise_connectors::ContentAnalysisResponse>
      file_warnings_;

  // This is set to true as soon as a TOO_MANY_REQUESTS response is obtained. No
  // more data should be upload for `this` at that point.
  bool throttled_ = false;

  std::string content_transfer_method_;

  CompletionCallback callback_;

  std::vector<base::TimeTicks> start_times_;

  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;

  base::WeakPtrFactory<FilesRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILES_REQUEST_HANDLER_H_
