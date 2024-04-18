// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_UPLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_UPLOAD_REQUEST_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/file_access/scoped_file_access.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class ConnectorUploadRequestFactory;

// This class encapsulates the base class of uploading metadata and file.
// This class is neither movable nor copyable.
class ConnectorUploadRequest {
 public:
  using Callback = base::OnceCallback<
      void(bool success, int http_status, const std::string& response_data)>;

  // Makes the passed `factory` the factory used to instantiate a
  // MultipartUploadRequest. Useful for tests.
  static void RegisterFactoryForTests(ConnectorUploadRequestFactory* factory) {
    factory_ = factory;
  }

  // Creates a ConnectorUploadRequest, which will upload `metadata` and `data`
  // to the given `base_url`.
  ConnectorUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a ConnectorUploadRequest, which will upload `metadata` and the file
  // corresponding to `path` to the given `base_url`.
  ConnectorUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a  ConnectorUploadRequest, which will upload `metadata` and the
  // page in `page_region` to the given `base_url`.
  ConnectorUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  ConnectorUploadRequest(const ConnectorUploadRequest&) = delete;
  ConnectorUploadRequest& operator=(const ConnectorUploadRequest&) = delete;
  ConnectorUploadRequest(ConnectorUploadRequest&&) = delete;
  ConnectorUploadRequest& operator=(ConnectorUploadRequest&&) = delete;

  virtual ~ConnectorUploadRequest();

  ConnectorDataPipeGetter* data_pipe_getter_for_testing() {
    return data_pipe_getter_.get();
  }

  void set_access_token(const std::string& access_token);

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  virtual void Start() = 0;

  // Return the upload protocol and scanning type of the request. E.g.,
  // "Multipart - Pending", "Multipart - Complete", "Resumable - Pending",
  // "Resumable - Metadata only scan", "Resumable - Full content scan".
  virtual std::string GetUploadInfo() = 0;

 protected:
  static ConnectorUploadRequestFactory* factory_;

  GURL base_url_;
  std::string metadata_;

  // Indicates what the source of the data to upload is.
  const enum { STRING = 0, FILE = 1, PAGE = 2 } data_source_;

  // String of content to upload. Only populated for STRING requests.
  std::string data_;

  // Path to read the file to upload. Only populated for FILE requests.
  base::FilePath path_;

  // Memory to upload. Only populated for PAGE requests.
  base::ReadOnlySharedMemoryRegion page_region_;

  // Size of the file or page region.
  uint64_t data_size_ = 0;

  // Data pipe getter used to stream a file or a page. Only populated for the
  // corresponding requests.
  std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter_;

  Callback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;

  std::string access_token_;

  std::unique_ptr<file_access::ScopedFileAccess> scoped_file_access_;
};

class ConnectorUploadRequestFactory {
 public:
  virtual ~ConnectorUploadRequestFactory() = default;
  virtual std::unique_ptr<ConnectorUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) = 0;
  virtual std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) = 0;
  virtual std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_UPLOAD_REQUEST_H_
