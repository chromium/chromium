// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage_queue.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArg;

namespace reporting {
namespace {

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent()
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~TestEvent() { EXPECT_TRUE(completed_.IsSignaled()) << "Not responded"; }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    completed_.Wait();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    DCHECK(!completed_.IsSignaled());
    return base::BindOnce(
        [](base::WaitableEvent* completed, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          completed->Signal();
        },
        base::Unretained(&completed_), base::Unretained(&result_));
  }

 private:
  base::WaitableEvent completed_;
  ResType result_;
};

class MockUploadClient : public StorageQueue::UploaderInterface {
 public:
  // Mapping of (generation, seq number) to matching record digest. Whenever a
  // record is uploaded and includes last record digest, this map should have
  // that digest already recorded. Only the first record in a generation is
  // uploaded without last record digest.
  using LastRecordDigestMap = std::map<
      std::pair<uint64_t /*generation */, uint64_t /*sequencing number*/>,
      std::string /*digest*/>;

  explicit MockUploadClient(LastRecordDigestMap* last_record_digest_map)
      : last_record_digest_map_(last_record_digest_map) {}

  void ProcessRecord(StatusOr<EncryptedRecord> encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    if (!encrypted_record.ok()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(encrypted_record.status()));
      return;
    }
    WrappedRecord wrapped_record;
    ASSERT_TRUE(wrapped_record.ParseFromString(
        encrypted_record.ValueOrDie().encrypted_wrapped_record()));
    // Verify generation match.
    const auto& sequencing_information =
        encrypted_record.ValueOrDie().sequencing_information();
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(Status(
              error::DATA_LOSS,
              base::StrCat({"Generation id mismatch, expected=",
                            base::NumberToString(generation_id_.value()),
                            " actual=",
                            base::NumberToString(
                                sequencing_information.generation_id())}))));
      return;
    }
    if (!generation_id_.has_value()) {
      generation_id_ = sequencing_information.generation_id();
    }

    // Verify digest and its match.
    // Last record digest is not verified yet, since duplicate records are
    // accepted in this test.
    {
      std::string serialized_record;
      wrapped_record.record().SerializeToString(&serialized_record);
      const auto record_digest = crypto::SHA256HashString(serialized_record);
      DCHECK_EQ(record_digest.size(), crypto::kSHA256Length);
      if (record_digest != wrapped_record.record_digest()) {
        std::move(processed_cb)
            .Run(UploadRecordFailure(
                Status(error::DATA_LOSS, "Record digest mismatch")));
        return;
      }
      if (wrapped_record.has_last_record_digest()) {
        auto it = last_record_digest_map_->find(
            std::make_pair(sequencing_information.sequencing_id() - 1,
                           sequencing_information.generation_id()));
        if (it == last_record_digest_map_->end() ||
            it->second != wrapped_record.last_record_digest()) {
          std::move(processed_cb)
              .Run(UploadRecordFailure(
                  Status(error::DATA_LOSS, "Last record digest mismatch")));
          return;
        }
      }
      last_record_digest_map_->emplace(
          std::make_pair(sequencing_information.sequencing_id(),
                         sequencing_information.generation_id()),
          record_digest);
    }

    std::move(processed_cb)
        .Run(UploadRecord(sequencing_information.sequencing_id(),
                          wrapped_record.record().data()));
  }

  void Completed(Status status) override { UploadComplete(status); }

  MOCK_METHOD(bool, UploadRecord, (uint64_t, base::StringPiece), (const));
  MOCK_METHOD(bool, UploadRecordFailure, (Status), (const));
  MOCK_METHOD(void, UploadComplete, (Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    explicit SetUp(MockUploadClient* client) : client_(client) {}
    ~SetUp() {
      EXPECT_CALL(*client_, UploadRecordFailure(_))
          .Times(0)
          .InSequence(client_->test_upload_sequence_);
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK())))
          .Times(1)
          .InSequence(client_->test_upload_sequence_);
    }

    SetUp& Required(uint64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*client_,
                  UploadRecord(Eq(sequence_number), StrEq(std::string(value))))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(uint64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*client_,
                  UploadRecord(Eq(sequence_number), StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

   private:
    MockUploadClient* const client_;
  };

 private:
  base::Optional<uint64_t> generation_id_;
  LastRecordDigestMap* const last_record_digest_map_;

  Sequence test_upload_sequence_;
};

class StorageQueueTest : public ::testing::TestWithParam<size_t> {
 protected:
  void SetUp() override { ASSERT_TRUE(location_.CreateUniqueTempDir()); }

  void CreateStorageQueueOrDie(const StorageQueue::Options& options) {
    ASSERT_FALSE(storage_queue_) << "StorageQueue already assigned";
    test_encryption_module_ =
        base::MakeRefCounted<test::TestEncryptionModule>();
    TestEvent<StatusOr<scoped_refptr<StorageQueue>>> e;
    StorageQueue::Create(
        options,
        base::BindRepeating(&StorageQueueTest::BuildMockUploader,
                            base::Unretained(this)),
        test_encryption_module_, e.cb());
    StatusOr<scoped_refptr<StorageQueue>> storage_queue_result = e.result();
    ASSERT_OK(storage_queue_result) << "Failed to create StorageQueue, error="
                                    << storage_queue_result.status();
    storage_queue_ = std::move(storage_queue_result.ValueOrDie());
  }

  StorageQueue::Options BuildStorageQueueOptionsImmediate() const {
    return StorageQueue::Options()
        .set_directory(
            base::FilePath(location_.GetPath()).Append(FILE_PATH_LITERAL("D1")))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"))
        .set_single_file_size(GetParam());
  }

  StorageQueue::Options BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::TimeDelta::FromSeconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  StorageQueue::Options BuildStorageQueueOptionsOnlyManual() const {
    return BuildStorageQueueOptionsPeriodic(base::TimeDelta::Max());
  }

  StatusOr<std::unique_ptr<StorageQueue::UploaderInterface>>
  BuildMockUploader() {
    auto uploader =
        std::make_unique<MockUploadClient>(&last_record_digest_map_);
    set_mock_uploader_expectations_.Call(uploader.get());
    return uploader;
  }

  Status WriteString(base::StringPiece data) {
    EXPECT_TRUE(storage_queue_) << "StorageQueue not created yet";
    TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    storage_queue_->Write(std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(base::StringPiece data) {
    const Status write_result = WriteString(data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(std::uint64_t seq_number) {
    TestEvent<Status> c;
    storage_queue_->Confirm(seq_number, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  base::ScopedTempDir location_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<StorageQueue> storage_queue_;

  // Test-wide global mapping of seq number to record digest.
  // Serves all MockUploadClients created by test fixture.
  MockUploadClient::LastRecordDigestMap last_record_digest_map_;

  ::testing::MockFunction<void(MockUploadClient*)>
      set_mock_uploader_expectations_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

constexpr std::array<const char*, 3> data = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> more_data = {"More1111", "More222",
                                                  "More33"};

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndReopen) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull())).Times(0);
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  storage_queue_.reset();

  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenAndWriteMore) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull())).Times(0);
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  storage_queue_.reset();

  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2]);
      }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  storage_queue_.reset();

  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndFlush) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2]);
      }));

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndFlush) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  storage_queue_.reset();

  CreateStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, ValidateVariousRecordSizes) {
  std::vector<std::string> data;
  for (size_t i = 16; i < 16 + 16; ++i) {
    data.emplace_back(i, 'R');
  }
  CreateStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  for (const auto& record : data) {
    WriteStringOrDie(record);
  }

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([&data](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp client_setup(mock_upload_client);
        for (size_t i = 0; i < data.size(); ++i) {
          client_setup.Required(i, data[i]);
        }
      }));

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2]);
      }));

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*seq_number=*/0);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(1, data[1])
            .Required(2, data[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*seq_number=*/1);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client).Required(2, data[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*seq_number=*/2);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmationsAndReopen) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2]);
      }));

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*seq_number=*/0);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(1, data[1])
            .Required(2, data[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*seq_number=*/1);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client).Required(2, data[2]);
      }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  storage_queue_.reset();
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(0, data[0])
            .Possible(1, data[1])
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*seq_number=*/2);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Possible(1, data[1])
            .Possible(2, data[2]);
      }));
  WriteStringOrDie(data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Possible(2, data[2]);
      }));
  WriteStringOrDie(data[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .Required(1, data[1])
            .Required(2, data[2]);
      }));
  WriteStringOrDie(data[2]);
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the data that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(0, data[0])
            .Possible(1, data[1])
            .Possible(2, data[2]);
      }));
  WriteStringOrDie(data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(0, data[0])
            .Possible(1, data[1])
            .Possible(2, data[2]);
      }));
  WriteStringOrDie(data[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(0, data[0])
            .Possible(1, data[1])
            .Required(2, data[2]);  // Not confirmed - hence |Required|
      }));
  WriteStringOrDie(data[2]);

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(/*seq_number=*/1);

  // Add more data and verify that #2 and new data are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Possible(4, more_data[1])
            .Possible(5, more_data[2]);
      }));
  WriteStringOrDie(more_data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Possible(5, more_data[2]);
      }));
  WriteStringOrDie(more_data[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(2, data[2])
            .Required(3, more_data[0])
            .Required(4, more_data[1])
            .Required(5, more_data[2]);
      }));
  WriteStringOrDie(more_data[2]);
}

TEST_P(StorageQueueTest, WriteEncryptFailure) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  DCHECK(test_encryption_module_);
  EXPECT_CALL(*test_encryption_module_, EncryptRecord(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })));
  const Status result = WriteString("TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

INSTANTIATE_TEST_SUITE_P(VaryingFileSize,
                         StorageQueueTest,
                         testing::Values(128 * 1024LL * 1024LL,
                                         64 /* two data in file */,
                                         32 /* single record in file */));

// TODO(b/157943006): Additional tests:
// 1) Options object with a bad path.
// 2) Have bad prefix files in the directory.
// 3) Attempt to create file with duplicated file extension.
// 4) Other negative tests.

}  // namespace
}  // namespace reporting
