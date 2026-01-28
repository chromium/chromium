// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_

#include "base/functional/callback_helpers.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/file_access/scoped_file_access.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#endif  //! BUILDFLAG(IS_ANDROID)

namespace safe_browsing {

// A specialization of `FileAnalysisRequestBase` for analyzing archive files
// (e.g., ZIP, RAR). It uses sandboxed analyzers to check for encryption before
// uploading the file, allowing for an early result for encrypted files.
class FileAnalysisRequest
    : public enterprise_connectors::FileAnalysisRequestBase {
 public:
  FileAnalysisRequest(
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      base::FilePath path,
      base::FilePath file_name,
      std::string mime_type,
      bool delay_opening_file,
      enterprise_connectors::BinaryUploadRequest::ContentAnalysisCallback
          callback,
      enterprise_connectors::BinaryUploadRequest::RequestStartCallback
          start_callback = base::DoNothing(),
      bool is_obfuscated = false,
      bool force_sync_hash_computation = true);
  FileAnalysisRequest(const FileAnalysisRequest&) = delete;
  FileAnalysisRequest& operator=(const FileAnalysisRequest&) = delete;
  ~FileAnalysisRequest() override;

 private:
#if !BUILDFLAG(IS_ANDROID)
  void ProcessZipFile(Data data) override;
  void ProcessRarFile(Data data) override;

  void OnCheckedForEncryption(Data data,
                              const ArchiveAnalyzerResults& analyzer_result);

  // Used to unpack and analyze archives in a sandbox.
  std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>
      zip_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
  std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter>
      rar_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};

#endif

  base::WeakPtrFactory<FileAnalysisRequest> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FILE_ANALYSIS_REQUEST_H_
