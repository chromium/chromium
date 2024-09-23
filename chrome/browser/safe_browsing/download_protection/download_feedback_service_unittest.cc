// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace safe_browsing {

namespace {

class FakeDownloadFeedback : public DownloadFeedback {
 public:
  FakeDownloadFeedback(const std::string& ping_request,
                       const std::string& ping_response,
                       base::OnceClosure deletion_callback)
      : ping_request_(ping_request),
        ping_response_(ping_response),
        deletion_callback_(std::move(deletion_callback)),
        start_called_(false) {}

  ~FakeDownloadFeedback() override { std::move(deletion_callback_).Run(); }

  void Start(base::OnceClosure finish_callback) override {
    start_called_ = true;
    finish_callback_ = std::move(finish_callback);
  }

  const std::string& GetPingRequestForTesting() const override {
    return ping_request_;
  }

  const std::string& GetPingResponseForTesting() const override {
    return ping_response_;
  }

  base::OnceClosure finish_callback() { return std::move(finish_callback_); }

  bool start_called() const { return start_called_; }

 private:
  scoped_refptr<base::TaskRunner> file_task_runner_;
  std::string ping_request_;
  std::string ping_response_;

  base::OnceClosure finish_callback_;
  base::OnceClosure deletion_callback_;
  bool start_called_;
};

class FakeDownloadFeedbackFactory : public DownloadFeedbackFactory {
 public:
  ~FakeDownloadFeedbackFactory() override {}

  std::unique_ptr<DownloadFeedback> CreateDownloadFeedback(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& file_path,
      uint64_t file_size,
      const std::string& ping_request,
      const std::string& ping_response) override {
    FakeDownloadFeedback* feedback = new FakeDownloadFeedback(
        ping_request, ping_response,
        base::BindOnce(&FakeDownloadFeedbackFactory::DownloadFeedbackSent,
                       base::Unretained(this), feedbacks_.size()));
    feedbacks_.push_back(feedback);
    return base::WrapUnique(feedback);
  }

  void DownloadFeedbackSent(size_t n) { feedbacks_[n] = nullptr; }

  FakeDownloadFeedback* feedback(size_t n) const { return feedbacks_[n]; }

  size_t num_feedbacks() const { return feedbacks_.size(); }

 private:
  std::vector<raw_ptr<FakeDownloadFeedback, VectorExperimental>> feedbacks_;
};

class FakeDownloadProtectionService : public DownloadProtectionService {
 public:
  FakeDownloadProtectionService() : DownloadProtectionService(nullptr) {}

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context) override {
    return nullptr;
  }
};

}  // namespace

class DownloadFeedbackServiceTest : public testing::Test {
 public:
  DownloadFeedbackServiceTest()
      : file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    DownloadFeedback::RegisterFactory(&download_feedback_factory_);
  }

  void TearDown() override { DownloadFeedback::RegisterFactory(nullptr); }

  base::FilePath CreateTestFile(int n) const {
    base::FilePath upload_file_path(temp_dir_.GetPath().AppendASCII(
        "test file " + base::NumberToString(n)));
    const std::string upload_file_data = "data";
    EXPECT_TRUE(base::WriteFile(upload_file_path, upload_file_data));
    return upload_file_path;
  }

  FakeDownloadFeedback* feedback(size_t n) const {
    return download_feedback_factory_.feedback(n);
  }

  size_t num_feedbacks() const {
    return download_feedback_factory_.num_feedbacks();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  FakeDownloadFeedbackFactory download_feedback_factory_;
  TestingProfile profile_;
  FakeDownloadProtectionService fake_download_service_;
};

TEST_F(DownloadFeedbackServiceTest, SingleFeedbackComplete) {
  const base::FilePath file_path(CreateTestFile(0));
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";

  download::DownloadItem::AcquireFileCallback download_discarded_callback;

  NiceMock<download::MockDownloadItem> item;
  EXPECT_CALL(item, IsDangerous()).WillRepeatedly(Return(true));
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
  EXPECT_CALL(item, GetReceivedBytes()).WillRepeatedly(Return(1000));
  EXPECT_CALL(item, CopyDownload(_))
      .WillOnce([&download_discarded_callback](
                    download::DownloadItem::AcquireFileCallback arg) {
        download_discarded_callback = std::move(arg);
      });

  DownloadFeedbackService service(&fake_download_service_,
                                  file_task_runner_.get());
  service.BeginFeedbackForDownload(&profile_, &item, ping_request,
                                   ping_response);
  ASSERT_FALSE(download_discarded_callback.is_null());
  EXPECT_EQ(0U, num_feedbacks());

  std::move(download_discarded_callback).Run(file_path);
  ASSERT_EQ(1U, num_feedbacks());
  ASSERT_TRUE(feedback(0));
  EXPECT_TRUE(feedback(0)->start_called());
  EXPECT_EQ(ping_request, feedback(0)->GetPingRequestForTesting());
  EXPECT_EQ(ping_response, feedback(0)->GetPingResponseForTesting());

  feedback(0)->finish_callback().Run();
  EXPECT_FALSE(feedback(0));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(DownloadFeedbackServiceTest, MultiplePendingFeedbackComplete) {
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";
  const size_t kNumDownloads = 3;

  std::array<download::DownloadItem::AcquireFileCallback, kNumDownloads>
      download_discarded_callback;
  std::array<base::FilePath, kNumDownloads> file_path;
  std::array<download::MockDownloadItem, kNumDownloads> item;
  for (size_t i = 0; i < kNumDownloads; ++i) {
    file_path[i] = CreateTestFile(i);
    EXPECT_CALL(item[i], GetDangerType())
        .WillRepeatedly(
            Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
    EXPECT_CALL(item[i], GetReceivedBytes()).WillRepeatedly(Return(1000));
    EXPECT_CALL(item[i], CopyDownload(_))
        .WillOnce([&download_discarded_callback,
                   i](download::DownloadItem::AcquireFileCallback arg) {
          download_discarded_callback[i] = std::move(arg);
        });
  }

  {
    DownloadFeedbackService service(&fake_download_service_,
                                    file_task_runner_.get());
    for (size_t i = 0; i < kNumDownloads; ++i) {
      SCOPED_TRACE(i);
      service.BeginFeedbackForDownload(&profile_, &item[i], ping_request,
                                       ping_response);
      ASSERT_FALSE(download_discarded_callback[i].is_null());
    }
    EXPECT_EQ(0U, num_feedbacks());

    for (size_t i = 0; i < kNumDownloads; ++i) {
      std::move(download_discarded_callback[i]).Run(file_path[i]);
    }

    ASSERT_EQ(3U, num_feedbacks());
    EXPECT_TRUE(feedback(0)->start_called());
    EXPECT_FALSE(feedback(1)->start_called());
    EXPECT_FALSE(feedback(2)->start_called());

    feedback(0)->finish_callback().Run();

    EXPECT_FALSE(feedback(0));
    EXPECT_TRUE(feedback(1)->start_called());
    EXPECT_FALSE(feedback(2)->start_called());

    feedback(1)->finish_callback().Run();

    EXPECT_FALSE(feedback(0));
    EXPECT_FALSE(feedback(1));
    EXPECT_TRUE(feedback(2)->start_called());

    feedback(2)->finish_callback().Run();

    EXPECT_FALSE(feedback(0));
    EXPECT_FALSE(feedback(1));
    EXPECT_FALSE(feedback(2));
  }

  content::RunAllTasksUntilIdle();
  // These files should still exist since the FakeDownloadFeedback does not
  // delete them.
  EXPECT_TRUE(base::PathExists(file_path[0]));
  EXPECT_TRUE(base::PathExists(file_path[1]));
  EXPECT_TRUE(base::PathExists(file_path[2]));
}

// TODO(crbug.com/40749638): Deflake this test.
TEST_F(DownloadFeedbackServiceTest, DISABLED_MultiFeedbackWithIncomplete) {
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";
  const size_t kNumDownloads = 3;

  std::array<download::DownloadItem::AcquireFileCallback, kNumDownloads>
      download_discarded_callback;
  std::array<base::FilePath, kNumDownloads> file_path;
  std::array<download::MockDownloadItem, kNumDownloads> item;
  for (size_t i = 0; i < kNumDownloads; ++i) {
    file_path[i] = CreateTestFile(i);
    EXPECT_CALL(item[i], GetDangerType())
        .WillRepeatedly(
            Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
    EXPECT_CALL(item[i], GetReceivedBytes()).WillRepeatedly(Return(1000));
    EXPECT_CALL(item[i], CopyDownload(_))
        .WillOnce([&download_discarded_callback,
                   i](download::DownloadItem::AcquireFileCallback arg) {
          download_discarded_callback[i] = std::move(arg);
        });
  }

  {
    DownloadFeedbackService service(&fake_download_service_,
                                    file_task_runner_.get());
    for (size_t i = 0; i < kNumDownloads; ++i) {
      SCOPED_TRACE(i);
      service.BeginFeedbackForDownload(&profile_, &item[i], ping_request,
                                       ping_response);
      ASSERT_FALSE(download_discarded_callback[i].is_null());
    }
    EXPECT_EQ(0U, num_feedbacks());

    std::move(download_discarded_callback[0]).Run(file_path[0]);
    ASSERT_EQ(1U, num_feedbacks());
    ASSERT_TRUE(feedback(0));
    EXPECT_TRUE(feedback(0)->start_called());

    std::move(download_discarded_callback[1]).Run(file_path[1]);
    ASSERT_EQ(2U, num_feedbacks());
    ASSERT_TRUE(feedback(1));
    EXPECT_FALSE(feedback(1)->start_called());

    feedback(0)->finish_callback().Run();
    EXPECT_FALSE(feedback(0));
    EXPECT_TRUE(feedback(1)->start_called());
  }

  EXPECT_EQ(2U, num_feedbacks());
  for (size_t i = 0; i < num_feedbacks(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(feedback(i));
  }

  // Running a download acquired callback after the DownloadFeedbackService is
  // destroyed should delete the file.
  std::move(download_discarded_callback[2]).Run(file_path[2]);
  EXPECT_EQ(2U, num_feedbacks());

  // File should still exist since the file deletion task hasn't run yet.
  EXPECT_TRUE(base::PathExists(file_path[2]));

  content::RunAllTasksUntilIdle();
  // File should be deleted since the AcquireFileCallback ran after the service
  // was deleted.
  EXPECT_FALSE(base::PathExists(file_path[2]));

  // These files should still exist since the FakeDownloadFeedback does not
  // delete them.
  EXPECT_TRUE(base::PathExists(file_path[0]));
  EXPECT_TRUE(base::PathExists(file_path[1]));
}

}  // namespace safe_browsing
