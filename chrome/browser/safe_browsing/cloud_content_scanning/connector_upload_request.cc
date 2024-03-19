// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_upload_request.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

// static
ConnectorUploadRequestFactory* ConnectorUploadRequest::factory_ = nullptr;

ConnectorUploadRequest::ConnectorUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_source_(STRING),
      data_(data),
      callback_(std::move(callback)),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ConnectorUploadRequest::ConnectorUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_source_(FILE),
      path_(path),
      data_size_(file_size),
      callback_(std::move(callback)),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ConnectorUploadRequest::ConnectorUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_source_(PAGE),
      page_region_(std::move(page_region)),
      data_size_(page_region_.GetSize()),
      callback_(std::move(callback)),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ConnectorUploadRequest::~ConnectorUploadRequest() {
  // Take ownership of the file in `data_pipe_getter_` if there is one to close
  // it on another thread since it makes blocking calls.
  if (!data_pipe_getter_) {
    return;
  }

  auto file = data_pipe_getter_->ReleaseFile();
  if (file) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](std::unique_ptr<
                ConnectorDataPipeGetter::InternalMemoryMappedFile> file) {},
            std::move(file)));
  }
}

void ConnectorUploadRequest::set_access_token(const std::string& access_token) {
  access_token_ = access_token;
}
}  // namespace safe_browsing
