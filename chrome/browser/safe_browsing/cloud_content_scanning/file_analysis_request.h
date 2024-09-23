// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/file_access/scoped_file_access.h"

namespace safe_browsing {

// A BinaryUploadService::Request implementation that gets the data to scan
// from the contents of a file. It caches the results so that future calls to
// GetRequestData will return quickly.
class FileAnalysisRequest : public BinaryUploadService::Request {
 public:
  FileAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      base::FilePath path,
      base::FilePath file_name,
      std::string mime_type,
      bool delay_opening_file,
      BinaryUploadService::ContentAnalysisCallback callback,
      BinaryUploadService::Request::RequestStartCallback start_callback =
          base::DoNothing(),
      bool is_obfuscated = false);
  FileAnalysisRequest(const FileAnalysisRequest&) = delete;
  FileAnalysisRequest& operator=(const FileAnalysisRequest&) = delete;
  ~FileAnalysisRequest() override;

  // BinaryUploadService::Request implementation. If |delay_opening_file_| is
  // false, OnGotFileData is called by posting after GetFileDataBlocking runs
  // a base::MayBlock() thread, otherwise the callback will be stored and run
  // later when OpenFile is called.
  void GetRequestData(DataCallback callback) override;

  // Opens the file, reads it, and then calls OnGotFileData on the UI thread.
  // This should be called on a thread with base::MayBlock().
  void OpenFile();

 private:
  void OnGotFileData(
      std::pair<BinaryUploadService::Result, Data> result_and_data);

  void OnCheckedForEncryption(Data data,
                              const ArchiveAnalyzerResults& analyzer_result);

  // Helper functions to access the request proto.
  bool HasMalwareRequest() const;

  void CacheResultAndData(BinaryUploadService::Result result, Data data);

  // Runs |data_callback_|.
  void RunCallback();

  void GetData(file_access::ScopedFileAccess file_access);

  bool has_cached_result_;
  BinaryUploadService::Result cached_result_;
  Data cached_data_;

  // Analysis settings relevant to file analysis requests, copied from the
  // overall analysis settings.
  std::map<std::string, enterprise_connectors::TagSettings> tag_settings_;

  // Path to the file on disk.
  base::FilePath path_;

  // File name excluding the path.
  base::FilePath file_name_;

  DataCallback data_callback_;

  // The file being opened can be delayed so that an external class can have
  // more control on parallelism when multiple files are being opened. If
  // |delay_opening_file_| is false, a task to open the file is posted in the
  // GetRequestData call.
  bool delay_opening_file_;

  // Whether the file contents have been obfuscated during the download process.
  bool is_obfuscated_ = false;

  // Used to unpack and analyze archives in a sandbox.
  std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>
      zip_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
  std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter>
      rar_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};

  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;

  base::WeakPtrFactory<FileAnalysisRequest> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_
