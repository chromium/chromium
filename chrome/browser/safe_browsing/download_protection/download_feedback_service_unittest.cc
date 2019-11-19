// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace safe_browsing {

namespace {

class FakeDownloadFeedback : public DownloadFeedback {
 public:
  FakeDownloadFeedback(base::TaskRunner* file_task_runner,
                       const std::string& ping_request,
                       const std::string& ping_response,
                       base::Closure deletion_callback)
      : ping_request_(ping_request),
        ping_response_(ping_response),
        deletion_callback_(deletion_callback),
        start_called_(false) {}

  ~FakeDownloadFeedback() override { deletion_callback_.Run(); }

  void Start(const base::Closure& finish_callback) override {
    start_called_ = true;
    finish_callback_ = finish_callback;
  }

  const std::string& GetPingRequestForTesting() const override {
    return ping_request_;
  }

  const std::string& GetPingResponseForTesting() const override {
    return ping_response_;
  }

  base::Closure finish_callback() const { return finish_callback_; }

  bool start_called() const { return start_called_; }

 private:
  scoped_refptr<base::TaskRunner> file_task_runner_;
  std::string ping_request_;
  std::string ping_response_;

  base::Closure finish_callback_;
  base::Closure deletion_callback_;
  bool start_called_;
};

class FakeDownloadFeedbackFactory : public DownloadFeedbackFactory {
 public:
  ~FakeDownloadFeedbackFactory() override {}

  std::unique_ptr<DownloadFeedback> CreateDownloadFeedback(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner,
      const base::FilePath& file_path,
      const std::string& ping_request,
      const std::string& ping_response) override {
    FakeDownloadFeedback* feedback = new FakeDownloadFeedback(
        file_task_runner, ping_request, ping_response,
        base::Bind(&FakeDownloadFeedbackFactory::DownloadFeedbackSent,
                   base::Unretained(this), feedbacks_.size()));
    feedbacks_.push_back(feedback);
    return base::WrapUnique(feedback);
  }

  void DownloadFeedbackSent(size_t n) { feedbacks_[n] = nullptr; }

  FakeDownloadFeedback* feedback(size_t n) const { return feedbacks_[n]; }

  size_t num_feedbacks() const { return feedbacks_.size(); }

 private:
  std::vector<FakeDownloadFeedback*> feedbacks_;
};

bool WillStorePings(DownloadCheckResult result,
                    bool upload_requested,
                    int64_t size) {
  download::MockDownloadItem item;
  EXPECT_CALL(item, GetReceivedBytes()).WillRepeatedly(Return(size));

  EXPECT_FALSE(DownloadFeedbackService::IsEnabledForDownload(item));
  DownloadFeedbackService::MaybeStorePingsForDownload(result, upload_requested,
                                                      &item, "a", "b");
  return DownloadFeedbackService::IsEnabledForDownload(item);
}

}  // namespace

class DownloadFeedbackServiceTest : public testing::Test {
 public:
  DownloadFeedbackServiceTest()
      : file_task_runner_(base::CreateSequencedTaskRunner(
            {base::ThreadPool(), base::MayBlock(),
             base::TaskPriority::BEST_EFFORT})) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    DownloadFeedback::RegisterFactory(&download_feedback_factory_);
  }

  void TearDown() override { DownloadFeedback::RegisterFactory(nullptr); }

  base::FilePath CreateTestFile(int n) const {
    base::FilePath upload_file_path(temp_dir_.GetPath().AppendASCII(
        "test file " + base::NumberToString(n)));
    const std::string upload_file_data = "data";
    int wrote = base::WriteFile(upload_file_path, upload_file_data.data(),
                                upload_file_data.size());
    EXPECT_EQ(static_cast<int>(upload_file_data.size()), wrote);
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
};

TEST_F(DownloadFeedbackServiceTest, MaybeStorePingsForDownload) {
  const int64_t ok_size = DownloadFeedback::kMaxUploadSize;
  const int64_t bad_size = DownloadFeedback::kMaxUploadSize + 1;

  std::vector<bool> upload_requests = {false, true};
  for (bool upload_requested : upload_requests) {
    // SAFE will never upload
    EXPECT_FALSE(
        WillStorePings(DownloadCheckResult::SAFE, upload_requested, ok_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::WHITELISTED_BY_POLICY,
                                upload_requested, ok_size));
    // Others will upload if requested.
    EXPECT_EQ(upload_requested, WillStorePings(DownloadCheckResult::UNKNOWN,
                                               upload_requested, ok_size));
    EXPECT_EQ(upload_requested, WillStorePings(DownloadCheckResult::DANGEROUS,
                                               upload_requested, ok_size));
    EXPECT_EQ(upload_requested, WillStorePings(DownloadCheckResult::UNCOMMON,
                                               upload_requested, ok_size));
    EXPECT_EQ(upload_requested,
              WillStorePings(DownloadCheckResult::DANGEROUS_HOST,
                             upload_requested, ok_size));
    EXPECT_EQ(upload_requested,
              WillStorePings(DownloadCheckResult::POTENTIALLY_UNWANTED,
                             upload_requested, ok_size));

    // Bad sizes never upload
    EXPECT_FALSE(
        WillStorePings(DownloadCheckResult::SAFE, upload_requested, bad_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::UNKNOWN, upload_requested,
                                bad_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::DANGEROUS,
                                upload_requested, bad_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::UNCOMMON, upload_requested,
                                bad_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::DANGEROUS_HOST,
                                upload_requested, bad_size));
    EXPECT_FALSE(WillStorePings(DownloadCheckResult::POTENTIALLY_UNWANTED,
                                upload_requested, bad_size));
  }
}

TEST_F(DownloadFeedbackServiceTest, SingleFeedbackCompleteAndDiscardDownload) {
  const base::FilePath file_path(CreateTestFile(0));
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";

  download::DownloadItem::AcquireFileCallback download_discarded_callback;

  download::MockDownloadItem item;
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
  EXPECT_CALL(item, GetReceivedBytes()).WillRepeatedly(Return(1000));
  EXPECT_CALL(item,
              StealDangerousDownload(true /*delete_file_after_feedback*/, _))
      .WillOnce(SaveArg<1>(&download_discarded_callback));

  DownloadFeedbackService service(nullptr, file_task_runner_.get());
  service.MaybeStorePingsForDownload(DownloadCheckResult::UNCOMMON,
                                     true /* upload_requested */, &item,
                                     ping_request, ping_response);
  ASSERT_TRUE(DownloadFeedbackService::IsEnabledForDownload(item));
  service.BeginFeedbackForDownload(&item, DownloadCommands::DISCARD);
  ASSERT_FALSE(download_discarded_callback.is_null());
  EXPECT_EQ(0U, num_feedbacks());

  download_discarded_callback.Run(file_path);
  ASSERT_EQ(1U, num_feedbacks());
  ASSERT_TRUE(feedback(0));
  EXPECT_TRUE(feedback(0)->start_called());
  EXPECT_EQ(ping_request, feedback(0)->GetPingRequestForTesting());
  EXPECT_EQ(ping_response, feedback(0)->GetPingResponseForTesting());

  feedback(0)->finish_callback().Run();
  EXPECT_FALSE(feedback(0));

  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(base::PathExists(file_path));
}

TEST_F(DownloadFeedbackServiceTest, SingleFeedbackCompleteAndKeepDownload) {
  const base::FilePath file_path(CreateTestFile(0));
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";

  download::DownloadItem::AcquireFileCallback download_discarded_callback;

  download::MockDownloadItem item;
  EXPECT_CALL(item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
  EXPECT_CALL(item, GetReceivedBytes()).WillRepeatedly(Return(1000));
  EXPECT_CALL(item,
              StealDangerousDownload(false /*delete_file_after_feedback*/, _))
      .WillOnce(SaveArg<1>(&download_discarded_callback));
  EXPECT_CALL(item, ValidateDangerousDownload()).Times(1);
  GURL empty_url;
  EXPECT_CALL(item, GetURL()).WillOnce(ReturnRef(empty_url));

  DownloadFeedbackService service(nullptr, file_task_runner_.get());
  service.MaybeStorePingsForDownload(DownloadCheckResult::UNCOMMON,
                                     true /* upload_requested */, &item,
                                     ping_request, ping_response);
  ASSERT_TRUE(DownloadFeedbackService::IsEnabledForDownload(item));
  service.BeginFeedbackForDownload(&item, DownloadCommands::KEEP);
  ASSERT_FALSE(download_discarded_callback.is_null());
  EXPECT_EQ(0U, num_feedbacks());

  download_discarded_callback.Run(file_path);
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

  download::DownloadItem::AcquireFileCallback
      download_discarded_callback[kNumDownloads];

  base::FilePath file_path[kNumDownloads];
  download::MockDownloadItem item[kNumDownloads];
  for (size_t i = 0; i < kNumDownloads; ++i) {
    file_path[i] = CreateTestFile(i);
    EXPECT_CALL(item[i], GetDangerType())
        .WillRepeatedly(
            Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
    EXPECT_CALL(item[i], GetReceivedBytes()).WillRepeatedly(Return(1000));
    EXPECT_CALL(item[i], StealDangerousDownload(true, _))
        .WillOnce(SaveArg<1>(&download_discarded_callback[i]));
    DownloadFeedbackService::MaybeStorePingsForDownload(
        DownloadCheckResult::UNCOMMON, true /* upload_requested */, &item[i],
        ping_request, ping_response);
    ASSERT_TRUE(DownloadFeedbackService::IsEnabledForDownload(item[i]));
  }

  {
    DownloadFeedbackService service(nullptr, file_task_runner_.get());
    for (size_t i = 0; i < kNumDownloads; ++i) {
      SCOPED_TRACE(i);
      service.BeginFeedbackForDownload(&item[i], DownloadCommands::DISCARD);
      ASSERT_FALSE(download_discarded_callback[i].is_null());
    }
    EXPECT_EQ(0U, num_feedbacks());

    for (size_t i = 0; i < kNumDownloads; ++i) {
      download_discarded_callback[i].Run(file_path[i]);
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

TEST_F(DownloadFeedbackServiceTest, MultiFeedbackWithIncomplete) {
  const std::string ping_request = "ping";
  const std::string ping_response = "resp";
  const size_t kNumDownloads = 3;

  download::DownloadItem::AcquireFileCallback
      download_discarded_callback[kNumDownloads];

  base::FilePath file_path[kNumDownloads];
  download::MockDownloadItem item[kNumDownloads];
  for (size_t i = 0; i < kNumDownloads; ++i) {
    file_path[i] = CreateTestFile(i);
    EXPECT_CALL(item[i], GetDangerType())
        .WillRepeatedly(
            Return(download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT));
    EXPECT_CALL(item[i], GetReceivedBytes()).WillRepeatedly(Return(1000));
    EXPECT_CALL(item[i], StealDangerousDownload(true, _))
        .WillOnce(SaveArg<1>(&download_discarded_callback[i]));
    DownloadFeedbackService::MaybeStorePingsForDownload(
        DownloadCheckResult::UNCOMMON, true /* upload_requested */, &item[i],
        ping_request, ping_response);
    ASSERT_TRUE(DownloadFeedbackService::IsEnabledForDownload(item[i]));
  }

  {
    DownloadFeedbackService service(nullptr, file_task_runner_.get());
    for (size_t i = 0; i < kNumDownloads; ++i) {
      SCOPED_TRACE(i);
      service.BeginFeedbackForDownload(&item[i], DownloadCommands::DISCARD);
      ASSERT_FALSE(download_discarded_callback[i].is_null());
    }
    EXPECT_EQ(0U, num_feedbacks());

    download_discarded_callback[0].Run(file_path[0]);
    ASSERT_EQ(1U, num_feedbacks());
    ASSERT_TRUE(feedback(0));
    EXPECT_TRUE(feedback(0)->start_called());

    download_discarded_callback[1].Run(file_path[1]);
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
  download_discarded_callback[2].Run(file_path[2]);
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
