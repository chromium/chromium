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
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_opening_job.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"
#include "components/file_access/scoped_file_access.h"

class Profile;

namespace enterprise_connectors {

class FileAnalysisRequestBase;

// Implementation of `FilesRequestHandlerBase::Delegate` for Clank and desktop
// platforms.
class FilesRequestHandler : public FilesRequestHandlerBase::Delegate {
 public:
  // Callback that informs caller of scanning verdicts for each file.
  using CompletionCallback =
      base::OnceCallback<void(std::vector<RequestHandlerResult>)>;

  // A factory function used in tests to create fake FilesRequestHandlerBase
  // instances.
  using Factory =
      base::RepeatingCallback<std::unique_ptr<FilesRequestHandlerBase>(
          ContentAnalysisInfo* content_analysis_info,
          BinaryUploadService* upload_service,
          Profile* profile,
          GURL url,
          const std::string& source,
          const std::string& destination,
          const std::string& content_transfer_method,
          DeepScanAccessPoint access_point,
          const std::vector<base::FilePath>& paths,
          CompletionCallback callback)>;

  // Create an instance of FilesRequestHandlerBase. If a factory is set, it will
  // be used instead.
  // Note that `analysis_settings` is saved as const reference and not copied.
  // The calling side is responsible that `analysis_settings` is not destroyed
  // before scanning is completed.
  static std::unique_ptr<FilesRequestHandlerBase> Create(
      ContentAnalysisInfo* content_analysis_info,
      BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& content_transfer_method,
      DeepScanAccessPoint access_point,
      const std::vector<base::FilePath>& paths,
      CompletionCallback callback);

  // In tests, sets a factory function for creating fake FilesRequestHandlers.
  static void SetFactoryForTesting(Factory factory);
  static void ResetFactoryForTesting();

  FilesRequestHandler(Profile* profile,
                      const std::string& source,
                      const std::string& destination,
                      const std::vector<base::FilePath>& paths,
                      CompletionCallback callback);

  ~FilesRequestHandler() override;

  // FilesRequestHandlerBase::Delegate overrides:
  std::unique_ptr<FileAnalysisRequestBase> CreateFileRequest(
      size_t index,
      const AnalysisSettings& settings,
      base::OnceCallback<void(ScanRequestUploadResult, ContentAnalysisResponse)>
          callback,
      base::OnceCallback<void(const BinaryUploadRequest&)>
          request_start_callback) override;
  void ReportWarningBypass(std::optional<std::u16string> user_justification,
                           const ContentAnalysisInfoBase& info,
                           const std::string& trigger,
                           const std::string& content_transfer_method) override;
  bool UploadDataImpl() override;
  void UpdateRequestHandlerResult(size_t index,
                                  RequestHandlerResult result,
                                  ContentAnalysisResponse response) override;
  const base::FilePath& GetPath(size_t index) const override;
  const FilesRequestHandlerBase::FileInfo& GetFileInfo(size_t index) override;
  FilesRequestHandlerBase::FileInfo& GetMutableFileInfo(size_t index) override;
  size_t GetFileCount() const override;
  void SetFileScanStartTime(size_t index) override;
  const base::TimeTicks GetFileScanStartTime(size_t index) override;
  ReportingEventRouter* GetReportingEventRouter() override;
  void MaybeCompleteScanRequest() override;
  std::string GetSource() override;
  std::string GetDestination() override;
  void SetHandler(FilesRequestHandlerBase* handler) override;
  void MaybeCancelAndReport() override;
  void MarkFileAsReported(size_t index) override;

 private:
  void MaybeTrackCancellation();

  void CreateFileOpeningJob(
      std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks,
      file_access::ScopedFileAccess file_access);

  // Constructs and owns a refcount to FileOpeningJob responsible for opening
  // files on parallel threads. Always nullptr for non-file content scanning.
  scoped_refptr<safe_browsing::FileOpeningJob> file_opening_job_;

  raw_ptr<FilesRequestHandlerBase> handler_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  std::vector<base::FilePath> paths_;
  std::vector<FilesRequestHandlerBase::FileInfo> file_info_;
  std::vector<RequestHandlerResult> results_;

  // Scanning responses of files that got DLP warning verdicts.
  std::map<size_t, ContentAnalysisResponse> file_warnings_;

  std::string source_;
  std::string destination_;
  CompletionCallback callback_;

  std::vector<base::TimeTicks> start_times_;

  // Set of file indices that have not yet been reported. This is used to
  // determine which files to report if cancellation detected during the
  // destruction of this object. Usually (99% of the time) this will be empty or
  // contain few elements. In rare cases (e.g., user by mistake triggers
  // uploading of thousands of files) this set will contain many elements.
  std::unordered_set<size_t> unreported_files_;

  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;

  base::WeakPtrFactory<FilesRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILES_REQUEST_HANDLER_H_
