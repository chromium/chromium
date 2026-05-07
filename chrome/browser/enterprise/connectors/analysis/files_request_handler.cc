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
    safe_browsing::ReferrerChain referrer_chain,
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
        content_transfer_method, unreported.file_info.size, referrer_chain,
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
    Profile* profile,
    const std::string& source,
    const std::string& destination,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback)
    : profile_(profile),
      paths_(paths),
      source_(source),
      destination_(destination),
      callback_(std::move(callback)) {
  results_.resize(paths_.size());
  file_info_.resize(paths_.size());
  start_times_.resize(paths_.size(), base::TimeTicks::Min());
  for (size_t i = 0; i < paths_.size(); ++i) {
    unreported_files_.insert(i);
  }
}

// static
std::unique_ptr<FilesRequestHandlerBase> FilesRequestHandler::Create(
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
    return std::make_unique<FilesRequestHandlerBase>(
        content_analysis_info, upload_service, url, content_transfer_method,
        access_point,
        std::make_unique<FilesRequestHandler>(profile, source, destination,
                                              paths, std::move(callback)));
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
  if (unreported_files_.empty() ||
      handler_->file_result_count() >= paths_.size()) {
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
  unreported_files_.clear();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FetchFileSizes, std::move(unreported_files)),
      base::BindOnce(&ReportCancellationsOnUIThread, profile_->GetWeakPtr(),
                     source_, destination_, handler_->access_point_string(),
                     handler_->content_transfer_method(),
                     GURL(handler_->content_analysis_info()->url()),
                     handler_->content_analysis_info()->tab_url(),
                     handler_->content_analysis_info()->referrer_chain()));
}

void FilesRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification,
    const ContentAnalysisInfoBase& info,
    const std::string& trigger,
    const std::string& content_transfer_method) {
  // Report a warning bypass for each previously warned file.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;

    ReportAnalysisConnectorWarningBypass(
        ReportingEventRouterFactory::GetForBrowserContext(profile_), &info,
        source_, destination_, paths_[index].AsUTF8Unsafe(),
        file_info_[index].sha256_or_cb, file_info_[index].mime_type, trigger,
        content_transfer_method, file_info_[index].size, warning.second,
        user_justification);
  }
}

bool FilesRequestHandler::UploadDataImpl() {
  if (!paths_.empty()) {
    std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks(
        paths_.size());
    for (size_t i = 0; i < paths_.size(); ++i) {
      tasks[i].request = handler_->PrepareFileRequest(i);
    }

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

std::unique_ptr<FileAnalysisRequestBase> FilesRequestHandler::CreateFileRequest(
    size_t index,
    const AnalysisSettings& settings,
    base::OnceCallback<void(ScanRequestUploadResult, ContentAnalysisResponse)>
        callback,
    base::OnceCallback<void(const BinaryUploadRequest&)>
        request_start_callback) {
  DCHECK_LT(index, paths_.size());
  base::FilePath path = paths_[index];
  return std::make_unique<safe_browsing::FileAnalysisRequest>(
      settings, path, path.BaseName(),
      /*mime_type*/ "",
      /* delay_opening_file */ true, std::move(callback),
      std::move(request_start_callback),
      /* is_obfuscated */ false,
      /* force_sync_hash_computation */ false);
}

void FilesRequestHandler::UpdateRequestHandlerResult(
    size_t index,
    RequestHandlerResult result,
    ContentAnalysisResponse response) {
  DCHECK_LT(index, paths_.size());
  results_[index] = result;
  if (result.final_result == FinalContentAnalysisResult::WARNING) {
    file_warnings_[index] = response;
  }
}

const base::FilePath& FilesRequestHandler::GetPath(size_t index) const {
  DCHECK_LT(index, paths_.size());
  return paths_[index];
}

const FilesRequestHandlerBase::FileInfo& FilesRequestHandler::GetFileInfo(
    size_t index) {
  DCHECK_LT(index, paths_.size());
  return file_info_[index];
}

FilesRequestHandlerBase::FileInfo& FilesRequestHandler::GetMutableFileInfo(
    size_t index) {
  DCHECK_LT(index, paths_.size());
  return file_info_[index];
}

size_t FilesRequestHandler::GetFileCount() const {
  return paths_.size();
}

void FilesRequestHandler::SetFileScanStartTime(size_t index) {
  DCHECK_LT(index, paths_.size());
  start_times_[index] = base::TimeTicks::Now();
}

const base::TimeTicks FilesRequestHandler::GetFileScanStartTime(size_t index) {
  DCHECK_LT(index, paths_.size());
  return start_times_[index];
}

ReportingEventRouter* FilesRequestHandler::GetReportingEventRouter() {
  return ReportingEventRouterFactory::GetForBrowserContext(profile_);
}

void FilesRequestHandler::MaybeCompleteScanRequest() {
  if (handler_->file_result_count() < paths_.size()) {
    return;
  }
  scoped_file_access_.reset();
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(results_));
}

std::string FilesRequestHandler::GetSource() {
  return source_;
}

std::string FilesRequestHandler::GetDestination() {
  return destination_;
}

void FilesRequestHandler::SetHandler(FilesRequestHandlerBase* handler) {
  handler_ = handler;
}

void FilesRequestHandler::MarkFileAsReported(size_t index) {
  unreported_files_.erase(index);
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
