// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_upload_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

class ResumableUploadRequestFactory;

// This class encapsulates the upload of a file with metadata using the
// resumable protocol. This class is neither movable nor copyable.
class ResumableUploadRequest : public ConnectorUploadRequest {
 public:
  using Callback = ConnectorUploadRequest::Callback;

  // Creates a ResumableUploadRequest, which will upload the `metadata` of the
  // file corresponding to the provided `path` to the given `base_url`, and then
  // the file content to the `path` if necessary.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a ResumableUploadRequest, which will upload the `metadata` of the
  // page to the given `base_url`, and then the content of `page_region` if
  // necessary.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  ResumableUploadRequest(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest& operator=(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest(ResumableUploadRequest&&) = delete;
  ResumableUploadRequest& operator=(ResumableUploadRequest&&) = delete;

  ~ResumableUploadRequest() override;

  // Makes the passed `factory` the factory used to instantiate a
  // ResumableUploadRequest. Useful for tests.
  static void RegisterFactoryForTests(ResumableUploadRequestFactory* factory) {
    factory_ = factory;
  }

  static std::unique_ptr<ResumableUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback);

  static std::unique_ptr<ResumableUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback);

  // Set the headers for the given metadata `request`.
  void SetMetadataRequestHeaders(network::ResourceRequest* request);

 private:
  void SendMetadataRequest();

  static ResumableUploadRequestFactory* factory_;

  base::WeakPtrFactory<ResumableUploadRequest> weak_factory_{this};
};

// TODO(b/322005479): Consider combining the factory methods and share them with
// MultipartUploadRequest.
class ResumableUploadRequestFactory {
 public:
  virtual ~ResumableUploadRequestFactory() = default;
  virtual std::unique_ptr<ResumableUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback) = 0;
  virtual std::unique_ptr<ResumableUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
