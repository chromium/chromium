// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/storage/storage_queue.h"

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/encryption/test_encryption_module.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_configuration.h"
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

// Metadata file name prefix.
const base::FilePath::CharType METADATA_NAME[] = FILE_PATH_LITERAL("META");

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//       base::OnceCallback<void(ResType* res)> type which also may perform some
//       other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//       collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() { EXPECT_FALSE(run_loop_->running()) << "Not responded"; }
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

class MockUploadClient : public StorageQueue::UploaderInterface {
 public:
  // Mapping of <generation id, sequencing id> to matching record digest.
  // Whenever a record is uploaded and includes last record digest, this map
  // should have that digest already recorded. Only the first record in a
  // generation is uploaded without last record digest. "Optional" is set to
  // no-value if there was a gap record instead of a real one.
  using LastRecordDigestMap = std::map<
      std::pair<uint64_t /*generation id */, uint64_t /*sequencing id*/>,
      base::Optional<std::string /*digest*/>>;

  explicit MockUploadClient(LastRecordDigestMap* last_record_digest_map)
      : last_record_digest_map_(last_record_digest_map) {}

  void ProcessRecord(EncryptedRecord encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    WrappedRecord wrapped_record;
    ASSERT_TRUE(wrapped_record.ParseFromString(
        encrypted_record.encrypted_wrapped_record()));
    // Verify generation match.
    const auto& sequencing_information =
        encrypted_record.sequencing_information();
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(
              sequencing_information.sequencing_id(),
              Status(
                  error::DATA_LOSS,
                  base::StrCat(
                      {"Generation id mismatch, expected=",
                       base::NumberToString(generation_id_.value()), " actual=",
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
                sequencing_information.sequencing_id(),
                Status(error::DATA_LOSS, "Record digest mismatch")));
        return;
      }
      // Store record digest for the next record in sequence to verify.
      last_record_digest_map_->emplace(
          std::make_pair(sequencing_information.sequencing_id(),
                         sequencing_information.generation_id()),
          record_digest);
      // If last record digest is present, match it and validate.
      if (wrapped_record.has_last_record_digest()) {
        auto it = last_record_digest_map_->find(
            std::make_pair(sequencing_information.sequencing_id() - 1,
                           sequencing_information.generation_id()));
        if (it == last_record_digest_map_->end() ||
            (it->second.has_value() &&
             it->second.value() != wrapped_record.last_record_digest())) {
          std::move(processed_cb)
              .Run(UploadRecordFailure(
                  sequencing_information.sequencing_id(),
                  Status(error::DATA_LOSS, "Last record digest mismatch")));
          return;
        }
      }
    }

    std::move(processed_cb)
        .Run(UploadRecord(sequencing_information.sequencing_id(),
                          wrapped_record.record().data()));
  }

  void ProcessGap(SequencingInformation sequencing_information,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    // Verify generation match.
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(
              sequencing_information.sequencing_id(),
              Status(
                  error::DATA_LOSS,
                  base::StrCat(
                      {"Generation id mismatch, expected=",
                       base::NumberToString(generation_id_.value()), " actual=",
                       base::NumberToString(
                           sequencing_information.generation_id())}))));
      return;
    }
    if (!generation_id_.has_value()) {
      generation_id_ = sequencing_information.generation_id();
    }

    last_record_digest_map_->emplace(
        std::make_pair(sequencing_information.sequencing_id(),
                       sequencing_information.generation_id()),
        base::nullopt);

    std::move(processed_cb)
        .Run(UploadGap(sequencing_information.sequencing_id(), count));
  }

  void Completed(bool need_encryption_key, Status status) override {
    UploadComplete(need_encryption_key, status);
  }

  MOCK_METHOD(bool, UploadRecord, (uint64_t, base::StringPiece), (const));
  MOCK_METHOD(bool, UploadRecordFailure, (uint64_t, Status), (const));
  MOCK_METHOD(bool, UploadGap, (uint64_t, uint64_t), (const));
  MOCK_METHOD(void, UploadComplete, (bool, Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    explicit SetUp(MockUploadClient* client) : client_(client) {}
    ~SetUp() {
      EXPECT_CALL(*client_, UploadComplete(_, Eq(Status::StatusOK())))
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

    SetUp& RequiredGap(uint64_t sequence_number, uint64_t count) {
      EXPECT_CALL(*client_, UploadGap(Eq(sequence_number), Eq(count)))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& PossibleGap(uint64_t sequence_number, uint64_t count) {
      EXPECT_CALL(*client_, UploadGap(Eq(sequence_number), Eq(count)))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

    SetUp& Failure(uint64_t sequence_number, Status error) {
      EXPECT_CALL(*client_, UploadRecordFailure(Eq(sequence_number), Eq(error)))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
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
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    options_.set_directory(base::FilePath(location_.GetPath()))
        .set_single_file_size(GetParam());
  }

  void CreateStorageQueueOrDie(const QueueOptions& options) {
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

  void InjectFailures(std::initializer_list<uint64_t> seq_numbers) {
    storage_queue_->TestInjectBlockReadErrors(seq_numbers);
  }

  QueueOptions BuildStorageQueueOptionsImmediate() const {
    return QueueOptions(options_)
        .set_subdirectory(FILE_PATH_LITERAL("D1"))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"));
  }

  QueueOptions BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::TimeDelta::FromSeconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  QueueOptions BuildStorageQueueOptionsOnlyManual() const {
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
  StorageOptions options_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<StorageQueue> storage_queue_;

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all MockUploadClients created by test fixture.
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

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUploadWithFailures) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Inject simulated failures.
  InjectFailures({1});

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, data[0])
            .RequiredGap(1, 1)
            .Possible(2, data[2]);  // Depending on records binpacking
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

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingMetadataWriteMoreAndUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  storage_queue_.reset();

  // Delete all metadata files.
  base::FileEnumerator dir_enum(
      options.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".*")}));
  base::FilePath full_name;
  while (full_name = dir_enum.Next(), !full_name.empty()) {
    base::DeleteFile(full_name);
  }

  // Reopen, starting a new generation.
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations. Previous data is all lost.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Required(0, more_data[0])
            .Required(1, more_data[1])
            .Required(2, more_data[2]);
      }));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingDataWriteMoreAndUpload) {
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(data[0]);
  WriteStringOrDie(data[1]);
  WriteStringOrDie(data[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  storage_queue_.reset();

  // Reopen with the same generation and sequencing information.
  CreateStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Delete the first data file.
  base::FilePath full_name = options.directory().Append(
      base::StrCat({options.file_prefix(), FILE_PATH_LITERAL(".0")}));
  base::DeleteFile(full_name);

  // Write more data.
  WriteStringOrDie(more_data[0]);
  WriteStringOrDie(more_data[1]);
  WriteStringOrDie(more_data[2]);

  // Set uploader expectations. Previous data is all lost.
  // The expected results depend on the test configuration.
  switch (options.single_file_size()) {
    case 1:  // single record in file - deletion killed the first record
      EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
          .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(mock_upload_client)
                .PossibleGap(0, 1)
                .Required(1, data[1])
                .Required(2, data[2])
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));
      break;
    case 256:  // two records in file - deletion killed the first two records.
      EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
          .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(mock_upload_client)
                .PossibleGap(0, 2)
                .Failure(
                    2, Status(error::DATA_LOSS, "Last record digest mismatch"))
                .Required(3, more_data[0])
                .Required(4, more_data[1])
                .Required(5, more_data[2]);
          }));
      break;
    default:  // UNlimited file size - deletion above killed all the data.
      EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
          .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(mock_upload_client).PossibleGap(0, 1);
          }));
  }

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

TEST_P(StorageQueueTest,
       WriteAndRepeatedlyUploadWithConfirmationsAndReopenWithFailures) {
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

  // Inject simulated failures.
  InjectFailures({4, 5});

  // Set uploader expectations.
  EXPECT_CALL(set_mock_uploader_expectations_, Call(NotNull()))
      .WillOnce(Invoke([](MockUploadClient* mock_upload_client) {
        MockUploadClient::SetUp(mock_upload_client)
            .Possible(0, data[0])
            .Possible(1, data[1])
            .Required(2, data[2])
            .Required(3, more_data[0])
            // Gap may be 2 records at once or 2 gaps 1 record each.
            .PossibleGap(4, 2)
            .PossibleGap(4, 1)
            .PossibleGap(5, 1);
      }));
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*seq_number=*/2);

  // Reset simulated failures.
  InjectFailures({});

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
                                         256 /* two records in file */,
                                         1 /* single record in file */));

// TODO(b/157943006): Additional tests:
// 1) Options object with a bad path.
// 2) Have bad prefix files in the directory.
// 3) Attempt to create file with duplicated file extension.
// 4) Disk and memory limit exceeded.
// 5) Other negative tests.

}  // namespace
}  // namespace reporting
