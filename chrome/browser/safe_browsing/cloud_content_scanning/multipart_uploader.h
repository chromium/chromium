// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/connector_upload_request.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace safe_browsing {

// This class encapsulates the upload of a file with metadata using the
// multipart protocol. This class is neither movable nor copyable.
class MultipartUploadRequest : public ConnectorUploadRequest {
 public:
  // Creates a MultipartUploadRequest, which will upload `data` to the given
  // `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a MultipartUploadRequest, which will upload the file corresponding
  // to `path` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a MultipartUploadRequest, which will upload the page in
  // `page_region` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  MultipartUploadRequest(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest& operator=(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest(MultipartUploadRequest&&) = delete;
  MultipartUploadRequest& operator=(MultipartUploadRequest&&) = delete;

  ~MultipartUploadRequest() override;

  // Start the upload. This must be called on the UI thread. When complete, this
  // will call `callback_` on the UI thread.
  void Start() override;

  std::string GetUploadInfo() override;

  static std::unique_ptr<ConnectorUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  void SetRequestHeaders(network::ResourceRequest* request);

  // Update `scan_type_` to be CONTENT to indicate that the content scan is
  // successful. Used in testing only.
  void MarkScanAsCompleteForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest, GeneratesCorrectBody);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest, RetriesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsNetworkRequestResponseCodeOrErrorHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsUploadSuccessHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadRequestTest,
                           EmitsRetriesNeededHistogram);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadDataPipeRequestTest, Retries);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadDataPipeRequestTest, DataControls);
  FRIEND_TEST_ALL_PREFIXES(MultipartUploadDataPipeRequestTest,
                           EquivalentToStringRequest);

  // Set the boundary between parts.
  void set_boundary(const std::string& boundary) { boundary_ = boundary; }

  // Helper method to create the multipart request body.
  std::string GenerateRequestBody(const std::string& metadata,
                                  const std::string& data);

  // Called whenever a net request finishes (on success or failure).
  void OnURLLoaderComplete(std::optional<std::string> response_body);

  // Called whenever a net request finishes (on success or failure).
  void RetryOrFinish(int net_error,
                     int response_code,
                     std::optional<std::string> response_body);

  // Called to send a single request. Is overridden in tests.
  virtual void SendRequest();
  void SendStringRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendFileRequest(std::unique_ptr<network::ResourceRequest> request);
  void SendPageRequest(std::unique_ptr<network::ResourceRequest> request);

  // Called after `data_pipe_getter_` has been initialized.
  void DataPipeCreatedCallback(
      std::unique_ptr<network::ResourceRequest> request,
      std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter);

  // Called by SendFileRequest and SendPageRequest after `data_pipe_getter_`
  // is known to be initialized to a correct state.
  virtual void CompleteSendRequest(
      std::unique_ptr<network::ResourceRequest> request);

  void CreateDatapipe(std::unique_ptr<network::ResourceRequest> request,
                      file_access::ScopedFileAccess file_access);

  std::string boundary_;

  base::TimeDelta current_backoff_;
  int retry_count_;

  base::Time start_time_;

  bool scan_complete_ = false;

  base::WeakPtrFactory<MultipartUploadRequest> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
