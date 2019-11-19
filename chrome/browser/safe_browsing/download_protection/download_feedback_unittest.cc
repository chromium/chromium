// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/download_protection/two_phase_uploader.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class FakeUploader : public TwoPhaseUploader {
 public:
  FakeUploader(base::TaskRunner* file_task_runner,
               const GURL& base_url,
               const std::string& metadata,
               const base::FilePath& file_path,
               const FinishCallback& finish_callback);
  ~FakeUploader() override {}

  void Start() override { start_called_ = true; }

  scoped_refptr<base::TaskRunner> file_task_runner_;
  GURL base_url_;
  std::string metadata_;
  base::FilePath file_path_;
  FinishCallback finish_callback_;

  bool start_called_;
};

FakeUploader::FakeUploader(base::TaskRunner* file_task_runner,
                           const GURL& base_url,
                           const std::string& metadata,
                           const base::FilePath& file_path,
                           const FinishCallback& finish_callback)
    : file_task_runner_(file_task_runner),
      base_url_(base_url),
      metadata_(metadata),
      file_path_(file_path),
      finish_callback_(finish_callback),
      start_called_(false) {}

class FakeUploaderFactory : public TwoPhaseUploaderFactory {
 public:
  FakeUploaderFactory() : uploader_(nullptr) {}
  ~FakeUploaderFactory() override {}

  std::unique_ptr<TwoPhaseUploader> CreateTwoPhaseUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      const TwoPhaseUploader::FinishCallback& finish_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  FakeUploader* uploader_;
};

std::unique_ptr<TwoPhaseUploader> FakeUploaderFactory::CreateTwoPhaseUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TaskRunner* file_task_runner,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    const TwoPhaseUploader::FinishCallback& finish_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  EXPECT_FALSE(uploader_);

  uploader_ = new FakeUploader(file_task_runner, base_url, metadata, file_path,
                               finish_callback);
  return base::WrapUnique(uploader_);
}

}  // namespace

class DownloadFeedbackTest : public testing::Test {
 public:
  DownloadFeedbackTest()
      : file_task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::BEST_EFFORT})),
        io_task_runner_(
            base::CreateSingleThreadTaskRunner({content::BrowserThread::IO})),
        feedback_finish_called_(false) {
    EXPECT_NE(io_task_runner_, file_task_runner_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    upload_file_path_ = temp_dir_.GetPath().AppendASCII("test file");
    upload_file_data_ = "data";
    ASSERT_EQ(static_cast<int>(upload_file_data_.size()),
              base::WriteFile(upload_file_path_, upload_file_data_.data(),
                              upload_file_data_.size()));
    TwoPhaseUploader::RegisterFactory(&two_phase_uploader_factory_);
  }

  void TearDown() override { TwoPhaseUploader::RegisterFactory(nullptr); }

  FakeUploader* uploader() const {
    return two_phase_uploader_factory_.uploader_;
  }

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
  FakeUploaderFactory two_phase_uploader_factory_;
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
      shared_url_loader_factory_, file_task_runner_.get(), upload_file_path_,
      ping_request, ping_response);
  EXPECT_FALSE(uploader());

  feedback->Start(base::Bind(&DownloadFeedbackTest::FinishCallback,
                             base::Unretained(this)));
  ASSERT_TRUE(uploader());
  EXPECT_FALSE(feedback_finish_called_);
  EXPECT_TRUE(uploader()->start_called_);

  EXPECT_EQ(file_task_runner_, uploader()->file_task_runner_);
  EXPECT_EQ(upload_file_path_, uploader()->file_path_);
  EXPECT_EQ(expected_report_metadata.SerializeAsString(),
            uploader()->metadata_);
  EXPECT_EQ(DownloadFeedback::kSbFeedbackURL, uploader()->base_url_.spec());

  EXPECT_TRUE(base::PathExists(upload_file_path_));

  EXPECT_FALSE(feedback_finish_called_);
  uploader()->finish_callback_.Run(TwoPhaseUploader::STATE_SUCCESS, net::OK, 0,
                                   "");
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
      shared_url_loader_factory_, file_task_runner_.get(), upload_file_path_,
      ping_request, ping_response);
  EXPECT_FALSE(uploader());

  feedback->Start(base::Bind(&DownloadFeedbackTest::FinishCallback,
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
