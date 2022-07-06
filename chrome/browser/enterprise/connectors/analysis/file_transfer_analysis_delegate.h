// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace storage {
class FileSystemContext;
class RecursiveOperationDelegate;
}  // namespace storage

namespace enterprise_connectors {

class FilesRequestHandler;

// `FileTransferAnalysisDelegate` handles scanning and reporting of ChromeOS
// file system transfers.
// A user of `FileTransferAnalysisDelegate` should first check whether scanning
// is enabled for a pair of filesystem urls using `IsEnabled()`.
// If scanning is enabled, a user proceeds with the creation of the class and
// then calls `UploadData()` to start the scan. Once the scans are complete,
// `callback_` is run.
// After a completed scan, a user of `FileTransferAnalysisDelegate` can access
// the scanning results for different filesystem urls using
// `GetAnalysisResult()`.
//
// If `source_url` is a directory, all files contained within the directory or
// any descended directory will be scanned. If `source_url` is a file only that
// file will be scanned.
class FileTransferAnalysisDelegate : public ContentAnalysisDelegateBase {
 public:
  enum FileTransferAnalysisResult {
    RESULT_ALLOWED,
    RESULT_BLOCKED,
    RESULT_UNKNOWN,
  };

  ~FileTransferAnalysisDelegate() override;

  static absl::optional<AnalysisSettings> IsEnabled(
      Profile* profile,
      storage::FileSystemURL source_url,
      storage::FileSystemURL destination_url);

  FileTransferAnalysisDelegate(safe_browsing::DeepScanAccessPoint access_point,
                               storage::FileSystemURL source_url,
                               storage::FileSystemURL destination_url,
                               Profile* profile,
                               storage::FileSystemContext* file_system_context,
                               AnalysisSettings settings,
                               base::OnceClosure result_callback);

  // Main entrypoint to start the file uploads.
  // Once scanning is complete `callback_` will be called.
  void UploadData();

  // Calling this function is only allowed after the scan is complete!
  FileTransferAnalysisResult GetAnalysisResultAfterScan(
      storage::FileSystemURL url);

  // ContentAnalysisDelegateBase:
  void BypassWarnings(
      absl::optional<std::u16string> user_justification) override;
  void Cancel(bool warning) override;
  absl::optional<std::u16string> GetCustomMessage() const override;
  absl::optional<GURL> GetCustomLearnMoreUrl() const override;
  bool BypassRequiresJustification() const override;
  std::u16string GetBypassJustificationLabel() const override;
  absl::optional<std::u16string> OverrideCancelButtonText() const override;

  FilesRequestHandler* GetFilesRequestHandlerForTesting();

 private:
  void OnGotFileSourceURLs(std::vector<storage::FileSystemURL> source_urls);

  void ContentAnalysisCompleted(std::vector<RequestHandlerResult> results);

  AnalysisSettings settings_;
  Profile* profile_;
  safe_browsing::DeepScanAccessPoint access_point_;
  std::vector<storage::FileSystemURL> source_urls_;
  storage::FileSystemURL destination_url_;
  base::OnceClosure callback_;
  std::vector<RequestHandlerResult> results_;

  std::unique_ptr<storage::RecursiveOperationDelegate> get_file_urls_delegate_;
  std::unique_ptr<FilesRequestHandler> request_handler_;

  base::WeakPtrFactory<FileTransferAnalysisDelegate> weak_ptr_factory_{this};
};
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
