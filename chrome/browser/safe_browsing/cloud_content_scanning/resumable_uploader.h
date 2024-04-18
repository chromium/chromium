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
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_upload_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {

// This class encapsulates the upload of a file with metadata using the
// resumable protocol. This class is neither movable nor copyable.
class ResumableUploadRequest : public ConnectorUploadRequest {
 public:
  // Creates a ResumableUploadRequest, which will upload the `metadata` of the
  // file corresponding to the provided `path` to the given `base_url`, and then
  // the file content to the `path` if necessary.
  //
  // `get_data_result` is the result when getting basic information about the
  // file or page.  It lets the ResumableUploadRequest know if the data is
  // considered too large or is encrypted.
  ResumableUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
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
      BinaryUploadService::Result get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  ResumableUploadRequest(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest& operator=(const ResumableUploadRequest&) = delete;
  ResumableUploadRequest(ResumableUploadRequest&&) = delete;
  ResumableUploadRequest& operator=(ResumableUploadRequest&&) = delete;

  ~ResumableUploadRequest() override;

  static std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      const base::FilePath& file,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback);

  static std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ResumableUploadRequest::Callback callback);

  // Set the headers for the given metadata `request`.
  void SetMetadataRequestHeaders(network::ResourceRequest* request);

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  void Start() override;

  std::string GetUploadInfo() override;

 private:
  // Send the metadata information about the file/page to the server.
  void SendMetadataRequest();

  // Called whenever a metadata request finishes (on success or failure).
  void OnMetadataUploadCompleted(base::TimeTicks start_time,
                                 std::optional<std::string> response_body);

  // Initialize `data_pipe_getter_`
  void CreateDatapipe(std::unique_ptr<network::ResourceRequest> request,
                      file_access::ScopedFileAccess file_access);

  // Called after `data_pipe_getter_` has been initialized.
  void OnDataPipeCreated(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter);

  // Called after a metadata request finishes successfully and provides a
  // `upload_url_`.
  void SendContentSoon();

  // Called after `data_pipe_getter_` is known to be initialized to a correct
  // state.
  void SendContentNow(std::unique_ptr<network::ResourceRequest> request);

  // Called whenever a content request finishes (on success or failure).
  void OnSendContentCompleted(base::TimeTicks start_time,
                              std::optional<std::string> response_body);

  // Returns true if all of the following conditions are met:
  //    1. The HTTP status is OK.
  //    2. The `headers` have `upload_status` and `upload_url`.
  //    3. The `upload_status` is "active".
  bool CanUploadContent(const scoped_refptr<net::HttpResponseHeaders>& headers);

  // Called whenever a net request finishes (on success or failure).
  void Finish(int net_error,
              int response_code,
              std::optional<std::string> response_body);

  // Helper used by metrics logging code.
  std::string GetRequestType();

  // The result returned by BinaryUploadService::Request::GetRequestData() when
  // retrieving the data.
  BinaryUploadService::Result get_data_result_;

  // Retrieved from metadata response to be used in upload content to the
  // server.
  std::string upload_url_;
  enum {
    PENDING = 0,
    METADATA_ONLY = 1,
    FULL_CONTENT = 2
  } scan_type_ = PENDING;
  base::WeakPtrFactory<ResumableUploadRequest> weak_factory_{this};
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_RESUMABLE_UPLOADER_H_
