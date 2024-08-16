// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_opening_job.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

namespace enterprise_connectors {

namespace {

// Global pointer of factory function (RepeatingCallback) used to create
// instances of ContentAnalysisDelegate in tests.  !is_null() only in tests.
FilesRequestHandler::Factory* GetFactoryStorage() {
  static base::NoDestructor<FilesRequestHandler::Factory> factory;
  return factory.get();
}

AnalysisConnector AccessPointToEnterpriseConnector(
    safe_browsing::DeepScanAccessPoint access_point) {
  switch (access_point) {
    case safe_browsing::DeepScanAccessPoint::FILE_TRANSFER:
      return enterprise_connectors::FILE_TRANSFER;
    case safe_browsing::DeepScanAccessPoint::UPLOAD:
    case safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP:
    case safe_browsing::DeepScanAccessPoint::PASTE:
      // A file can be uploaded to a website by either a normal file picker, a
      // dragNdrop event or using copy+paste.
      return enterprise_connectors::FILE_ATTACHED;
    case safe_browsing::DeepScanAccessPoint::DOWNLOAD:
    case safe_browsing::DeepScanAccessPoint::PRINT:
      NOTREACHED_IN_MIGRATION();
  }
  return enterprise_connectors::FILE_ATTACHED;
}

std::string AccessPointToTriggerString(
    safe_browsing::DeepScanAccessPoint access_point) {
  switch (access_point) {
    case safe_browsing::DeepScanAccessPoint::FILE_TRANSFER:
      return extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer;
    case safe_browsing::DeepScanAccessPoint::UPLOAD:
    case safe_browsing::DeepScanAccessPoint::DRAG_AND_DROP:
    case safe_browsing::DeepScanAccessPoint::PASTE:
      // A file can be uploaded to a website by either a normal file picker, a
      // dragNdrop event or using copy+paste.
      return extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload;
    case safe_browsing::DeepScanAccessPoint::DOWNLOAD:
    case safe_browsing::DeepScanAccessPoint::PRINT:
      NOTREACHED_IN_MIGRATION();
  }
  return "";
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
    const std::string& source,
    const std::string& destination,
    const std::string& user_action_id,
    const std::string& tab_title,
    const std::string& content_transfer_method,
    safe_browsing::DeepScanAccessPoint access_point,
    ContentAnalysisRequest::Reason reason,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback)
    : RequestHandlerBase(upload_service,
                         profile,
                         analysis_settings,
                         url,
                         source,
                         destination,
                         user_action_id,
                         tab_title,
                         paths.size(),
                         access_point,
                         reason),
      paths_(paths),
      content_transfer_method_(content_transfer_method),
      callback_(std::move(callback)) {
  results_.resize(paths_.size());
  file_info_.resize(paths_.size());
  start_times_.resize(paths_.size(), base::TimeTicks::Min());
}

// static
std::unique_ptr<FilesRequestHandler> FilesRequestHandler::Create(
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
    CompletionCallback callback) {
  if (GetFactoryStorage()->is_null()) {
    return base::WrapUnique(new FilesRequestHandler(
        upload_service, profile, analysis_settings, url, source, destination,
        user_action_id, tab_title, content_transfer_method, access_point,
        reason, paths, std::move(callback)));
  } else {
    // Use the factory to create a fake FilesRequestHandler.
    return GetFactoryStorage()->Run(
        upload_service, profile, analysis_settings, url, source, destination,
        user_action_id, tab_title, content_transfer_method, access_point,
        reason, paths, std::move(callback));
  }
}

// static
void FilesRequestHandler::SetFactoryForTesting(Factory factory) {
  *GetFactoryStorage() = factory;
}

// static
void FilesRequestHandler::ResetFactoryForTesting() {
  if (GetFactoryStorage())
    GetFactoryStorage()->Reset();
}

FilesRequestHandler::~FilesRequestHandler() = default;

void FilesRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  // Report a warning bypass for each previously warned file.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, url_, source_, destination_,
        paths_[index].AsUTF8Unsafe(), file_info_[index].sha256,
        file_info_[index].mime_type, AccessPointToTriggerString(access_point_),
        content_transfer_method_, access_point_, file_info_[index].size,
        warning.second, user_justification);
  }
}

void FilesRequestHandler::FileRequestCallbackForTesting(
    base::FilePath path,
    safe_browsing::BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  auto it = base::ranges::find(paths_, path);
  CHECK(it != paths_.end(), base::NotFatalUntil::M130);
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

    file_access::RequestFilesAccessForSystem(
        paths_,
        base::BindOnce(&FilesRequestHandler::CreateFileOpeningJob,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tasks)));

    return true;
  }

  // If zero files were passed to the FilesRequestHandler, we call the callback
  // directly.
  MaybeCompleteScanRequest();
  return false;
}

safe_browsing::FileAnalysisRequest* FilesRequestHandler::PrepareFileRequest(
    size_t index) {
  DCHECK_LT(index, paths_.size());
  base::FilePath path = paths_[index];
  auto request = std::make_unique<safe_browsing::FileAnalysisRequest>(
      *analysis_settings_, path, path.BaseName(), /*mime_type*/ "",
      /* delay_opening_file */ true,
      base::BindOnce(&FilesRequestHandler::FileRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), index),
      base::BindOnce(&FilesRequestHandler::FileRequestStartCallback,
                     weak_ptr_factory_.GetWeakPtr(), index));
  safe_browsing::FileAnalysisRequest* request_raw = request.get();
  PrepareRequest(AccessPointToEnterpriseConnector(access_point_), request_raw);
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

  bool is_cloud =
      analysis_settings_->cloud_or_local_settings.is_cloud_analysis();
  bool is_resumable = IsResumableUpload(*request);
  bool failed = is_resumable
                    ? CloudResumableResultIsFailure(
                          result, analysis_settings_->block_large_files,
                          analysis_settings_->block_password_protected_files)
                    : (is_cloud ? CloudMultipartResultIsFailure(result)
                                : LocalResultIsFailure(result));
  if (failed) {
    FinishRequestEarly(std::move(request), result);
    return;
  }

  // Don't bother sending empty files for deep scanning.
  if (data.size == 0) {
    FinishRequestEarly(std::move(request),
                       safe_browsing::BinaryUploadService::Result::SUCCESS);
    return;
  }

  // If |throttled_| is true, then the file shouldn't be upload since the server
  // is receiving too many requests.
  if (throttled_) {
    FinishRequestEarly(
        std::move(request),
        safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS);
    return;
  }

  UploadFileForDeepScanning(result, paths_[index], std::move(request));
}

void FilesRequestHandler::FinishRequestEarly(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    safe_browsing::BinaryUploadService::Result result) {
  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), /*access_token*/ "", /*upload_info*/ "",
      /*upload_url=*/"", request->content_analysis_request());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      /*token=*/"", safe_browsing::BinaryUploadService::ResultToString(result),
      enterprise_connectors::ContentAnalysisResponse());

  request->FinishRequest(result,
                         enterprise_connectors::ContentAnalysisResponse());
}

void FilesRequestHandler::UploadFileForDeepScanning(
    safe_browsing::BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  safe_browsing::BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void FilesRequestHandler::FileRequestStartCallback(
    size_t index,
    const safe_browsing::BinaryUploadService::Request& request) {
  start_times_[index] = base::TimeTicks::Now();
}

void FilesRequestHandler::FileRequestCallback(
    size_t index,
    safe_browsing::BinaryUploadService::Result upload_result,
    enterprise_connectors::ContentAnalysisResponse response) {
  // Remember to send an ack for this response.  It's possible for the response
  // to be empty and have no request token.  This may happen if Chrome decides
  // to allow the file without uploading with the binary upload service.  For
  // example, zero length files.
  if (upload_result == safe_browsing::BinaryUploadService::Result::SUCCESS &&
      response.has_request_token()) {
    request_tokens_to_ack_final_actions_[response.request_token()] =
        GetAckFinalAction(response);
  }

  DCHECK_EQ(results_.size(), paths_.size());
  if (upload_result ==
      safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS) {
    throttled_ = true;
  }

  // Find the path in the set of files that are being scanned.
  DCHECK_LT(index, paths_.size());
  const base::FilePath& path = paths_[index];

  const auto start_timestamp = (start_times_[index] != base::TimeTicks::Min())
                                   ? start_times_[index]
                                   : upload_start_time_;

  RecordDeepScanMetrics(
      analysis_settings_->cloud_or_local_settings.is_cloud_analysis(),
      access_point_, base::TimeTicks::Now() - start_timestamp,
      file_info_[index].size, upload_result, response);

  RequestHandlerResult request_handler_result = CalculateRequestHandlerResult(
      *analysis_settings_, upload_result, response);
  results_[index] = request_handler_result;
  ++file_result_count_;

  bool result_is_warning = request_handler_result.final_result ==
                           FinalContentAnalysisResult::WARNING;
  if (result_is_warning) {
    file_warnings_[index] = response;
  }

  MaybeReportDeepScanningVerdict(
      profile_, url_, url_, source_, destination_, path.AsUTF8Unsafe(),
      file_info_[index].sha256, file_info_[index].mime_type,
      AccessPointToTriggerString(access_point_), content_transfer_method_,

      access_point_, file_info_[index].size, upload_result, response,
      CalculateEventResult(*analysis_settings_, request_handler_result.complies,
                           result_is_warning));

  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_FILE_UPLOADS);

  MaybeCompleteScanRequest();
}

void FilesRequestHandler::MaybeCompleteScanRequest() {
  if (file_result_count_ < paths_.size()) {
    return;
  }
  scoped_file_access_.reset();
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(results_));
}

void FilesRequestHandler::CreateFileOpeningJob(
    std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks,
    file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  file_opening_job_ =
      std::make_unique<safe_browsing::FileOpeningJob>(std::move(tasks));
}

}  // namespace enterprise_connectors
