// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "content/public/test/browser_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;

class ResumableUploadRequestTest : public testing::Test {
 public:
  base::FilePath CreateFile(const std::string& file_name,
                            const std::string& content) {
    if (!temp_dir_.IsValid()) {
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    }

    base::FilePath path = temp_dir_.GetPath().AppendASCII(file_name);
    base::File file(path, base::File::FLAG_CREATE_ALWAYS |
                              base::File::FLAG_READ | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(content.data(), content.size());
    return path;
  }

  base::ReadOnlySharedMemoryRegion CreatePage(const std::string& content) {
    base::MappedReadOnlyRegion region =
        base::ReadOnlySharedMemoryRegion::Create(content.size());
    EXPECT_TRUE(region.IsValid());
    std::memcpy(region.mapping.memory(), content.data(), content.size());
    return std::move(region.region);
  }

  void VerifyMetadataRequestHeaders(
      const network::ResourceRequest& resource_request,
      std::string expected_size) {
    std::string header_value;

    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Protocol"));
    ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Protocol",
                                                   &header_value));
    ASSERT_EQ(header_value, "resumable");

    ASSERT_TRUE(resource_request.headers.HasHeader("X-Goog-Upload-Command"));
    ASSERT_TRUE(resource_request.headers.GetHeader("X-Goog-Upload-Command",
                                                   &header_value));
    ASSERT_EQ(header_value, "start");

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Type"));
    ASSERT_TRUE(resource_request.headers.GetHeader(
        "X-Goog-Upload-Header-Content-Type", &header_value));
    ASSERT_EQ(header_value, "application/octet-stream");

    ASSERT_TRUE(resource_request.headers.HasHeader(
        "X-Goog-Upload-Header-Content-Length"));
    ASSERT_TRUE(resource_request.headers.GetHeader(
        "X-Goog-Upload-Header-Content-Length", &header_value));
    ASSERT_EQ(header_value, expected_size);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_FileRequest) {
  network::ResourceRequest resource_request;
  std::unique_ptr<ResumableUploadRequest> request =
      ResumableUploadRequest::CreateFileRequest(
          nullptr, GURL(), "metadata",
          CreateFile("my_file_name.foo", "file_data"), 9,
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "9");
}

TEST_F(ResumableUploadRequestTest,
       GeneratesCorrectMetadataHeaders_PageRequest) {
  network::ResourceRequest resource_request;
  std::unique_ptr<ResumableUploadRequest> request =
      ResumableUploadRequest::CreatePageRequest(
          nullptr, GURL(), "metadata", CreatePage("print_data"),
          TRAFFIC_ANNOTATION_FOR_TESTS, base::DoNothing());
  request->SetMetadataRequestHeaders(&resource_request);

  VerifyMetadataRequestHeaders(std::move(resource_request), "10");
}

}  // namespace safe_browsing
