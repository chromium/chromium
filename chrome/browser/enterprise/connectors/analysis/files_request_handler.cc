// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_opening_job.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace enterprise_connectors {

namespace {

// Global pointer of factory function (RepeatingCallback) used to create
// instances of ContentAnalysisDelegate in tests.  !is_null() only in tests.
FilesRequestHandler::Factory* GetFactoryStorage() {
  static base::NoDestructor<FilesRequestHandler::Factory> factory;
  return factory.get();
}

AnalysisConnector AccessPointToEnterpriseConnector(
    DeepScanAccessPoint access_point) {
  switch (access_point) {
    case DeepScanAccessPoint::FILE_TRANSFER:
      return enterprise_connectors::FILE_TRANSFER;
    case DeepScanAccessPoint::UPLOAD:
    case DeepScanAccessPoint::DRAG_AND_DROP:
    case DeepScanAccessPoint::PASTE:
      // A file can be uploaded to a website by either a normal file picker, a
      // dragNdrop event or using copy+paste.
      return enterprise_connectors::FILE_ATTACHED;
    case DeepScanAccessPoint::DOWNLOAD:
    case DeepScanAccessPoint::PRINT:
  }
  NOTREACHED();
}

// LINT.IfChange(AccessPointToUmaHistogramPrefix)
std::string AccessPointToUmaHistogramPrefix(DeepScanAccessPoint access_point) {
  switch (AccessPointToEnterpriseConnector(access_point)) {
    case enterprise_connectors::FILE_TRANSFER:
      return "Enterprise.OnFileTransfer";
    case enterprise_connectors::FILE_ATTACHED:
      return "Enterprise.OnFileAttach";
    default:
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/histograms.xml:FileUploadEvent)

std::string AccessPointToTriggerString(DeepScanAccessPoint access_point) {
  switch (AccessPointToEnterpriseConnector(access_point)) {
    case enterprise_connectors::FILE_TRANSFER:
      return kFileTransferDataTransferEventTrigger;
    case enterprise_connectors::FILE_ATTACHED:
      return kFileUploadDataTransferEventTrigger;
    default:
  }
  NOTREACHED();
}

struct UnreportedFileInfo {
  base::FilePath path;
  FilesRequestHandlerBase::FileInfo file_info;
};

void ReportCancellationsOnUIThread(
    base::WeakPtr<Profile> profile_weak,
    std::string source,
    std::string destination,
    std::string access_point_trigger,
    std::string content_transfer_method,
    GURL url,
    GURL tab_url,
    std::vector<UnreportedFileInfo> unreported_files) {
  Profile* profile = profile_weak.get();
  if (!profile) {
    return;
  }

  auto* router = ReportingEventRouterFactory::GetForBrowserContext(profile);
  if (!router) {
    return;
  }

  for (const auto& unreported : unreported_files) {
    router->OnUnscannedFileEvent(
        url, tab_url, source, destination, unreported.path.AsUTF8Unsafe(),
        unreported.file_info.sha256_or_cb, unreported.file_info.mime_type,
        access_point_trigger, /*scan_id=*/"",
        enterprise_connectors::kUserCancelledUnscannedReason,
        content_transfer_method, unreported.file_info.size,
        EventResult::CANCELLED);
  }
}

std::vector<UnreportedFileInfo> FetchFileSizes(
    std::vector<UnreportedFileInfo> unreported_files) {
  for (auto& unreported : unreported_files) {
    if (unreported.file_info.size == 0) {
      int64_t file_size = base::GetFileSize(unreported.path).value_or(0);
      unreported.file_info.size = file_size;
    }
  }
  return unreported_files;
}

}  // namespace

FilesRequestHandler::FilesRequestHandler(
    ContentAnalysisInfo* content_analysis_info,
    BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    const std::string& source,
    const std::string& destination,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         url,
                         access_point),
      paths_(paths),
      source_(source),
      destination_(destination),
      content_transfer_method_(content_transfer_method),
      profile_(profile),
      callback_(std::move(callback)) {
  results_.resize(paths_.size());
  file_info_.resize(paths_.size());
  start_times_.resize(paths_.size(), base::TimeTicks::Min());
  for (size_t i = 0; i < paths_.size(); ++i) {
    unreported_files_.insert(i);
  }
}

// static
std::unique_ptr<FilesRequestHandler> FilesRequestHandler::Create(
    ContentAnalysisInfo* content_analysis_info,
    BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    const std::string& source,
    const std::string& destination,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback) {
  if (GetFactoryStorage()->is_null()) {
    return base::WrapUnique(new FilesRequestHandler(
        content_analysis_info, upload_service, profile, url, source,
        destination, content_transfer_method, access_point, paths,
        std::move(callback)));
  } else {
    // Use the factory to create a fake FilesRequestHandler.
    return GetFactoryStorage()->Run(content_analysis_info, upload_service,
                                    profile, url, source, destination,
                                    content_transfer_method, access_point,
                                    paths, std::move(callback));
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

FilesRequestHandler::~FilesRequestHandler() {
  MaybeCancelAndReport();
}

void FilesRequestHandler::MaybeCancelAndReport() {
  // If all files have been reported, then we can return early without
  // reporting a cancellation / cancelling the file opening job.
  if (file_result_count_ >= paths_.size()) {
    return;
  }

  // Actively cancel the file opening job to ensure that all file access is
  // released as soon as possible and to avoid memory leaks. It makes sense to
  // only call this if the handler was interrupted by user. This will allow to
  // track user cancellation metrics.
  if (file_opening_job_) {
    file_opening_job_->Cancel();
  }

  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableCancelUploadOnContentAnalysis)) {
    return;
  }

  std::vector<UnreportedFileInfo> unreported_files;
  unreported_files.reserve(unreported_files_.size());
  for (size_t index : unreported_files_) {
    unreported_files.push_back(
        {std::move(paths_[index]), std::move(file_info_[index])});
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FetchFileSizes, std::move(unreported_files)),
      base::BindOnce(
          &ReportCancellationsOnUIThread, profile_->GetWeakPtr(), source_,
          destination_, AccessPointToTriggerString(access_point_),
          content_transfer_method_, GURL(content_analysis_info_->url()),
          content_analysis_info_->tab_url()));
}

void FilesRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  // Report a warning bypass for each previously warned file.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;

    ReportAnalysisConnectorWarningBypass(
        ReportingEventRouterFactory::GetForBrowserContext(profile_),
        content_analysis_info_.get(), source_, destination_,
        paths_[index].AsUTF8Unsafe(), file_info_[index].sha256_or_cb,
        file_info_[index].mime_type, AccessPointToTriggerString(access_point_),
        content_transfer_method_, file_info_[index].size, warning.second,
        user_justification);
  }
}

void FilesRequestHandler::FileRequestCallbackForTesting(
    base::FilePath path,
    ScanRequestUploadResult result,
    enterprise_connectors::ContentAnalysisResponse response) {
  auto it = std::ranges::find(paths_, path);
  CHECK(it != paths_.end());
  size_t index = std::distance(paths_.begin(), it);
  FileRequestCallback(index, result, response);
}

bool FilesRequestHandler::UploadDataImpl() {
  IncrementCrashKey(ScanningCrashKey::PENDING_FILE_UPLOADS, paths_.size());

  if (!paths_.empty()) {
    IncrementCrashKey(ScanningCrashKey::TOTAL_FILE_UPLOADS, paths_.size());

    std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks(
        paths_.size());
    for (size_t i = 0; i < paths_.size(); ++i) {
      tasks[i].request = PrepareFileRequest(i);
    }

    file_access::RequestFilesAccessForSystem(
        paths_,
        base::BindOnce(&FilesRequestHandler::CreateFileOpeningJob,
                       weak_ptr_factory_.GetWeakPtr(), std::move(tasks)));

    if (auto prefix = AccessPointToUmaHistogramPrefix(access_point_);
        !prefix.empty()) {
      base::UmaHistogramCustomCounts(prefix + ".FileCount", paths_.size(), 1,
                                     1000, 100);
    }

    return true;
  }

  // If zero files were passed to the FilesRequestHandler, we call the callback
  // directly.
  MaybeCompleteScanRequest();
  return false;
}

enterprise_connectors::FileAnalysisRequestBase*
FilesRequestHandler::PrepareFileRequest(size_t index) {
  DCHECK_LT(index, paths_.size());
  base::FilePath path = paths_[index];
  auto request = std::make_unique<safe_browsing::FileAnalysisRequest>(
      content_analysis_info_->settings(), path, path.BaseName(),
      /*mime_type*/ "",
      /* delay_opening_file */ true,
      base::BindOnce(&FilesRequestHandler::FileRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), index),
      base::BindOnce(&FilesRequestHandler::FileRequestStartCallback,
                     weak_ptr_factory_.GetWeakPtr(), index),
      /* is_obfuscated */ false,
      /* force_sync_hash_computation */ false);
  enterprise_connectors::FileAnalysisRequestBase* request_raw = request.get();
  content_analysis_info_->InitializeRequest(
      request_raw, /*include_enterprise_only_fields=*/true);
  request_raw->set_analysis_connector(
      AccessPointToEnterpriseConnector(access_point_));
  request_raw->set_source(source_);
  request_raw->set_destination(destination_);
  request_raw->GetRequestData(base::BindOnce(
      &FilesRequestHandler::OnGotFileInfo, weak_ptr_factory_.GetWeakPtr(),
      std::move(request), index));

  return request_raw;
}

void FilesRequestHandler::OnGotFileInfo(
    std::unique_ptr<BinaryUploadRequest> request,
    size_t index,
    ScanRequestUploadResult result,
    BinaryUploadRequest::Data data) {
  DCHECK_LT(index, paths_.size());
  DCHECK_EQ(paths_.size(), file_info_.size());

  file_info_[index].sha256_or_cb = data.hash;
  if (data.hash.empty() && request->register_on_got_hash_callback_) {
    request->register_on_got_hash_callback_.Run(
        /* call_last= */ false,
        base::BindOnce(&FilesRequestHandler::OnGotHash,
                       weak_ptr_factory_.GetWeakPtr(), index));
    file_info_[index].sha256_or_cb = base::BindRepeating(
        request->register_on_got_hash_callback_, /* call_last= */ false);
  }
  file_info_[index].size = data.size;
  file_info_[index].mime_type = data.mime_type;

  const auto& analysis_settings = content_analysis_info_->settings();
  bool is_cloud = analysis_settings.cloud_or_local_settings.is_cloud_analysis();
  bool is_resumable = IsResumableUpload(*request);
  bool failed = is_resumable
                    ? CloudResumableResultIsFailure(
                          result, analysis_settings.block_large_files,
                          analysis_settings.block_password_protected_files)
                    : (is_cloud ? CloudMultipartResultIsFailure(result)
                                : LocalResultIsFailure(result));
  if (failed) {
    FinishRequestEarly(std::move(request), result);
    return;
  }

  // Don't bother sending empty files for deep scanning.
  if (data.size == 0) {
    FinishRequestEarly(std::move(request), ScanRequestUploadResult::kSuccess);
    return;
  }

  // If |throttled_| is true, then the file shouldn't be upload since the server
  // is receiving too many requests.
  if (throttled_) {
    FinishRequestEarly(std::move(request),
                       ScanRequestUploadResult::kTooManyRequests);
    return;
  }

  UploadFileForDeepScanning(result, paths_[index], std::move(request));
}

void FilesRequestHandler::OnGotHash(size_t index, std::string hash) {
  // The FileAnalysisRequest will soon be destroyed, so overwrite the callback
  // to that object with the actual hash.
  file_info_[index].sha256_or_cb = hash;
}

void FilesRequestHandler::FinishRequestEarly(
    std::unique_ptr<BinaryUploadRequest> request,
    ScanRequestUploadResult result) {
  // We add the request here in case we never actually uploaded anything, so it
  // wasn't added in OnGetRequestData
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanRequests(
          request->per_profile_request(),
          /*access_token*/ "",
          /*upload_info*/ ScanRequestUploadResultToString(result),
          /*upload_url=*/"", request->content_analysis_request());
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanResponses(
          /*token=*/"", ScanRequestUploadResultToString(result),
          enterprise_connectors::ContentAnalysisResponse());

  request->FinishRequest(result,
                         enterprise_connectors::ContentAnalysisResponse());
}

void FilesRequestHandler::UploadFileForDeepScanning(
    ScanRequestUploadResult result,
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadRequest> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void FilesRequestHandler::FileRequestStartCallback(
    size_t index,
    const BinaryUploadRequest& request) {
  start_times_[index] = base::TimeTicks::Now();
}

void FilesRequestHandler::FileRequestCallback(
    size_t index,
    ScanRequestUploadResult upload_result,
    enterprise_connectors::ContentAnalysisResponse response) {
  // Remember to send an ack for this response.  It's possible for the response
  // to be empty and have no request token.  This may happen if Chrome decides
  // to allow the file without uploading with the binary upload service.  For
  // example, zero length files.
  if (upload_result == ScanRequestUploadResult::kSuccess &&
      response.has_request_token()) {
    request_tokens_to_ack_final_actions_[response.request_token()] =
        GetAckFinalAction(response);
  }

  DCHECK_EQ(results_.size(), paths_.size());
  if (upload_result == ScanRequestUploadResult::kTooManyRequests) {
    if (!throttled_) {
      if (auto prefix = AccessPointToUmaHistogramPrefix(access_point_);
          !prefix.empty()) {
        base::UmaHistogramBoolean(prefix + ".Throttled", true);
      }
    }
    throttled_ = true;
  }

  // Find the path in the set of files that are being scanned.
  DCHECK_LT(index, paths_.size());
  const base::FilePath& path = paths_[index];

  const auto start_timestamp = (start_times_[index] != base::TimeTicks::Min())
                                   ? start_times_[index]
                                   : upload_start_time_;

  const auto& analysis_settings = content_analysis_info_->settings();
  RecordDeepScanMetrics(
      analysis_settings.cloud_or_local_settings.is_cloud_analysis(),
      access_point_, base::TimeTicks::Now() - start_timestamp,
      file_info_[index].size, upload_result, response);

  RequestHandlerResult request_handler_result =
      CalculateRequestHandlerResult(analysis_settings, upload_result, response);
  results_[index] = request_handler_result;
  ++file_result_count_;

  bool result_is_warning = request_handler_result.final_result ==
                           FinalContentAnalysisResult::WARNING;
  if (result_is_warning) {
    file_warnings_[index] = response;
  }

  MaybeReportDeepScanningVerdict(
      ReportingEventRouterFactory::GetForBrowserContext(profile_),
      content_analysis_info_.get(), source_, destination_, path.AsUTF8Unsafe(),
      file_info_[index].sha256_or_cb, file_info_[index].mime_type,
      AccessPointToTriggerString(access_point_), content_transfer_method_,
      content_analysis_info_->GetContentAreaAccountEmail(),
      file_info_[index].size, upload_result, response,
      CalculateEventResult(analysis_settings, request_handler_result.complies,
                           result_is_warning));
  unreported_files_.erase(index);

  DecrementCrashKey(ScanningCrashKey::PENDING_FILE_UPLOADS);

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
  // Keep a reference to `file_opening_job` in each task to ensure
  // its lifetime will be longer than the request.
  file_opening_job_ =
      base::MakeRefCounted<safe_browsing::FileOpeningJob>(std::move(tasks));
}

}  // namespace enterprise_connectors
