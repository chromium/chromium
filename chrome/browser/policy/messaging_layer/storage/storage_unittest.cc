// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage.h"

#include <cstdint>
#include <tuple>
#include <utility>

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
#include "components/policy/proto/record_constants.pb.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Property;
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

class MockUploadClient : public Storage::UploaderInterface {
 public:
  // Mapping of (generation, seq number) to matching record digest. Whenever a
  // record is uploaded and includes last record digest, this map should have
  // that digest already recorded. Only the first record in a generation is
  // uploaded without last record digest.
  using LastRecordDigestMap =
      std::map<std::tuple<Priority,
                          uint64_t /*generation */,
                          uint64_t /*sequencing number*/>,
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
            std::make_tuple(sequencing_information.priority(),
                            sequencing_information.sequencing_id() - 1,
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
          std::make_tuple(sequencing_information.priority(),
                          sequencing_information.sequencing_id(),
                          sequencing_information.generation_id()),
          record_digest);
    }

    std::move(processed_cb)
        .Run(UploadRecord(sequencing_information.priority(),
                          sequencing_information.sequencing_id(),
                          wrapped_record.record().data()));
  }

  void Completed(Status status) override { UploadComplete(status); }

  MOCK_METHOD(bool,
              UploadRecord,
              (Priority, uint64_t, base::StringPiece),
              (const));
  MOCK_METHOD(bool, UploadRecordFailure, (Status), (const));
  MOCK_METHOD(void, UploadComplete, (Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    SetUp(Priority priority, MockUploadClient* client)
        : priority_(priority), client_(client) {}

    ~SetUp() {
      EXPECT_CALL(*client_, UploadRecordFailure(_))
          .Times(0)
          .InSequence(client_->test_upload_sequence_);
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK())))
          .Times(1)
          .InSequence(client_->test_upload_sequence_);
    }

    SetUp& Required(uint64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*client_, UploadRecord(Eq(priority_), Eq(sequence_number),
                                         StrEq(std::string(value))))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(uint64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*client_, UploadRecord(Eq(priority_), Eq(sequence_number),
                                         StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

   private:
    Priority priority_;
    MockUploadClient* const client_;
  };

  // Helper class for setting up mock client expectations on empty queue.
  class SetEmpty {
   public:
    SetEmpty(Priority priority, MockUploadClient* client)
        : priority_(priority), client_(client) {}

    ~SetEmpty() {
      EXPECT_CALL(*client_, UploadRecord(Eq(priority_), _, _)).Times(0);
      EXPECT_CALL(*client_, UploadRecordFailure(_)).Times(0);
      EXPECT_CALL(*client_, UploadComplete(Property(&Status::error_code,
                                                    Eq(error::OUT_OF_RANGE))))
          .Times(1);
    }

   private:
    Priority priority_;
    MockUploadClient* const client_;
  };

 private:
  base::Optional<uint64_t> generation_id_;
  LastRecordDigestMap* const last_record_digest_map_;

  Sequence test_upload_sequence_;
};

class StorageTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(location_.CreateUniqueTempDir()); }

  void CreateStorageTestOrDie(const Storage::Options& options) {
    ASSERT_FALSE(storage_) << "StorageTest already assigned";
    test_encryption_module_ =
        base::MakeRefCounted<test::TestEncryptionModule>();
    TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    Storage::Create(options,
                    base::BindRepeating(&StorageTest::BuildMockUploader,
                                        base::Unretained(this)),
                    test_encryption_module_, e.cb());
    StatusOr<scoped_refptr<Storage>> storage_result = e.result();
    ASSERT_OK(storage_result)
        << "Failed to create StorageTest, error=" << storage_result.status();
    storage_ = std::move(storage_result.ValueOrDie());
  }

  Storage::Options BuildStorageOptions() const {
    return Storage::Options().set_directory(
        base::FilePath(location_.GetPath()));
  }

  StatusOr<std::unique_ptr<Storage::UploaderInterface>> BuildMockUploader(
      Priority priority) {
    auto uploader =
        std::make_unique<MockUploadClient>(&last_record_digest_map_);
    set_mock_uploader_expectations_.Call(priority, uploader.get());
    return uploader;
  }

  Status WriteString(Priority priority, base::StringPiece data) {
    EXPECT_TRUE(storage_) << "Storage not created yet";
    TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    storage_->Write(priority, std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(Priority priority, base::StringPiece data) {
    const Status write_result = WriteString(priority, data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(Priority priority, std::uint64_t seq_number) {
    TestEvent<Status> c;
    storage_->Confirm(priority, seq_number, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  base::ScopedTempDir location_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<Storage> storage_;

  // Test-wide global mapping of seq number to record digest.
  // Serves all MockUploadClients created by test fixture.
  MockUploadClient::LastRecordDigestMap last_record_digest_map_;

  ::testing::MockFunction<void(Priority, MockUploadClient*)>
      set_mock_uploader_expectations_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

constexpr std::array<const char*, 3> data = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> more_data = {"More1111", "More222",
                                                  "More33"};

TEST_F(StorageTest, WriteIntoNewStorageAndReopen) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, NotNull())).Times(0);
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, data[0]);
  WriteStringOrDie(FAST_BATCH, data[1]);
  WriteStringOrDie(FAST_BATCH, data[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
}

TEST_F(StorageTest, WriteIntoNewStorageReopenAndWriteMore) {
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, NotNull())).Times(0);
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, data[0]);
  WriteStringOrDie(FAST_BATCH, data[1]);
  WriteStringOrDie(FAST_BATCH, data[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, more_data[0]);
  WriteStringOrDie(FAST_BATCH, more_data[1]);
  WriteStringOrDie(FAST_BATCH, more_data[2]);
}

TEST_F(StorageTest, WriteIntoNewStorageAndUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, data[0]);
  WriteStringOrDie(FAST_BATCH, data[1]);
  WriteStringOrDie(FAST_BATCH, data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Required(2, data[2]);
          }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(StorageTest, WriteIntoNewStorageReopenWriteMoreAndUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, data[0]);
  WriteStringOrDie(FAST_BATCH, data[1]);
  WriteStringOrDie(FAST_BATCH, data[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(FAST_BATCH, more_data[0]);
  WriteStringOrDie(FAST_BATCH, more_data[1]);
  WriteStringOrDie(FAST_BATCH, more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
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

TEST_F(StorageTest, WriteIntoNewStorageAndFlush) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, data[0]);
  WriteStringOrDie(MANUAL_BATCH, data[1]);
  WriteStringOrDie(MANUAL_BATCH, data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(MANUAL_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Required(2, data[2]);
          }));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_F(StorageTest, WriteIntoNewStorageReopenWriteMoreAndFlush) {
  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, data[0]);
  WriteStringOrDie(MANUAL_BATCH, data[1]);
  WriteStringOrDie(MANUAL_BATCH, data[2]);

  storage_.reset();

  CreateStorageTestOrDie(BuildStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, more_data[0]);
  WriteStringOrDie(MANUAL_BATCH, more_data[1]);
  WriteStringOrDie(MANUAL_BATCH, more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(MANUAL_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_F(StorageTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateStorageTestOrDie(BuildStorageOptions());

  WriteStringOrDie(FAST_BATCH, data[0]);
  WriteStringOrDie(FAST_BATCH, data[1]);
  WriteStringOrDie(FAST_BATCH, data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Required(2, data[2]);
          }));

  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #0 and forward time again, removing data #0
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/0);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(1, data[1])
                .Required(2, data[2]);
          }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #1 and forward time again, removing data #1
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/1);
  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(2, data[2]);
          }));
  // Forward time to trigger upload
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Add more records and verify that #2 and new records are returned.
  WriteStringOrDie(FAST_BATCH, more_data[0]);
  WriteStringOrDie(FAST_BATCH, more_data[1]);
  WriteStringOrDie(FAST_BATCH, more_data[2]);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing data #2
  ConfirmOrDie(FAST_BATCH, /*seq_number=*/2);

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_F(StorageTest, WriteAndRepeatedlyImmediateUpload) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Possible(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   data[0]);  // Immediately uploads and verifies.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   data[1]);  // Immediately uploads and verifies.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, data[0])
                .Required(1, data[1])
                .Required(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE,
                   data[2]);  // Immediately uploads and verifies.
}

TEST_F(StorageTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(0, data[0])
                .Possible(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(0, data[0])
                .Possible(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(0, data[0])
                .Possible(1, data[1])
                .Required(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[2]);

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(IMMEDIATE, /*seq_number=*/1);

  // Add more records and verify that #2 and new records are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Possible(4, more_data[1])
                .Possible(5, more_data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Possible(5, more_data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_data[1]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, more_data[2]);
}

TEST_F(StorageTest, WriteAndRepeatedlyUploadMultipleQueues) {
  CreateStorageTestOrDie(BuildStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(0, data[0])
                .Possible(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[0]);
  WriteStringOrDie(SLOW_BATCH, more_data[0]);
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(0, data[0])
                .Possible(1, data[1])
                .Possible(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[1]);
  WriteStringOrDie(SLOW_BATCH, more_data[1]);

  // Set uploader expectations for SLOW_BATCH.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillRepeatedly(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetEmpty(priority, mock_upload_client);
          }));
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(SLOW_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Required(0, more_data[0])
                .Required(1, more_data[1]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));

  // Confirm #0 SLOW_BATCH, removing data #0
  ConfirmOrDie(SLOW_BATCH, /*seq_number=*/0);

  // Confirm #1 IMMEDIATE, removing data #0 and #1
  ConfirmOrDie(IMMEDIATE, /*seq_number=*/1);

  // Add more data
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(IMMEDIATE), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client)
                .Possible(1, data[1])
                .Required(2, data[2]);
          }));
  WriteStringOrDie(IMMEDIATE, data[2]);
  WriteStringOrDie(SLOW_BATCH, more_data[2]);

  // Set uploader expectations for SLOW_BATCH.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(FAST_BATCH), NotNull()))
      .WillRepeatedly(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetEmpty(priority, mock_upload_client);
          }));
  EXPECT_CALL(set_mock_uploader_expectations_, Call(Eq(SLOW_BATCH), NotNull()))
      .WillOnce(
          Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(SLOW_BATCH, mock_upload_client)
                .Required(1, more_data[1])
                .Required(2, more_data[2]);
          }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
}

TEST_F(StorageTest, WriteEncryptFailure) {
  CreateStorageTestOrDie(BuildStorageOptions());
  DCHECK(test_encryption_module_);
  EXPECT_CALL(*test_encryption_module_, EncryptRecord(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })));
  const Status result = WriteString(FAST_BATCH, "TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

}  // namespace
}  // namespace reporting
