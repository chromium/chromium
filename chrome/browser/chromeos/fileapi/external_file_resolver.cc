// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_resolver.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "url/gurl.h"

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
  using Lifetime = std::unique_ptr<URLHelper>;
  using HelperCallback = base::OnceCallback<void(
      net::Error,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      file_manager::util::FileSystemURLAndHandle isolated_file_system,
      const std::string& mime_type)>;

  URLHelper(void* profile_id, const GURL& url, HelperCallback callback)
      : callback_(std::move(callback)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    Lifetime lifetime(this);
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&URLHelper::RunOnUIThread, base::Unretained(this),
                       std::move(lifetime), profile_id, url));
  }

 private:
  void RunOnUIThread(Lifetime lifetime, void* profile_id, const GURL& url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!g_browser_process->profile_manager()->IsValidProfile(profile_id)) {
      ReplyResult(net::ERR_FAILED);
      return;
    }
    Profile* const profile = reinterpret_cast<Profile*>(profile_id);
    content::StoragePartition* const storage =
        content::BrowserContext::GetStoragePartitionForSite(profile, url);
    DCHECK(storage);

    scoped_refptr<storage::FileSystemContext> context =
        storage->GetFileSystemContext();
    DCHECK(context.get());

    // Obtain the absolute path in the file system.
    const base::FilePath virtual_path = ExternalFileURLToVirtualPath(url);

    // Obtain the file system URL.
    isolated_file_system_ =
        file_manager::util::CreateIsolatedURLFromVirtualPath(
            *context, /* empty origin */ GURL(), virtual_path);

    // Check if the obtained path providing external file URL or not.
    if (!isolated_file_system_.url.is_valid()) {
      ReplyResult(net::ERR_INVALID_URL);
      return;
    }

    if (!IsExternalFileURLType(isolated_file_system_.url.type())) {
      ReplyResult(net::ERR_FAILED);
      return;
    }

    file_system_context_ = std::move(context);
    extensions::app_file_handler_util::GetMimeTypeForLocalPath(
        profile, isolated_file_system_.url.path(),
        base::BindRepeating(&URLHelper::OnGotMimeTypeOnUIThread,
                            base::Unretained(this), base::Passed(&lifetime)));
  }

  void OnGotMimeTypeOnUIThread(Lifetime lifetime,
                               const std::string& mime_type) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    mime_type_ = mime_type;

    if (mime_type_ == kMimeTypeForRFC822)
      mime_type_ = kMimeTypeForMHTML;

    ReplyResult(net::OK);
  }

  void ReplyResult(net::Error error) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(std::move(callback_), error,
                       std::move(file_system_context_),
                       std::move(isolated_file_system_), mime_type_));
  }

  HelperCallback callback_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  file_manager::util::FileSystemURLAndHandle isolated_file_system_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(URLHelper);
};

}  // namespace

ExternalFileResolver::ExternalFileResolver(void* profile_id)
    : profile_id_(profile_id), range_parse_result_(net::OK) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ExternalFileResolver::~ExternalFileResolver() = default;

void ExternalFileResolver::ProcessHeaders(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Read the range header if present.
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // Currently this job only cares about the Range header, and only supports
    // single range requests.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges) &&
        ranges.size() == 1) {
      byte_range_ = ranges[0];
    } else {
      range_parse_result_ = net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
    }
  }
}

void ExternalFileResolver::Resolve(const std::string& method,
                                   const GURL& url,
                                   ErrorCallback error_callback,
                                   RedirectCallback redirect_callback,
                                   StreamCallback stream_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  error_callback_ = std::move(error_callback);
  redirect_callback_ = std::move(redirect_callback);
  stream_callback_ = std::move(stream_callback);

  if (range_parse_result_ != net::OK) {
    std::move(error_callback_).Run(range_parse_result_);
    return;
  }

  // We only support GET request.
  if (method != "GET") {
    LOG(WARNING) << "Failed to start request: " << method
                 << " method is not supported";
    std::move(error_callback_).Run(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  // Check if the scheme is correct.
  if (!url.SchemeIs(content::kExternalFileScheme)) {
    std::move(error_callback_).Run(net::ERR_INVALID_URL);
    return;
  }

  // Owned by itself.
  new URLHelper(profile_id_, url,
                base::BindOnce(&ExternalFileResolver::OnHelperResultObtained,
                               weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileResolver::OnHelperResultObtained(
    net::Error error,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    file_manager::util::FileSystemURLAndHandle isolated_file_system,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != net::OK) {
    std::move(error_callback_).Run(error);
    return;
  }

  DCHECK(file_system_context.get());
  isolated_file_system_ = std::move(isolated_file_system);
  mime_type_ = mime_type;

  // Check if the entry has a redirect URL.
  file_system_context_ = std::move(file_system_context);
  file_system_context_->external_backend()->GetRedirectURLForContents(
      isolated_file_system_.url,
      base::BindRepeating(&ExternalFileResolver::OnRedirectURLObtained,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileResolver::OnRedirectURLObtained(const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!redirect_url.is_empty()) {
    std::move(redirect_callback_).Run(mime_type_, redirect_url);
    return;
  }

  // If there's no redirect then we're serving the file from the file system.
  file_system_context_->operation_runner()->GetMetadata(
      isolated_file_system_.url,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindOnce(&ExternalFileResolver::OnFileInfoObtained,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileResolver::OnFileInfoObtained(
    base::File::Error error,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (error == base::File::FILE_ERROR_NOT_FOUND) {
    std::move(error_callback_).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }

  if (error != base::File::FILE_OK || file_info.is_directory ||
      file_info.size < 0) {
    std::move(error_callback_).Run(net::ERR_FAILED);
    return;
  }

  // Compute content size.
  if (!byte_range_.ComputeBounds(file_info.size)) {
    std::move(error_callback_).Run(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }
  const int64_t offset = byte_range_.first_byte_position();
  const int64_t remaining_bytes = byte_range_.last_byte_position() - offset + 1;

  std::unique_ptr<storage::FileStreamReader> stream_reader =
      file_system_context_->CreateFileStreamReader(
          isolated_file_system_.url, offset, remaining_bytes, base::Time());
  if (!stream_reader) {
    std::move(error_callback_).Run(net::ERR_FILE_NOT_FOUND);
    return;
  }

  std::move(stream_callback_)
      .Run(mime_type_, std::move(isolated_file_system_.handle),
           std::move(stream_reader), remaining_bytes);
}

}  // namespace chromeos
