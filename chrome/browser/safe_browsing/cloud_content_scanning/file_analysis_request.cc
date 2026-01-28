// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

#include <algorithm>
#include <string_view>

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/file_access/scoped_file_access.h"
#include "content/public/browser/browser_thread.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/file_util_service.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/enterprise/obfuscation/core/utils.h"
#endif

namespace safe_browsing {

namespace {

using ::enterprise_connectors::BinaryUploadRequest;
using ::enterprise_connectors::FileAnalysisRequestBase;
using ::enterprise_connectors::GetBrowserPolicyConnector;

}  // namespace

FileAnalysisRequest::FileAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    std::string mime_type,
    bool delay_opening_file,
    BinaryUploadRequest::ContentAnalysisCallback callback,
    BinaryUploadRequest::RequestStartCallback start_callback,
    bool is_obfuscated,
    bool force_sync_hash_computation)
    : FileAnalysisRequestBase(analysis_settings,
                              std::move(path),
                              std::move(file_name),
                              std::move(mime_type),
                              delay_opening_file,
                              std::move(callback),
                              base::BindRepeating(&GetBrowserPolicyConnector),
                              content::GetUIThreadTaskRunner({}),
                              std::move(start_callback),
                              is_obfuscated,
                              force_sync_hash_computation) {}

FileAnalysisRequest::~FileAnalysisRequest() = default;

#if !BUILDFLAG(IS_ANDROID)
void FileAnalysisRequest::ProcessZipFile(Data data) {
  auto callback =
      base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
                     weakptr_factory_.GetWeakPtr(), std::move(data));
  if (is_obfuscated_ &&
      base::FeatureList::IsEnabled(
          enterprise_obfuscation::kEnterpriseFileObfuscationArchiveAnalyzer)) {
    zip_analyzer_ = SandboxedZipAnalyzer::CreateObfuscatedAnalyzer(
        path_,
        /*password=*/password(), std::move(callback), LaunchFileUtilService());
  } else {
    zip_analyzer_ = SandboxedZipAnalyzer::CreateAnalyzer(
        path_,
        /*password=*/password(), std::move(callback), LaunchFileUtilService());
  }
  zip_analyzer_->Start();
}

void FileAnalysisRequest::ProcessRarFile(Data data) {
  auto callback =
      base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
                     weakptr_factory_.GetWeakPtr(), std::move(data));
  if (is_obfuscated_ &&
      base::FeatureList::IsEnabled(
          enterprise_obfuscation::kEnterpriseFileObfuscationArchiveAnalyzer)) {
    rar_analyzer_ = SandboxedRarAnalyzer::CreateObfuscatedAnalyzer(
        path_,
        /*password=*/password(), std::move(callback), LaunchFileUtilService());
  } else {
    rar_analyzer_ = SandboxedRarAnalyzer::CreateAnalyzer(
        path_,
        /*password=*/password(), std::move(callback), LaunchFileUtilService());
  }
  rar_analyzer_->Start();
}

void FileAnalysisRequest::OnCheckedForEncryption(
    Data data,
    const ArchiveAnalyzerResults& analyzer_result) {
  bool encrypted = analyzer_result.encryption_info.is_encrypted &&
                   analyzer_result.encryption_info.password_status ==
                       EncryptionInfo::kKnownIncorrect;

  enterprise_connectors::ScanRequestUploadResult result =
      encrypted ? enterprise_connectors::ScanRequestUploadResult::kFileEncrypted
                : enterprise_connectors::ScanRequestUploadResult::kSuccess;
  CacheResultAndData(result, std::move(data));
  RunCallback();
}
#endif

}  // namespace safe_browsing
