// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"

namespace enterprise_connectors {
namespace {
FilesRequestHandler::FakeFileUploadCallback* FakeFileUploadCallbackStorage() {
  static base::NoDestructor<FilesRequestHandler::FakeFileUploadCallback>
      fake_file_upload_callback;
  return fake_file_upload_callback.get();
}
}  // namespace

FilesRequestHandler::FileInfo::FileInfo() = default;
FilesRequestHandler::FileInfo::FileInfo(FileInfo&& other) = default;
FilesRequestHandler::FileInfo::~FileInfo() = default;

FilesRequestHandler::FilesRequestHandler(
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    GURL url,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback)
    : RequestHandlerBase(upload_service,
                         profile,
                         analysis_settings,
                         url,
                         access_point),
      paths_(paths),
      callback_(std::move(callback)) {
  results_.resize(paths_.size());
  file_info_.resize(paths_.size());
}

FilesRequestHandler::~FilesRequestHandler() = default;

void FilesRequestHandler::ReportWarningBypass(
    absl::optional<std::u16string> user_justification) {
  // Report a warning bypass for each previously warned file.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, paths_[index].AsUTF8Unsafe(), file_info_[index].sha256,
        file_info_[index].mime_type,
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
        access_point_, file_info_[index].size, warning.second,
        user_justification);
  }
}

// static
void FilesRequestHandler::SetFakeUploadCallback(
    FakeFileUploadCallback fake_file_upload_callback) {
  *FakeFileUploadCallbackStorage() = fake_file_upload_callback;
}

void FilesRequestHandler::FileRequestCallbackForTesting(
    base::FilePath path,
    safe_browsing::BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  auto it = std::find(paths_.begin(), paths_.end(), path);
  DCHECK(it != paths_.end());
  size_t index = std::distance(paths_.begin(), it);
  FileRequestCallback(index, result, response);
}

bool FilesRequestHandler::UploadDataImpl() {
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_FILE_UPLOADS, paths_.size());

  if (!paths_.empty()) {
    safe_browsing::IncrementCrashKey(
        safe_browsing::ScanningCrashKey::TOTAL_FILE_UPLOADS, paths_.size());

    std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks(
        paths_.size());
    for (size_t i = 0; i < paths_.size(); ++i)
      tasks[i].request = PrepareFileRequest(i);

    file_opening_job_ =
        std::make_unique<safe_browsing::FileOpeningJob>(std::move(tasks));
    return true;
  }
  return false;
}

safe_browsing::FileAnalysisRequest* FilesRequestHandler::PrepareFileRequest(
    size_t index) {
  DCHECK_LT(index, paths_.size());
  base::FilePath path = paths_[index];
  auto request = std::make_unique<safe_browsing::FileAnalysisRequest>(
      analysis_settings_, path, path.BaseName(), /*mime_type*/ "",
      /* delay_opening_file */ true,
      base::BindOnce(&FilesRequestHandler::FileRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), index));
  safe_browsing::FileAnalysisRequest* request_raw = request.get();
  PrepareRequest(enterprise_connectors::FILE_ATTACHED, request_raw);
  request_raw->GetRequestData(base::BindOnce(
      &FilesRequestHandler::OnGotFileInfo, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), index));

  return request_raw;
}

void FilesRequestHandler::OnGotFileInfo(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    size_t index,
    safe_browsing::BinaryUploadService::Result result,
    safe_browsing::BinaryUploadService::Request::Data data) {
  DCHECK_LT(index, paths_.size());
  DCHECK_EQ(paths_.size(), file_info_.size());

  file_info_[index].sha256 = data.hash;
  file_info_[index].size = data.size;
  file_info_[index].mime_type = data.mime_type;

  // If a non-SUCCESS result was previously obtained, it means the file has some
  // property (too large, unsupported file type, encrypted, ...) that make its
  // upload pointless, so the request should finish early.
  if (result != safe_browsing::BinaryUploadService::Result::SUCCESS) {
    request->FinishRequest(result,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  // If |throttled_| is true, then the file shouldn't be upload since the server
  // is receiving too many requests.
  if (throttled_) {
    request->FinishRequest(
        safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS,
        enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  UploadFileForDeepScanning(result, paths_[index], std::move(request));
}

void FilesRequestHandler::UploadFileForDeepScanning(
    safe_browsing::BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  if (!FakeFileUploadCallbackStorage()->is_null()) {
    FakeFileUploadCallbackStorage()->Run(result, path, std::move(request));
    return;
  }
  safe_browsing::BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void FilesRequestHandler::FileRequestCallback(
    size_t index,
    safe_browsing::BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (result == safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS)
    throttled_ = true;

  // Find the path in the set of files that are being scanned.
  DCHECK_LT(index, paths_.size());
  const base::FilePath& path = paths_[index];

  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        file_info_[index].size, result, response);
  std::string tag;
  auto action = GetHighestPrecedenceAction(response, &tag);

  bool file_complies = ResultShouldAllowDataUse(result) &&
                       ContentAnalysisActionAllowsDataUse(action);

  bool should_warn = action == enterprise_connectors::TriggeredRule::WARN;

  MaybeReportDeepScanningVerdict(
      profile_, url_, path.AsUTF8Unsafe(), file_info_[index].sha256,
      file_info_[index].mime_type,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      access_point_, file_info_[index].size, result, response,
      CalculateEventResult(file_complies, should_warn));

  ++file_result_count_;

  DCHECK_EQ(results_.size(), paths_.size());
  Result file_result;
  file_result.complies = file_complies;
  file_result.tag = tag;
  if (!file_complies) {
    if (result == safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE) {
      file_result.final_result = FinalContentAnalysisResult::LARGE_FILES;
    } else if (result ==
               safe_browsing::BinaryUploadService::Result::FILE_ENCRYPTED) {
      file_result.final_result = FinalContentAnalysisResult::ENCRYPTED_FILES;
    } else if (should_warn) {
      file_warnings_[index] = std::move(response);
      file_result.final_result = FinalContentAnalysisResult::WARNING;
    } else {
      file_result.final_result = FinalContentAnalysisResult::FAILURE;
    }
  } else {
    file_result.final_result = FinalContentAnalysisResult::SUCCESS;
  }
  results_[index] = file_result;

  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_FILE_UPLOADS);
  MaybeCompleteScanRequest();
}

void FilesRequestHandler::MaybeCompleteScanRequest() {
  if (file_result_count_ < paths_.size()) {
    return;
  }
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(results_));
}

}  // namespace enterprise_connectors
