// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_stream_writer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_file_stream_writer.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_file_system_url_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

ArcDocumentsProviderFileStreamWriter::ArcDocumentsProviderFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset)
    : offset_(offset), content_url_resolved_(false), arc_url_(url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

ArcDocumentsProviderFileStreamWriter::~ArcDocumentsProviderFileStreamWriter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

int ArcDocumentsProviderFileStreamWriter::Write(
    net::IOBuffer* buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!content_url_resolved_) {
    pending_operations_.emplace_back(base::BindOnce(
        &ArcDocumentsProviderFileStreamWriter::RunPendingWrite,
        weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(buffer),
        buffer_length, std::move(callback)));

    // Resolve the |arc_url_| to a Content URL to instantiate the underlying
    // writer.
    ResolveToContentUrlOnIOThread(
        arc_url_,
        base::BindOnce(
            &ArcDocumentsProviderFileStreamWriter::OnResolveToContentUrl,
            weak_ptr_factory_.GetWeakPtr()));

    return net::ERR_IO_PENDING;
  }
  if (!underlying_writer_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_writer_->Write(buffer, buffer_length, std::move(callback));
}

int ArcDocumentsProviderFileStreamWriter::Cancel(
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!content_url_resolved_) {
    pending_operations_.emplace_back(
        base::BindOnce(&ArcDocumentsProviderFileStreamWriter::RunPendingCancel,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  if (!underlying_writer_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_writer_->Cancel(std::move(callback));
}

int ArcDocumentsProviderFileStreamWriter::Flush(
    storage::FlushMode flush_mode,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!content_url_resolved_) {
    pending_operations_.emplace_back(base::BindOnce(
        &ArcDocumentsProviderFileStreamWriter::RunPendingFlush,
        weak_ptr_factory_.GetWeakPtr(), flush_mode, std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  if (!underlying_writer_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_writer_->Flush(flush_mode, std::move(callback));
}

void ArcDocumentsProviderFileStreamWriter::OnResolveToContentUrl(
    const GURL& content_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!content_url_resolved_);

  if (content_url.is_valid()) {
    underlying_writer_ = std::make_unique<ArcContentFileSystemFileStreamWriter>(
        content_url, offset_);
  }
  content_url_resolved_ = true;

  std::vector<base::OnceClosure> pending_operations;
  pending_operations.swap(pending_operations_);
  for (base::OnceClosure& callback : pending_operations)
    std::move(callback).Run();
}

void ArcDocumentsProviderFileStreamWriter::RunPendingWrite(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  // Create two copies of |callback| though it can still only called at most
  // once. This is safe because Write() is guaranteed not to call |callback| if
  // it returns synchronously.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = underlying_writer_
                   ? underlying_writer_->Write(buffer.get(), buffer_length,
                                               std::move(split_callback.first))
                   : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(result);
}

void ArcDocumentsProviderFileStreamWriter::RunPendingCancel(
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  // Create two copies of |callback| though it can still only called at most
  // once. This is safe because Cancel() is guaranteed not to call |callback| if
  // it returns synchronously.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = underlying_writer_
                   ? underlying_writer_->Cancel(std::move(split_callback.first))
                   : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(result);
}

void ArcDocumentsProviderFileStreamWriter::RunPendingFlush(
    storage::FlushMode flush_mode,
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  // Create two copies of |callback| though it can still only called at most
  // once. This is safe because Flush() is guaranteed not to call |callback| if
  // it returns synchronously.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = underlying_writer_
                   ? underlying_writer_->Flush(flush_mode,
                                               std::move(split_callback.first))
                   : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    std::move(split_callback.second).Run(result);
}

}  // namespace arc
