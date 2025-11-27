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
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_data_pipe_getter.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/multipart_uploader_base.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace safe_browsing {

// This class encapsulates the upload of a file with metadata using the
// multipart protocol. This class is neither movable nor copyable.
class MultipartUploadRequest
    : public enterprise_connectors::MultipartUploadRequestBase {
 public:
  // Creates a MultipartUploadRequest, which will upload `data` to the given
  // `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
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
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  // Creates a MultipartUploadRequest, which will upload the page in
  // `page_region` to the given `base_url` with `metadata` attached.
  MultipartUploadRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Callback callback);

  MultipartUploadRequest(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest& operator=(const MultipartUploadRequest&) = delete;
  MultipartUploadRequest(MultipartUploadRequest&&) = delete;
  MultipartUploadRequest& operator=(MultipartUploadRequest&&) = delete;

  ~MultipartUploadRequest() override;

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file,
      uint64_t file_size,
      bool is_obfuscated,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

  static std::unique_ptr<enterprise_connectors::ConnectorUploadRequest>
  CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page_region,
      const std::string& histogram_suffix,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      MultipartUploadRequest::Callback callback);

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

  scoped_refptr<base::TaskRunner> GetTaskRunner() override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_MULTIPART_UPLOADER_H_
