// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_request_job.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/isolated_context.h"

using content::BrowserThread;

namespace chromeos {
namespace {

const char kMimeTypeForRFC822[] = "message/rfc822";
const char kMimeTypeForMHTML[] = "multipart/related";

// Helper for obtaining FileSystemContext, FileSystemURL, and mime type on the
// UI thread.
class URLHelper {
 public:
  // The scoped pointer to control lifetime of the instance itself. The pointer
  // is passed to callback functions and binds the lifetime of the instance to
  // the callback's lifetime.
  typedef std::unique_ptr<URLHelper> Lifetime;

  URLHelper(void* profile_id,
            const GURL& url,
            const ExternalFileURLRequestJob::HelperCallback& callback)
      : profile_id_(profile_id), url_(url), callback_(callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    Lifetime lifetime(this);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&URLHelper::RunOnUIThread, base::Unretained(this),
                   base::Passed(&lifetime)));
  }

 private:
  void RunOnUIThread(Lifetime lifetime) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!g_browser_process->profile_manager()->IsValidProfile(profile_id_)) {
      ReplyResult(net::ERR_FAILED);
      return;
    }
    Profile* const profile = reinterpret_cast<Profile*>(profile_id_);
    content::StoragePartition* const storage =
        content::BrowserContext::GetStoragePartitionForSite(profile, url_);
    DCHECK(storage);

    scoped_refptr<storage::FileSystemContext> context =
        storage->GetFileSystemContext();
    DCHECK(context.get());

    // Obtain the absolute path in the file system.
    const base::FilePath virtual_path = ExternalFileURLToVirtualPath(url_);

    // Obtain the file system URL.
    file_system_url_ = file_manager::util::CreateIsolatedURLFromVirtualPath(
        *context, /* empty origin */ GURL(), virtual_path);

    // Check if the obtained path providing external file URL or not.
    if (!file_system_url_.is_valid()) {
      ReplyResult(net::ERR_INVALID_URL);
      return;
    }

    isolated_file_system_scope_.reset(
        new ExternalFileURLRequestJob::IsolatedFileSystemScope(
            file_system_url_.filesystem_id()));

    if (!IsExternalFileURLType(file_system_url_.type())) {
      ReplyResult(net::ERR_FAILED);
      return;
    }

    file_system_context_ = context;
    extensions::app_file_handler_util::GetMimeTypeForLocalPath(
        profile,
        file_system_url_.path(),
        base::Bind(&URLHelper::OnGotMimeTypeOnUIThread,
                   base::Unretained(this),
                   base::Passed(&lifetime)));
  }

  void OnGotMimeTypeOnUIThread(Lifetime lifetime,
                               const std::string& mime_type) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    mime_type_ = mime_type;

    if (mime_type_ == kMimeTypeForRFC822)
      mime_type_ = kMimeTypeForMHTML;

    ReplyResult(net::OK);
  }

  void ReplyResult(net::Error error) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::Bind(callback_, error, file_system_context_,
                   base::Passed(&isolated_file_system_scope_), file_system_url_,
                   mime_type_));
  }

  void* const profile_id_;
  const GURL url_;
  const ExternalFileURLRequestJob::HelperCallback callback_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<ExternalFileURLRequestJob::IsolatedFileSystemScope>
      isolated_file_system_scope_;
  storage::FileSystemURL file_system_url_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(URLHelper);
};

}  // namespace

ExternalFileURLRequestJob::IsolatedFileSystemScope::IsolatedFileSystemScope(
    const std::string& file_system_id)
    : file_system_id_(file_system_id) {
}

ExternalFileURLRequestJob::IsolatedFileSystemScope::~IsolatedFileSystemScope() {
  storage::IsolatedContext::GetInstance()->RevokeFileSystem(file_system_id_);
}

ExternalFileURLRequestJob::ExternalFileURLRequestJob(
    void* profile_id,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      profile_id_(profile_id),
      range_parse_result_(net::OK),
      remaining_bytes_(0),
      weak_ptr_factory_(this) {}

void ExternalFileURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // Currently this job only cares about the Range header, and only supports
    // single range requests. Note that validation is deferred to Start,
    // because NotifyStartError is not legal to call since the job has not
    // started.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges) &&
        ranges.size() == 1) {
      byte_range_ = ranges[0];
    } else {
      range_parse_result_ = net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
    }
  }
}

void ExternalFileURLRequestJob::Start() {
  // Post a task to invoke StartAsync asynchronously to avoid re-entering the
  // delegate, because NotifyStartError is not legal to call synchronously in
  // Start().
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&ExternalFileURLRequestJob::StartAsync,
                            weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::StartAsync() {
  DVLOG(1) << "Starting request";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!stream_reader_);

  if (range_parse_result_ != net::OK) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           range_parse_result_));
    return;
  }

  // We only support GET request.
  if (request()->method() != "GET") {
    LOG(WARNING) << "Failed to start request: " << request()->method()
                 << " method is not supported";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }

  // Check if the scheme is correct.
  if (!request()->url().SchemeIs(content::kExternalFileScheme)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
    return;
  }

  // Owned by itself.
  new URLHelper(profile_id_,
                request()->url(),
                base::Bind(&ExternalFileURLRequestJob::OnHelperResultObtained,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnHelperResultObtained(
    net::Error error,
    const scoped_refptr<storage::FileSystemContext>& file_system_context,
    std::unique_ptr<IsolatedFileSystemScope> isolated_file_system_scope,
    const storage::FileSystemURL& file_system_url,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != net::OK) {
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
    return;
  }

  DCHECK(file_system_context.get());
  file_system_context_ = file_system_context;
  isolated_file_system_scope_ = std::move(isolated_file_system_scope);
  file_system_url_ = file_system_url;
  mime_type_ = mime_type;

  // Check if the entry has a redirect URL.
  file_system_context_->external_backend()->GetRedirectURLForContents(
      file_system_url_,
      base::Bind(&ExternalFileURLRequestJob::OnRedirectURLObtained,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnRedirectURLObtained(
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  redirect_url_ = redirect_url;
  if (!redirect_url_.is_empty()) {
    NotifyHeadersComplete();
    return;
  }

  // Obtain file system context.
  file_system_context_->operation_runner()->GetMetadata(
      file_system_url_,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::Bind(&ExternalFileURLRequestJob::OnFileInfoObtained,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnFileInfoObtained(
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result == base::File::FILE_ERROR_NOT_FOUND) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  if (result != base::File::FILE_OK || file_info.is_directory ||
      file_info.size < 0) {
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  // Compute content size.
  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }
  const int64_t offset = byte_range_.first_byte_position();
  const int64_t size =
      byte_range_.last_byte_position() + 1 - byte_range_.first_byte_position();
  set_expected_content_size(size);
  remaining_bytes_ = size;

  // Create file stream reader.
  stream_reader_ = file_system_context_->CreateFileStreamReader(
      file_system_url_, offset, size, base::Time());
  if (!stream_reader_) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  NotifyHeadersComplete();
}

void ExternalFileURLRequestJob::Kill() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  stream_reader_.reset();
  isolated_file_system_scope_.reset();
  file_system_context_ = NULL;
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool ExternalFileURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mime_type->assign(mime_type_);
  return !mime_type->empty();
}

bool ExternalFileURLRequestJob::IsRedirectResponse(
    GURL* location,
    int* http_status_code,
    bool* insecure_scheme_was_upgraded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (redirect_url_.is_empty())
    return false;

  // Redirect a hosted document.
  *insecure_scheme_was_upgraded = false;
  *location = redirect_url_;
  const int kHttpFound = 302;
  *http_status_code = kHttpFound;
  return true;
}

int ExternalFileURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(stream_reader_);

  if (remaining_bytes_ == 0)
    return 0;

  const int result = stream_reader_->Read(
      buf, std::min<int64_t>(buf_size, remaining_bytes_),
      base::Bind(&ExternalFileURLRequestJob::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr()));

  if (result < 0)
    return result;

  remaining_bytes_ -= result;
  return result;
}

ExternalFileURLRequestJob::~ExternalFileURLRequestJob() {
}

void ExternalFileURLRequestJob::OnReadCompleted(int read_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (read_result > 0)
    remaining_bytes_ -= read_result;

  ReadRawDataComplete(read_result);
}

}  // namespace chromeos
