// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"

#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;

namespace reporting {
namespace {

class MockFileUploadJobDelegate : public FileUploadJob::Delegate {
 public:
  MockFileUploadJobDelegate() = default;

  MOCK_METHOD(Status,
              DoInitiate,
              (base::StringPiece origin_path,        // IN
               base::StringPiece upload_parameters,  // IN
               int64_t* total,                       // OUT
               std::string* session_token            // OUT
               ),
              (override));

  MOCK_METHOD(Status,
              DoNextStep,
              (int64_t total,              // IN
               int64_t* uploaded,          // INOUT
               std::string* session_token  // INOUT
               ),
              (override));

  MOCK_METHOD(Status,
              DoFinalize,
              (base::StringPiece session_token,  // IN
               std::string* access_parameters    // OUT
               ),
              (override));
};

class FileUploadJobTest : public ::testing::Test {
 protected:
  template <typename Method>
  void RunAsyncJobAndWait(FileUploadJob& job, Method method) {
    test::TestCallbackAutoWaiter waiter;
    (job.*method)(base::BindOnce(&test::TestCallbackAutoWaiter::Signal,
                                 base::Unretained(&waiter)));
  }

  base::test::TaskEnvironment task_environment_;

  MockFileUploadJobDelegate mock_delegate_;
};

TEST_F(FileUploadJobTest, SuccessfulRun) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 100L;
            return Status::StatusOK();
          });
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(_, _))
      .WillOnce(Invoke(
          [](base::StringPiece session_token, std::string* access_parameters) {
            EXPECT_THAT(session_token, StrEq("ABC"));
            *access_parameters = "http://destination";
            return Status::StatusOK();
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_FALSE(job->tracker().has_status());
  ASSERT_THAT(job->tracker().session_token(), IsEmpty());
  ASSERT_THAT(job->tracker().access_parameters(), StrEq("http://destination"));
}

TEST_F(FileUploadJobTest, NoMoreRetries) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(0);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _)).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::OUT_OF_RANGE)),
                    Property(&StatusProto::error_message,
                             StrEq("Too many upload attempts"))));
}

TEST_F(FileUploadJobTest, FailToInitiate) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Return(Status(error::CANCELLED, "Declined in test")));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, AlreadyInitiated) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  tracker.set_session_token("ABC");
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _)).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has already been initiated"))));
}

TEST_F(FileUploadJobTest, FailToPerformNextStep) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .WillOnce(Invoke(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 100L;
            return Status::StatusOK();
          }))
      .WillOnce(Return(Status(error::CANCELLED, "Declined in test")));
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  }
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, FailToFinalize) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .Times(3)
      .WillRepeatedly(Invoke(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 100L;
            return Status::StatusOK();
          }));
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(_, _))
      .WillOnce(Return(Status(error::CANCELLED, "Declined in test")));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::CANCELLED)),
            Property(&StatusProto::error_message, StrEq("Declined in test"))));
}

TEST_F(FileUploadJobTest, IncompleteUpload) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .Times(3)
      .WillRepeatedly(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 99L;
            return Status::StatusOK();
          });
  for (size_t i = 0u; i < 3u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(_, _)).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::DATA_LOSS)),
                    Property(&StatusProto::error_message,
                             StrEq("Upload incomplete 297 out of 300"))));
}

TEST_F(FileUploadJobTest, ExcessiveUpload) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .WillOnce(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 500L;
            return Status::StatusOK();
          });
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  ASSERT_FALSE(job->tracker().has_status());
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::OUT_OF_RANGE)),
                    Property(&StatusProto::error_message,
                             StrEq("Uploaded 500 out of range"))));
}

TEST_F(FileUploadJobTest, BackingUpload) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _))
      .WillOnce(Invoke([](base::StringPiece origin_path,
                          base::StringPiece upload_parameters, int64_t* total,
                          std::string* session_token) {
        EXPECT_THAT(*session_token, IsEmpty());
        *total = 300L;
        *session_token = "ABC";
        return Status::StatusOK();
      }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Initiate);
  ASSERT_FALSE(job->tracker().has_status());
  EXPECT_THAT(job->settings().retry_count(), Eq(0));

  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .WillOnce(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 100L;
            return Status::StatusOK();
          })
      .WillOnce(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded -= 1L;
            return Status::StatusOK();
          });
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  ASSERT_FALSE(job->tracker().has_status());
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(job->tracker().status(),
              AllOf(Property(&StatusProto::code, Eq(error::DATA_LOSS)),
                    Property(&StatusProto::error_message,
                             StrEq("Job has backtracked from 100 to 99"))));
}

TEST_F(FileUploadJobTest, SuccessfulResumption) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  tracker.set_total(300L);
  tracker.set_uploaded(100L);
  tracker.set_session_token("ABC");
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);
  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(
          [](int64_t total, int64_t* uploaded, std::string* session_token) {
            EXPECT_THAT(*session_token, StrEq("ABC"));
            *uploaded += 100L;
            return Status::StatusOK();
          }));
  for (size_t i = 0u; i < 2u; ++i) {
    RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
    ASSERT_FALSE(job->tracker().has_status());
  }

  EXPECT_CALL(mock_delegate_, DoFinalize(_, _))
      .WillOnce(Invoke(
          [](base::StringPiece session_token, std::string* access_parameters) {
            EXPECT_THAT(session_token, StrEq("ABC"));
            *access_parameters = "http://destination";
            return Status::StatusOK();
          }));
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_FALSE(job->tracker().has_status());
  ASSERT_THAT(job->tracker().session_token(), IsEmpty());
  ASSERT_THAT(job->tracker().access_parameters(), StrEq("http://destination"));
}

TEST_F(FileUploadJobTest, FailToResumeStep) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  tracker.set_total(300L);
  tracker.set_uploaded(100L);
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);

  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _)).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize(_, _)).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::NextStep);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has not been initiated yet"))));
}

TEST_F(FileUploadJobTest, FailToResumeFinalize) {
  UploadSettings init_settings;
  init_settings.set_origin_path("/tmp/file");
  init_settings.set_retry_count(1);
  init_settings.set_upload_parameters("http://upload");
  UploadTracker tracker;
  tracker.set_total(300L);
  tracker.set_uploaded(300L);
  auto job =
      std::make_unique<FileUploadJob>(init_settings, tracker, &mock_delegate_);

  EXPECT_CALL(mock_delegate_, DoInitiate(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep(_, _, _)).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize(_, _)).Times(0);
  RunAsyncJobAndWait(*job, &FileUploadJob::Finalize);
  ASSERT_TRUE(job->tracker().has_status());
  EXPECT_THAT(
      job->tracker().status(),
      AllOf(Property(&StatusProto::code, Eq(error::FAILED_PRECONDITION)),
            Property(&StatusProto::error_message,
                     StrEq("Job has not been initiated yet"))));
}
}  // namespace
}  // namespace reporting
