// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace reporting {
namespace {

constexpr char kUploadFileName[] = "/tmp/file";
constexpr char kSessionToken[] = "ABC";
constexpr char kUploadParameters[] = "http://upload";
constexpr char kAccessParameters[] = "http://destination";

class MockFileUploadDelegate : public FileUploadJob::Delegate {
 public:
  // Forwarder to `MockFileUploadDelegate` that allows to repeatedly construct
  // its instances and then forward the mocked calls to the single instance of
  // `MockFileUploadDelegate`.
  class Forwarder : public FileUploadJob::Delegate {
   public:
    explicit Forwarder(MockFileUploadDelegate* actual_mock)
        : actual_mock_(actual_mock) {}

    void DoInitiate(
        std::string_view origin_path,
        std::string_view upload_parameters,
        base::OnceCallback<void(
            StatusOr<std::pair<int64_t /*total*/,
                               std::string /*session_token*/>>)> cb) override {
      actual_mock_->DoInitiate(origin_path, upload_parameters, std::move(cb));
    }

    void DoNextStep(
        int64_t total,
        int64_t uploaded,
        std::string_view session_token,
        ScopedReservation scoped_reservation,
        base::OnceCallback<void(
            StatusOr<std::pair<int64_t /*uploaded*/,
                               std::string /*session_token*/>>)> cb) override {
      actual_mock_->DoNextStep(total, uploaded, session_token,
                               std::move(scoped_reservation), std::move(cb));
    }

    void DoFinalize(
        std::string_view session_token,
        base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
            cb) override {
      actual_mock_->DoFinalize(session_token, std::move(cb));
    }

    void DoDeleteFile(std::string_view origin_path) override {
      actual_mock_->DoDeleteFile(origin_path);
    }

   private:
    raw_ptr<MockFileUploadDelegate> actual_mock_;
  };

  MOCK_METHOD(void,
              DoInitiate,
              (std::string_view origin_path,
               std::string_view upload_parameters,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*total*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(void,
              DoNextStep,
              (int64_t total,
               int64_t uploaded,
               std::string_view session_token,
               ScopedReservation scoped_reservation,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*uploaded*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(
      void,
      DoFinalize,
      (std::string_view session_token,
       base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
           cb),
      (override));

  MOCK_METHOD(void, DoDeleteFile, (std::string_view origin_path), (override));
};

UploadSettings ComposeUploadSettings(int64_t retry_count = 1) {
  UploadSettings settings;
  settings.set_origin_path(kUploadFileName);
  settings.set_retry_count(retry_count);
  settings.set_upload_parameters(kUploadParameters);
  return settings;
}

UploadTracker ComposeUploadTracker(int64_t total, int64_t uploaded) {
  UploadTracker tracker;
  tracker.set_total(total);
  tracker.set_uploaded(uploaded);
  tracker.set_session_token(kSessionToken);
  return tracker;
}

::testing::Matcher<UploadSettings> MatchSettings(int64_t retry_count = 1) {
  return AllOf(
      Property(&UploadSettings::retry_count, Eq(retry_count)),
      Property(&UploadSettings::origin_path, StrEq(kUploadFileName)),
      Property(&UploadSettings::upload_parameters, StrEq(kUploadParameters)));
}

class FileUploadJobTest : public ::testing::Test {
 protected:
  template <typename Method, class... Args>
  void RunAsyncJobAndWait(FileUploadJob& job, Method method, Args&&... args) {
    test::TestCallbackAutoWaiter waiter;
    (job.*method)(std::forward<Args>(args)...,
                  base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                 base::Unretained(&waiter)));
  }

  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
  }

  void TearDown() override {
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  FileUploadJob::Delegate::SmartPtr CreateForwarderDelegate() {
    return FileUploadJob::Delegate::SmartPtr(
        new MockFileUploadDelegate::Forwarder(&mock_delegate_),
        base::OnTaskRunnerDeleter(
            FileUploadJob::Manager::GetInstance()->sequenced_task_runner()));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FileUploadJob::TestEnvironment manager_test_env_;

  StrictMock<MockFileUploadDelegate> mock_delegate_;

  scoped_refptr<ResourceManager> memory_resource_;
};

TEST_F(FileUploadJobTest, SuccessfulRun) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(kAccessParameters);
          }));
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_FALSE(job->tracker().has_status());
  ASSERT_THAT(job->tracker().session_token(), IsEmpty());
  ASSERT_THAT(job->tracker().access_parameters(), StrEq(kAccessParameters));
}

TEST_F(FileUploadJobTest, NoMoreRetries) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/0);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::OUT_OF_RANGE)),
                    Property(&StatusProto::error_message,
                             StrEq("Too many upload attempts"))));
}

TEST_F(FileUploadJobTest, FailToInitiate) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/1);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, FailToInitiateWithMoreRetries) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/2);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings(/*retry_count=*/2));
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, AlreadyInitiated) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/2);
  auto& tracker = *log_upload_event.mutable_upload_tracker();
  tracker.set_session_token(kSessionToken);
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has already been initiated"))));
}

TEST_F(FileUploadJobTest, FailToPerformNextStep) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/2);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings(/*retry_count=*/2));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .WillOnce(Invoke(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          }))
      .WillOnce(Invoke(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 2u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  }
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, FailToPerformNextStepWithMoreRetries) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/2);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings(/*retry_count=*/2));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .WillOnce(Invoke(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          }))
      .WillOnce(Invoke(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 2u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  }
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, FailToFinalize) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, FailToFinalizeWithMoreRetries) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/2);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings(/*retry_count=*/2));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Declined in test")));
          }));
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, IncompleteUpload) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(std::make_pair(uploaded + 100L - 1L,
                                             std::string(session_token)));
          });
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::DATA_LOSS)),
                    Property(&StatusProto::error_message,
                             StrEq("Upload incomplete 297 out of 300"))));
}

TEST_F(FileUploadJobTest, ExcessiveUpload) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());

  EXPECT_CALL(mock_delegate_, DoNextStep(300L, _, StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 500L, std::string(session_token)));
          });
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  ASSERT_FALSE(job->tracker().has_status());
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::OUT_OF_RANGE)),
                    Property(&StatusProto::error_message,
                             StrEq("Uploaded 500 out of range"))));
}

TEST_F(FileUploadJobTest, BackingUpload) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings(), MatchSettings());

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          })
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                std::make_pair(uploaded - 1L, std::string(session_token)));
          });
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  ASSERT_FALSE(job->tracker().has_status());
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::DATA_LOSS)),
                    Property(&StatusProto::error_message,
                             StrEq("Job has backtracked from 100 to 99"))));
}

TEST_F(FileUploadJobTest, SuccessfulResumption) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  *log_upload_event.mutable_upload_tracker() = ComposeUploadTracker(300L, 100L);
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(2)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  for (size_t i = 0u; i < 2u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(kAccessParameters);
          }));
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_FALSE(job->tracker().has_status());
  ASSERT_THAT(job->tracker().session_token(), IsEmpty());
  ASSERT_THAT(job->tracker().access_parameters(), StrEq(kAccessParameters));
}

TEST_F(FileUploadJobTest, FailToResumeStep) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  *log_upload_event.mutable_upload_tracker() = ComposeUploadTracker(300L, 100L);
  // Corrupt tracker - lose token!
  log_upload_event.mutable_upload_tracker()->clear_session_token();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  ScopedReservation scoped_reservation(0uL, memory_resource_);
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep, scoped_reservation);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has not been initiated yet"))));
}

TEST_F(FileUploadJobTest, FailToResumeFinalize) {
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
  *log_upload_event.mutable_upload_tracker() = ComposeUploadTracker(300L, 300L);
  // Corrupt tracker - lose token!
  log_upload_event.mutable_upload_tracker()->clear_session_token();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  auto job = std::make_unique<FileUploadJob>(log_upload_event.upload_settings(),
                                             log_upload_event.upload_tracker(),
                                             CreateForwarderDelegate());
  job->SetEventHelperForTest(std::make_unique<FileUploadJob::EventHelper>(
      job->GetWeakPtr(), Priority::IMMEDIATE, std::move(record_copy),
      std::move(log_upload_event)));
  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
      .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has not been initiated yet"))));
}

TEST_F(FileUploadJobTest, AttemptToInitiateMultipleJobs) {
  // Imitate multiple copies of the same log_upload_event initiating jobs.
  // Only one is expected to succeed (call `DoInitiate`), others just pass.

  // Collect weak pointers to track jobs life.
  std::vector<base::WeakPtr<FileUploadJob>> jobs_weak_ptrs;

  static constexpr size_t kJobsCount = 16u;
  std::atomic<size_t> failures = 0;
  {
    test::TestCallbackAutoWaiter waiter;
    waiter.Attach(kJobsCount - 1);

    // Initiation will happen only once!
    EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
        .WillOnce(Invoke(
            [](std::string_view origin_path, std::string_view upload_parameters,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*total*/,
                                      std::string /*session_token*/>>)> cb) {
              std::move(cb).Run(std::make_pair(300L, kSessionToken));
            }));
    EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

    // Attempt to add and initiate jobs multiple times.
    for (size_t i = 0u; i < kJobsCount; ++i) {
      ::ash::reporting::LogUploadEvent log_upload_event;
      *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
      log_upload_event.mutable_upload_tracker();
      Record record_copy;
      ASSERT_TRUE(
          log_upload_event.SerializeToString(record_copy.mutable_data()));
      record_copy.set_destination(Destination::LOG_UPLOAD);
      base::ScopedClosureRunner done(base::BindOnce(
          &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
      FileUploadJob::Manager::GetInstance()->Register(
          Priority::IMMEDIATE, std::move(record_copy),
          /*log_upload_event=*/log_upload_event, CreateForwarderDelegate(),
          base::BindOnce(
              [](base::ScopedClosureRunner done,
                 std::vector<base::WeakPtr<FileUploadJob>>* jobs_weak_ptrs,
                 std::atomic<size_t>* failures,
                 StatusOr<FileUploadJob*> job_or_error) {
                if (!job_or_error.has_value()) {
                  EXPECT_THAT(job_or_error.error().error_code(),
                              Eq(error::ALREADY_EXISTS));
                  EXPECT_THAT(job_or_error.error().error_message(),
                              StrEq("Duplicate event"));
                  ++(*failures);
                  return;
                }
                auto* const job = job_or_error.value();
                jobs_weak_ptrs->push_back(job->GetWeakPtr());
                job->Initiate(done.Release());
              },
              std::move(done), base::Unretained(&jobs_weak_ptrs),
              base::Unretained(&failures)));
    }
  }
  EXPECT_THAT(failures.load(), Eq(kJobsCount - 1));
  // Wait for less than job life time.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Check that all weak pointers are still valid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_TRUE(job_weak_ptr.MaybeValid());
  }
  // Wait for the jobs to be unregistered and erased.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Now all weak pointers are invalid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_FALSE(job_weak_ptr.MaybeValid());
  }
}

TEST_F(FileUploadJobTest, AttemptToNextStepMultipleJobs) {
  // Imitate multiple copies of the same log_upload_event initiating jobs.
  // Only one is expected to succeed (call `DoInitiate`), others just pass.

  // Collect weak pointers to track jobs life.
  std::vector<base::WeakPtr<FileUploadJob>> jobs_weak_ptrs;

  static constexpr size_t kJobsCount = 16u;
  std::atomic<size_t> failures = 0;
  {
    test::TestCallbackAutoWaiter waiter;
    waiter.Attach(kJobsCount - 1);

    // Next step will happen at least once and no more than 3 times!
    // After the first Job is registered, every time we attempt to register
    // a new one, we actually get the same Job.
    // In production code we would probably also compare `uploaded` to the one
    // specified in the log_upload_event, and only proceed if they match, but in
    // the test we can do differently.
    EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
        .Times(Between(1, 3))
        .WillRepeatedly(
            [](int64_t total, int64_t uploaded, std::string_view session_token,
               ScopedReservation scoped_reservation,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*uploaded*/,
                                      std::string /*session_token*/>>)> cb) {
              EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
              std::move(cb).Run(
                  std::make_pair(uploaded + 100L, std::string(session_token)));
            });
    EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

    // Attempt to add and step jobs multiple times.
    for (size_t i = 0u; i < kJobsCount; ++i) {
      ::ash::reporting::LogUploadEvent log_upload_event;
      *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
      *log_upload_event.mutable_upload_tracker() =
          ComposeUploadTracker(300L, 0L);
      Record record_copy;
      ASSERT_TRUE(
          log_upload_event.SerializeToString(record_copy.mutable_data()));
      record_copy.set_destination(Destination::LOG_UPLOAD);
      base::ScopedClosureRunner done(base::BindOnce(
          &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
      FileUploadJob::Manager::GetInstance()->Register(
          Priority::IMMEDIATE, std::move(record_copy),
          /*log_upload_event=*/log_upload_event, CreateForwarderDelegate(),
          base::BindOnce(
              [](base::ScopedClosureRunner done,
                 std::vector<base::WeakPtr<FileUploadJob>>* jobs_weak_ptrs,
                 scoped_refptr<ResourceManager> memory_resource,
                 std::atomic<size_t>* failures,
                 StatusOr<FileUploadJob*> job_or_error) {
                if (!job_or_error.has_value()) {
                  EXPECT_THAT(job_or_error.error().error_code(),
                              Eq(error::ALREADY_EXISTS));
                  EXPECT_THAT(job_or_error.error().error_message(),
                              StrEq("Duplicate event"));
                  ++(*failures);
                  return;
                }
                EXPECT_TRUE(job_or_error.has_value()) << job_or_error.error();
                auto* const job = job_or_error.value();
                jobs_weak_ptrs->push_back(job->GetWeakPtr());
                ScopedReservation scoped_reservation(0uL, memory_resource);
                job->NextStep(scoped_reservation, done.Release());
              },
              std::move(done), base::Unretained(&jobs_weak_ptrs),
              memory_resource_, base::Unretained(&failures)));
    }
  }
  EXPECT_THAT(failures.load(), Eq(kJobsCount - 1));
  // Wait for less than job life time.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Check that all weak pointers are still valid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_TRUE(job_weak_ptr.MaybeValid());
  }
  // Wait for the jobs to be unregistered and erased.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Now all weak pointers are invalid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_FALSE(job_weak_ptr.MaybeValid());
  }
}

TEST_F(FileUploadJobTest, AttemptToFinalizeMultipleJobs) {
  // Imitate multiple copies of the same log_upload_event initiating jobs.
  // Only one is expected to succeed (call `DoInitiate`), others just pass.

  // Collect weak pointers to track jobs life.
  std::vector<base::WeakPtr<FileUploadJob>> jobs_weak_ptrs;

  static constexpr size_t kJobsCount = 16u;
  std::atomic<size_t> failures = 0;
  {
    test::TestCallbackAutoWaiter waiter;
    waiter.Attach(kJobsCount - 1);

    // Finalize will happen only once!
    EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
        .WillOnce(
            Invoke([](std::string_view session_token,
                      base::OnceCallback<void(
                          StatusOr<std::string /*access_parameters*/>)> cb) {
              std::move(cb).Run(kAccessParameters);
            }));

    // File will be deleted only once too!
    waiter.Attach(1);
    EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
        .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));

    // Attempt to add and finalize jobs multiple times.
    for (size_t i = 0u; i < kJobsCount; ++i) {
      ::ash::reporting::LogUploadEvent log_upload_event;
      *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
      *log_upload_event.mutable_upload_tracker() =
          ComposeUploadTracker(300L, 300L);
      Record record_copy;
      ASSERT_TRUE(
          log_upload_event.SerializeToString(record_copy.mutable_data()));
      record_copy.set_destination(Destination::LOG_UPLOAD);
      base::ScopedClosureRunner done(base::BindOnce(
          &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
      FileUploadJob::Manager::GetInstance()->Register(
          Priority::IMMEDIATE, std::move(record_copy),
          /*log_upload_event=*/log_upload_event, CreateForwarderDelegate(),
          base::BindOnce(
              [](base::ScopedClosureRunner done,
                 std::vector<base::WeakPtr<FileUploadJob>>* jobs_weak_ptrs,
                 std::atomic<size_t>* failures,
                 StatusOr<FileUploadJob*> job_or_error) {
                if (!job_or_error.has_value()) {
                  EXPECT_THAT(job_or_error.error().error_code(),
                              Eq(error::ALREADY_EXISTS));
                  EXPECT_THAT(job_or_error.error().error_message(),
                              StrEq("Duplicate event"));
                  ++(*failures);
                  return;
                }
                EXPECT_TRUE(job_or_error.has_value()) << job_or_error.error();
                auto* const job = job_or_error.value();
                jobs_weak_ptrs->push_back(job->GetWeakPtr());
                job->Finalize(done.Release());
              },
              std::move(done), base::Unretained(&jobs_weak_ptrs),
              base::Unretained(&failures)));
    }
  }
  EXPECT_THAT(failures.load(), Eq(kJobsCount - 1));
  // Wait for less than job life time.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Check that all weak pointers are still valid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_TRUE(job_weak_ptr.MaybeValid());
  }
  // Wait for the jobs to be unregistered and erased.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Now all weak pointers are invalid.
  for (auto& job_weak_ptr : jobs_weak_ptrs) {
    EXPECT_FALSE(job_weak_ptr.MaybeValid());
  }
}

TEST_F(FileUploadJobTest, MultipleStagesJob) {
  // Run single job through multiple stages with 1/2 life time delay,
  // to make sure the timer is extended every time.

  // Collect weak pointer to track job's life.
  base::WeakPtr<FileUploadJob> job_weak_ptr;

  {
    test::TestCallbackAutoWaiter waiter;

    EXPECT_CALL(mock_delegate_, DoInitiate(Not(IsEmpty()), Not(IsEmpty()), _))
        .WillOnce(Invoke(
            [](std::string_view origin_path, std::string_view upload_parameters,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*total*/,
                                      std::string /*session_token*/>>)> cb) {
              std::move(cb).Run(std::make_pair(300L, kSessionToken));
            }));

    // Attempt to add and initiate job.
    ::ash::reporting::LogUploadEvent log_upload_event;
    *log_upload_event.mutable_upload_settings() = ComposeUploadSettings();
    log_upload_event.mutable_upload_tracker();
    Record record_copy;
    ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
    record_copy.set_destination(Destination::LOG_UPLOAD);
    base::ScopedClosureRunner done(base::BindOnce(
        &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
    FileUploadJob::Manager::GetInstance()->Register(
        Priority::IMMEDIATE, std::move(record_copy),
        /*log_upload_event=*/log_upload_event, CreateForwarderDelegate(),
        base::BindOnce(
            [](base::ScopedClosureRunner done,
               base::WeakPtr<FileUploadJob>* job_weak_ptr,
               StatusOr<FileUploadJob*> job_or_error) {
              EXPECT_TRUE(job_or_error.has_value()) << job_or_error.error();
              auto* const job = job_or_error.value();
              *job_weak_ptr = job->GetWeakPtr();
              job->Initiate(done.Release());
            },
            std::move(done), base::Unretained(&job_weak_ptr)));
  }

  // Make 3 steps.
  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, StrEq(kSessionToken), _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            EXPECT_THAT(uploaded, AllOf(Ge(0L), Lt(total)));
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  for (size_t i = 0; i < 3; ++i) {
    // Wait for less than job life time.
    task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
    // Check that weak pointer is still valid.
    EXPECT_TRUE(job_weak_ptr.MaybeValid());

    test::TestCallbackAutoWaiter waiter;
    FileUploadJob::Manager::GetInstance()->sequenced_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileUploadJob::NextStep, job_weak_ptr,
                       ScopedReservation(0uL, memory_resource_),
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter))));
  }

  // Wait for less than job life time.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Check that weak pointer is still valid.
  EXPECT_TRUE(job_weak_ptr.MaybeValid());

  // Finalize job.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
        .WillOnce(
            Invoke([](std::string_view session_token,
                      base::OnceCallback<void(
                          StatusOr<std::string /*access_parameters*/>)> cb) {
              std::move(cb).Run(kAccessParameters);
            }));
    waiter.Attach(1);  // File deletion.
    EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName)))
        .WillOnce(Invoke(&waiter, &test::TestCallbackAutoWaiter::Signal));
    FileUploadJob::Manager::GetInstance()->sequenced_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileUploadJob::Finalize, job_weak_ptr,
                       base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                      base::Unretained(&waiter))));
  }

  // Wait for less than job life time.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Check that weak pointer is still valid.
  EXPECT_TRUE(job_weak_ptr.MaybeValid());
  // Wait for the job to be unregistered and erased.
  task_environment_.FastForwardBy(FileUploadJob::Manager::kLifeTime / 2);
  // Now weak pointer is invalid.
  EXPECT_FALSE(job_weak_ptr.MaybeValid());
}

TEST_F(FileUploadJobTest, FailureRegisteringJobWithNoRetries) {
  test::TestCallbackAutoWaiter waiter;

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

  // Attempt to add job.
  ::ash::reporting::LogUploadEvent log_upload_event;
  *log_upload_event.mutable_upload_settings() =
      ComposeUploadSettings(/*retry_count=*/0);
  log_upload_event.mutable_upload_tracker();
  Record record_copy;
  ASSERT_TRUE(log_upload_event.SerializeToString(record_copy.mutable_data()));
  record_copy.set_destination(Destination::LOG_UPLOAD);
  base::ScopedClosureRunner done(base::BindOnce(
      &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
  FileUploadJob::Manager::GetInstance()->Register(
      Priority::IMMEDIATE, std::move(record_copy),
      /*log_upload_event=*/log_upload_event, CreateForwarderDelegate(),
      base::BindOnce(
          [](base::ScopedClosureRunner done,
             StatusOr<FileUploadJob*> job_or_error) {
            EXPECT_THAT(
                job_or_error.error(),
                AllOf(Property(&Status::code, Eq(error::INVALID_ARGUMENT)),
                      Property(&Status::error_message,
                               StrEq("Too many upload attempts"))));
          },
          std::move(done)));
}
}  // namespace
}  // namespace reporting
