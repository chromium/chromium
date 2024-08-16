// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class FakeUploader : public MultipartUploadRequest {
 public:
  FakeUploader(const GURL& base_url,
               const std::string& metadata,
               const base::FilePath& file_path,
               uint64_t file_size,
               Callback finish_callback,
               const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~FakeUploader() override {}

  void Start() override { start_called_ = true; }

  GURL base_url_;
  std::string metadata_;
  base::FilePath file_path_;
  Callback finish_callback_;

  bool start_called_;
};

FakeUploader::FakeUploader(
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    uint64_t file_size,
    Callback finish_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : MultipartUploadRequest(/*url_loader_factory=*/nullptr,
                             base_url,
                             metadata,
                             file_path,
                             file_size,
                             traffic_annotation,
                             base::DoNothing()),
      base_url_(base_url),
      metadata_(metadata),
      file_path_(file_path),
      finish_callback_(std::move(finish_callback)),
      start_called_(false) {}

class FakeUploaderFactory : public ConnectorUploadRequestFactory {
 public:
  FakeUploaderFactory() : uploader_(nullptr) {}
  ~FakeUploaderFactory() override {}

  std::unique_ptr<ConnectorUploadRequest> CreateStringRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      const std::string& data,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override;
  std::unique_ptr<ConnectorUploadRequest> CreateFileRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      const base::FilePath& file_path,
      uint64_t file_size,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override;
  std::unique_ptr<ConnectorUploadRequest> CreatePageRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& base_url,
      const std::string& metadata,
      BinaryUploadService::Result get_data_result,
      base::ReadOnlySharedMemoryRegion page_region,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ConnectorUploadRequest::Callback callback) override;

  raw_ptr<FakeUploader, DanglingUntriaged> uploader_;
};

std::unique_ptr<ConnectorUploadRequest>
FakeUploaderFactory::CreateStringRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ConnectorUploadRequest::Callback callback) {
  NOTREACHED();
}

std::unique_ptr<ConnectorUploadRequest> FakeUploaderFactory::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
    const base::FilePath& file_path,
    uint64_t file_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ConnectorUploadRequest::Callback callback) {
  EXPECT_FALSE(uploader_);

  auto uploader =
      std::make_unique<FakeUploader>(base_url, metadata, file_path, file_size,
                                     std::move(callback), traffic_annotation);
  uploader_ = uploader.get();
  return uploader;
}
std::unique_ptr<ConnectorUploadRequest> FakeUploaderFactory::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ConnectorUploadRequest::Callback callback) {
  NOTREACHED();
}

}  // namespace

class DownloadFeedbackTest : public testing::Test {
 public:
  DownloadFeedbackTest()
      : file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
        io_task_runner_(content::GetIOThreadTaskRunner({})),
        feedback_finish_called_(false) {
    EXPECT_NE(io_task_runner_, file_task_runner_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    upload_file_path_ = temp_dir_.GetPath().AppendASCII("test file");
    upload_file_data_ = "data";
    ASSERT_TRUE(base::WriteFile(upload_file_path_, upload_file_data_));
    ConnectorUploadRequest::RegisterFactoryForTests(&uploader_factory_);
  }

  void TearDown() override {
    ConnectorUploadRequest::RegisterFactoryForTests(nullptr);
  }

  FakeUploader* uploader() const { return uploader_factory_.uploader_; }

  void FinishCallback() {
    EXPECT_FALSE(feedback_finish_called_);
    feedback_finish_called_ = true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath upload_file_path_;
  std::string upload_file_data_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  FakeUploaderFactory uploader_factory_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;

  bool feedback_finish_called_;
};

TEST_F(DownloadFeedbackTest, CompleteUpload) {
  ClientDownloadReport expected_report_metadata;
  auto* request = expected_report_metadata.mutable_download_request();
  request->set_url("http://test");
  request->set_length(upload_file_data_.size());
  request->mutable_digests()->set_sha1("hi");
  expected_report_metadata.mutable_download_response()->set_verdict(
      ClientDownloadResponse::DANGEROUS_HOST);
  std::string ping_request(
      expected_report_metadata.download_request().SerializeAsString());
  std::string ping_response(
      expected_report_metadata.download_response().SerializeAsString());

  std::unique_ptr<DownloadFeedback> feedback = DownloadFeedback::Create(
      shared_url_loader_factory_, upload_file_path_, upload_file_data_.size(),
      ping_request, ping_response);
  EXPECT_FALSE(uploader());

  feedback->Start(base::BindOnce(&DownloadFeedbackTest::FinishCallback,
                                 base::Unretained(this)));
  ASSERT_TRUE(uploader());
  EXPECT_FALSE(feedback_finish_called_);
  EXPECT_TRUE(uploader()->start_called_);

  EXPECT_EQ(upload_file_path_, uploader()->file_path_);
  EXPECT_EQ(expected_report_metadata.SerializeAsString(),
            uploader()->metadata_);
  EXPECT_EQ(DownloadFeedback::kSbFeedbackURL, uploader()->base_url_.spec());

  EXPECT_TRUE(base::PathExists(upload_file_path_));

  EXPECT_FALSE(feedback_finish_called_);
  std::move(uploader()->finish_callback_)
      .Run(/*success=*/true, net::HTTP_OK, "");
  EXPECT_TRUE(feedback_finish_called_);
  feedback.reset();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(base::PathExists(upload_file_path_));
}

TEST_F(DownloadFeedbackTest, CancelUpload) {
  ClientDownloadReport expected_report_metadata;
  auto* request = expected_report_metadata.mutable_download_request();
  request->set_url("http://test");
  request->set_length(upload_file_data_.size());
  request->mutable_digests()->set_sha1("hi");
  expected_report_metadata.mutable_download_response()->set_verdict(
      ClientDownloadResponse::DANGEROUS_HOST);
  std::string ping_request(
      expected_report_metadata.download_request().SerializeAsString());
  std::string ping_response(
      expected_report_metadata.download_response().SerializeAsString());

  std::unique_ptr<DownloadFeedback> feedback = DownloadFeedback::Create(
      shared_url_loader_factory_, upload_file_path_, upload_file_data_.size(),
      ping_request, ping_response);
  EXPECT_FALSE(uploader());

  feedback->Start(base::BindOnce(&DownloadFeedbackTest::FinishCallback,
                                 base::Unretained(this)));
  ASSERT_TRUE(uploader());
  EXPECT_FALSE(feedback_finish_called_);
  EXPECT_TRUE(uploader()->start_called_);
  EXPECT_TRUE(base::PathExists(upload_file_path_));

  feedback.reset();
  EXPECT_FALSE(feedback_finish_called_);

  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(base::PathExists(upload_file_path_));
}

}  // namespace safe_browsing
