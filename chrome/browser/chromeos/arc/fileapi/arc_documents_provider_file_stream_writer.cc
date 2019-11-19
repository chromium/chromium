// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_file_stream_writer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_writer.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace arc {

namespace {

void OnResolveToContentUrlOnUIThread(
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(callback), url));
}

void ResolveToContentUrlOnUIThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderRoot::ResolveToContentUrlCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnResolveToContentUrlOnUIThread(std::move(callback), GURL());
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnResolveToContentUrlOnUIThread(std::move(callback), GURL());
    return;
  }

  root->ResolveToContentUrl(
      path,
      base::BindOnce(&OnResolveToContentUrlOnUIThread, std::move(callback)));
}

}  // namespace

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
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &ResolveToContentUrlOnUIThread, arc_url_,
            base::BindOnce(
                &ArcDocumentsProviderFileStreamWriter::OnResolveToContentUrl,
                weak_ptr_factory_.GetWeakPtr())));

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
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!content_url_resolved_) {
    pending_operations_.emplace_back(
        base::BindOnce(&ArcDocumentsProviderFileStreamWriter::RunPendingFlush,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return net::ERR_IO_PENDING;
  }
  if (!underlying_writer_)
    return net::ERR_FILE_NOT_FOUND;
  return underlying_writer_->Flush(std::move(callback));
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
  // Create |copyable_callback| which is copyable, though it can still only
  // called at most once.  This is safe, because Write() is guaranteed not to
  // call |callback| if it returns synchronously.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  int result = underlying_writer_
                   ? underlying_writer_->Write(buffer.get(), buffer_length,
                                               copyable_callback)
                   : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    copyable_callback.Run(result);
}

void ArcDocumentsProviderFileStreamWriter::RunPendingCancel(
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  // Create |copyable_callback| which is copyable, though it can still only
  // called at most once.  This is safe, because Cancel() is guaranteed not to
  // call |callback| if it returns synchronously.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  int result = underlying_writer_
                   ? underlying_writer_->Cancel(copyable_callback)
                   : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    copyable_callback.Run(result);
}

void ArcDocumentsProviderFileStreamWriter::RunPendingFlush(
    net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(content_url_resolved_);
  // Create |copyable_callback| which is copyable, though it can still only
  // called at most once.  This is safe, because Flush() is guaranteed not to
  // call |callback| if it returns synchronously.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  int result = underlying_writer_ ? underlying_writer_->Flush(copyable_callback)
                                  : net::ERR_FILE_NOT_FOUND;
  if (result != net::ERR_IO_PENDING)
    copyable_callback.Run(result);
}

}  // namespace arc
