// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "chrome/browser/enterprise/connectors/common.h"
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
// is enabled for a pair of filesystem urls using `IsEnabledVec()`.
// Note: `IsEnabledVec()` allows checking for multiple source urls at once. In
// this case, a user has to create a FileTransferAnalysisDelegate for each
// source url.
//
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
  using FileTransferAnalysisDelegateFactory = base::RepeatingCallback<
      std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>(
          safe_browsing::DeepScanAccessPoint access_point,
          storage::FileSystemURL source_url,
          storage::FileSystemURL destination_url,
          Profile* profile,
          storage::FileSystemContext* file_system_context,
          enterprise_connectors::AnalysisSettings settings)>;

  enum FileTransferAnalysisResult {
    RESULT_ALLOWED,
    RESULT_BLOCKED,
    RESULT_UNKNOWN,
  };

  ~FileTransferAnalysisDelegate() override;

  // Create the FileTransferAnalysisDelegate. This function uses the factory if
  // it is set via `SetFactorForTesting()`.
  //
  // For `block_until_verdict == 0`, the `destination_url` has to point to the
  // copied file/directory and not its parent. If it points to the parent, all
  // files within the destination directory are scanned.
  static std::unique_ptr<FileTransferAnalysisDelegate> Create(
      safe_browsing::DeepScanAccessPoint access_point,
      storage::FileSystemURL source_url,
      storage::FileSystemURL destination_url,
      Profile* profile,
      storage::FileSystemContext* file_system_context,
      AnalysisSettings settings);

  // Set a factory for the FileTransferAnalysisDelegate.
  // Can be used in testing to create `MockFileTransferAnalysisDelegate`s.
  static void SetFactorForTesting(FileTransferAnalysisDelegateFactory factory);

  // Returns a vector with the AnalysisSettings for file transfers from the
  // respective source url to the destination_url.
  // If the enterprise connectors are not enabled for any of the transfers an
  // empty vector is returned. Each entry in the returned vector corresponds to
  // the entry in the `source_urls` vector with the same index.
  static std::vector<absl::optional<AnalysisSettings>> IsEnabledVec(
      Profile* profile,
      const std::vector<storage::FileSystemURL>& source_urls,
      storage::FileSystemURL destination_url);

  // Main entrypoint to start the file uploads.
  // Once scanning is complete `callback_` will be called.
  virtual void UploadData(base::OnceClosure completion_callback);

  // Calling this function is only allowed after the scan is complete!
  virtual FileTransferAnalysisResult GetAnalysisResultAfterScan(
      storage::FileSystemURL url);
  // Calling this function is only allowed after the scan is complete!
  virtual std::vector<storage::FileSystemURL> GetWarnedFiles() const;

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

 protected:
  // For `block_until_verdict == 0`, the `destination_url` has to point to the
  // copied file/directory and not its parent. If it points to the parent, all
  // files within the destination directory are scanned.
  FileTransferAnalysisDelegate(safe_browsing::DeepScanAccessPoint access_point,
                               storage::FileSystemURL source_url,
                               storage::FileSystemURL destination_url,
                               Profile* profile,
                               storage::FileSystemContext* file_system_context,
                               AnalysisSettings settings);

 private:
  void OnGotFileURLs(std::vector<storage::FileSystemURL> source_urls);

  void ContentAnalysisCompleted(std::vector<RequestHandlerResult> results);

  AnalysisSettings settings_;
  raw_ptr<Profile, ExperimentalAsh> profile_;
  safe_browsing::DeepScanAccessPoint access_point_;
  std::vector<storage::FileSystemURL> scanning_urls_;
  storage::FileSystemURL source_url_;
  storage::FileSystemURL destination_url_;
  base::OnceClosure callback_;
  std::vector<RequestHandlerResult> results_;
  std::vector<size_t> warned_file_indices_;
  bool warning_is_bypassed_{false};

  std::unique_ptr<storage::RecursiveOperationDelegate> get_file_urls_delegate_;
  std::unique_ptr<FilesRequestHandler> request_handler_;

  base::WeakPtrFactory<FileTransferAnalysisDelegate> weak_ptr_factory_{this};
};
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FILE_TRANSFER_ANALYSIS_DELEGATE_H_
