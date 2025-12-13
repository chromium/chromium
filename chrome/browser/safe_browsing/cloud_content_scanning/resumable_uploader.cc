// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace safe_browsing {

using ::enterprise_connectors::ConnectorUploadRequest;

// TODO(crbug.com/456489971): Replace BrowserThreadGuardImpl with
// content::GetUIThreadTaskRunner and call RunsTasksInCurrentSequence instead of
// DCHECK
ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 get_data_result,
                                 path,
                                 file_size,
                                 is_obfuscated,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload,
                                 content::GetUIThreadTaskRunner({})) {}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 get_data_result,
                                 std::move(page_region),
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload,
                                 content::GetUIThreadTaskRunner({})) {}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    enterprise_connectors::ConnectorUploadRequest::DataSource data_source,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 data,
                                 data_source,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload,
                                 content::GetUIThreadTaskRunner({})) {}

ResumableUploadRequest::~ResumableUploadRequest() = default;

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreateStringRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    enterprise_connectors::ConnectorUploadRequest::DataSource data_source,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreateStringRequest(
        url_loader_factory, base_url, metadata, data, data_source,
        histogram_suffix, traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, data, data_source,
      histogram_suffix, traffic_annotation,
      std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
}

std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreateFileRequest(
        url_loader_factory, base_url, metadata, get_data_result, path,
        file_size, is_obfuscated, histogram_suffix, traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, get_data_result, path, file_size,
      is_obfuscated, histogram_suffix, traffic_annotation,
      std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
}

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreatePageRequest(
        url_loader_factory, base_url, metadata, get_data_result,
        std::move(page_region), histogram_suffix, traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, get_data_result,
      std::move(page_region), histogram_suffix, traffic_annotation,
      std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
}

}  // namespace safe_browsing
