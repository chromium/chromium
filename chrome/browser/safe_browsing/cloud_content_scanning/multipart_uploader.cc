// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

using ::enterprise_connectors::BrowserThreadGuard;
using ::enterprise_connectors::ConnectorUploadRequest;

class BrowserThreadGuardImpl : public BrowserThreadGuard {
 public:
  void AssertCalledOnUIThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }
};

}  // namespace

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : MultipartUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 data,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(callback),
                                 std::make_unique<BrowserThreadGuardImpl>()) {}

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : MultipartUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 path,
                                 file_size,
                                 is_obfuscated,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(callback),
                                 std::make_unique<BrowserThreadGuardImpl>()) {}

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : MultipartUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 std::move(page_region),
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(callback),
                                 std::make_unique<BrowserThreadGuardImpl>()) {}

MultipartUploadRequest::~MultipartUploadRequest() = default;

scoped_refptr<base::TaskRunner> MultipartUploadRequest::GetTaskRunner() {
  return content::GetUIThreadTaskRunner({});
}

// static
std::unique_ptr<ConnectorUploadRequest>
MultipartUploadRequest::CreateStringRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, data, histogram_suffix,
        traffic_annotation, std::move(callback));
  }

  return factory_->CreateStringRequest(url_loader_factory, base_url, metadata,
                                       data, histogram_suffix,
                                       traffic_annotation, std::move(callback));
}

// static
std::unique_ptr<ConnectorUploadRequest>
MultipartUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, path, file_size, is_obfuscated,
        histogram_suffix, traffic_annotation, std::move(callback));
  }

  // Note that multipart uploads only handle data that is less than
  // `kMaxUploadSizeBytes` and not encrypted.  Therefore `Result::SUCCESS` is
  // passed as the `get_data_result` argument.
  return factory_->CreateFileRequest(
      url_loader_factory, base_url, metadata,
      enterprise_connectors::ScanRequestUploadResult::SUCCESS, path, file_size,
      is_obfuscated, histogram_suffix, traffic_annotation, std::move(callback));
}

// static
std::unique_ptr<ConnectorUploadRequest>
MultipartUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, std::move(page_region),
        histogram_suffix, traffic_annotation, std::move(callback));
  }

  return factory_->CreatePageRequest(
      url_loader_factory, base_url, metadata,
      enterprise_connectors::ScanRequestUploadResult::SUCCESS,
      std::move(page_region), histogram_suffix, traffic_annotation,
      std::move(callback));
}
}  // namespace safe_browsing
