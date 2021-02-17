// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/profile_proto_db.h"

#include <map>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/persisted_state_db/profile_proto_db_test_proto.pb.h"

using testing::_;

namespace {
persisted_state_db::PersistedStateContentProto BuildProto(
    const char* protoKey,
    const std::vector<uint8_t> byteArray) {
  persisted_state_db::PersistedStateContentProto proto;
  proto.set_key(protoKey);
  proto.set_content_data(byteArray.data(), byteArray.size());
  return proto;
}

profile_proto_db::ProfileProtoDBTestProto BuildTestProto(const char* key,
                                                         const int32_t value) {
  profile_proto_db::ProfileProtoDBTestProto proto;
  proto.set_key(key);
  proto.set_b(value);
  return proto;
}

const char kMockKeyA[] = "A_key";
const char kMockKeyPrefixA[] = "A";
const std::vector<uint8_t> kMockValueArrayA = {0xfa, 0x5b, 0x4c, 0x12};
const persisted_state_db::PersistedStateContentProto kMockValueA =
    BuildProto(kMockKeyA, kMockValueArrayA);
const std::vector<
    ProfileProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedA = {{kMockKeyA, kMockValueA}};
const char kMockKeyB[] = "B_key";
const std::vector<uint8_t> kMockValueArrayB = {0x3c, 0x9f, 0x5e, 0x69};
const persisted_state_db::PersistedStateContentProto kMockValueB =
    BuildProto(kMockKeyB, kMockValueArrayB);
const std::vector<
    ProfileProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedB = {{kMockKeyB, kMockValueB}};
const std::vector<
    ProfileProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kExpectedAB = {{kMockKeyA, kMockValueA}, {kMockKeyB, kMockValueB}};
const std::vector<
    ProfileProtoDB<persisted_state_db::PersistedStateContentProto>::KeyAndValue>
    kEmptyExpected = {};
const profile_proto_db::ProfileProtoDBTestProto kTestProto =
    BuildTestProto(kMockKeyA, 42);
const std::vector<
    ProfileProtoDB<profile_proto_db::ProfileProtoDBTestProto>::KeyAndValue>
    kTestProtoExpected = {{kMockKeyA, kTestProto}};

}  // namespace

class ProfileProtoDBTest : public testing::Test {
 public:
  ProfileProtoDBTest() = default;
  ProfileProtoDBTest(const ProfileProtoDBTest&) = delete;
  ProfileProtoDBTest& operator=(const ProfileProtoDBTest&) = delete;

  // The following methods are specific to the database containing
  // persisted_state_db::PersistedStateContentProto. There is one test which
  // contains profile_proto_db::ProfileProtoDBTestProto to test the use case
  // of multiple ProfileProtoDB databases running at the same time.

  // Initialize the test database
  void InitPersistedStateDB() {
    InitPersistedStateDBWithoutCallback();
    MockInitCallbackPersistedStateDB(content_db_,
                                     leveldb_proto::Enums::InitStatus::kOK);
  }

  void InitPersistedStateDBWithoutCallback() {
    auto storage_db = std::make_unique<leveldb_proto::test::FakeDB<
        persisted_state_db::PersistedStateContentProto>>(&content_db_storage_);
    content_db_ = storage_db.get();
    persisted_state_db_ = base::WrapUnique(
        new ProfileProtoDB<persisted_state_db::PersistedStateContentProto>(
            std::move(storage_db),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE})));
  }

  void MockInitCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      leveldb_proto::Enums::InitStatus status) {
    storage_db->InitStatusCallback(status);
    RunUntilIdle();
  }

  void MockInsertCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool result) {
    storage_db->UpdateCallback(result);
    RunUntilIdle();
  }

  void MockLoadCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->LoadCallback(res);
    RunUntilIdle();
  }

  void MockDeleteCallbackPersistedStateDB(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->UpdateCallback(res);
    RunUntilIdle();
  }

  void GetEvaluationPersistedStateDB(
      base::OnceClosure closure,
      std::vector<ProfileProtoDB<
          persisted_state_db::PersistedStateContentProto>::KeyAndValue>
          expected,
      bool result,
      std::vector<ProfileProtoDB<
          persisted_state_db::PersistedStateContentProto>::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.content_data(),
                expected[i].second.content_data());
    }
    std::move(closure).Run();
  }

  // Common to both databases
  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  // Wait for all tasks to be cleared off the queue
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Specific to profile_proto_db::ProfileProtoDBTestProto database
  void InitTestProtoDB() {
    auto storage_db = std::make_unique<
        leveldb_proto::test::FakeDB<profile_proto_db::ProfileProtoDBTestProto>>(
        &test_content_db_storage_);
    test_content_db_ = storage_db.get();
    test_proto_db_ = base::WrapUnique(
        new ProfileProtoDB<profile_proto_db::ProfileProtoDBTestProto>(
            std::move(storage_db),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE})));
  }

  void GetTestEvaluationTestProtoDB(
      base::OnceClosure closure,
      std::vector<ProfileProtoDB<
          profile_proto_db::ProfileProtoDBTestProto>::KeyAndValue> expected,
      bool result,
      std::vector<ProfileProtoDB<
          profile_proto_db::ProfileProtoDBTestProto>::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.b(), expected[i].second.b());
    }
    std::move(closure).Run();
  }

  // For persisted_state_db::PersistedStateContentProto database
  ProfileProtoDB<persisted_state_db::PersistedStateContentProto>*
  persisted_state_db() {
    return persisted_state_db_.get();
  }
  leveldb_proto::test::FakeDB<persisted_state_db::PersistedStateContentProto>*
  content_db() {
    return content_db_;
  }

  std::vector<base::OnceClosure>& deferred_operations() {
    return persisted_state_db()->deferred_operations_;
  }

  bool InitStatusUnknown() { return persisted_state_db()->InitStatusUnknown(); }
  bool FailedToInit() { return persisted_state_db()->FailedToInit(); }

  std::map<std::string, persisted_state_db::PersistedStateContentProto>
      content_db_storage_;

  // For profile_proto_db::ProfileProtoDBTestProto database
  ProfileProtoDB<profile_proto_db::ProfileProtoDBTestProto>* test_proto_db() {
    return test_proto_db_.get();
  }
  leveldb_proto::test::FakeDB<profile_proto_db::ProfileProtoDBTestProto>*
  test_content_db() {
    return test_content_db_;
  }

  std::map<std::string, profile_proto_db::ProfileProtoDBTestProto>
      test_content_db_storage_;

 protected:
  leveldb_proto::test::FakeDB<profile_proto_db::ProfileProtoDBTestProto>*
      test_content_db_;

 private:
  base::test::TaskEnvironment task_environment_;

  // For persisted_state_db::PersistedStateContentProto database
  leveldb_proto::test::FakeDB<persisted_state_db::PersistedStateContentProto>*
      content_db_;
  std::unique_ptr<
      ProfileProtoDB<persisted_state_db::PersistedStateContentProto>>
      persisted_state_db_;

  // For profile_proto_db::ProfileProtoDBTestProto database
  std::unique_ptr<ProfileProtoDB<profile_proto_db::ProfileProtoDBTestProto>>
      test_proto_db_;
};

// Test an arbitrary proto - this ensures we can have two ProfilProtoDB
// databases running simultaneously.
TEST_F(ProfileProtoDBTest, TestArbitraryProto) {
  InitTestProtoDB();
  test_content_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  RunUntilIdle();
  base::RunLoop run_loop[2];
  test_proto_db()->InsertContent(
      kMockKeyA, kTestProto,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  test_content_db()->UpdateCallback(true);
  RunUntilIdle();
  run_loop[0].Run();
  test_proto_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetTestEvaluationTestProtoDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kTestProtoExpected));
  test_content_db()->LoadCallback(true);
  RunUntilIdle();
  run_loop[1].Run();
}

TEST_F(ProfileProtoDBTest, TestInit) {
  InitPersistedStateDB();
  EXPECT_EQ(false, FailedToInit());
}

TEST_F(ProfileProtoDBTest, TestKeyInsertionSucceeded) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(ProfileProtoDBTest, TestKeyInsertionFailed) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  MockInsertCallbackPersistedStateDB(content_db(), false);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(ProfileProtoDBTest, TestKeyInsertionPrefix) {
  InitPersistedStateDB();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
}

TEST_F(ProfileProtoDBTest, TestLoadOneEntry) {
  InitPersistedStateDB();
  base::RunLoop run_loop[4];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->InsertContent(
      kMockKeyB, kMockValueB,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->LoadOneEntry(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[2].QuitClosure(),
                     kExpectedA));
  content_db()->GetCallback(true);
  run_loop[2].Run();
  persisted_state_db()->LoadOneEntry(
      kMockKeyB,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kExpectedB));
  content_db()->GetCallback(true);
  run_loop[3].Run();
}

TEST_F(ProfileProtoDBTest, TestLoadAllEntries) {
  InitPersistedStateDB();
  base::RunLoop run_loop[3];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->InsertContent(
      kMockKeyB, kMockValueB,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->LoadAllEntries(base::BindOnce(
      &ProfileProtoDBTest::GetEvaluationPersistedStateDB,
      base::Unretained(this), run_loop[2].QuitClosure(), kExpectedAB));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[2].Run();
}

TEST_F(ProfileProtoDBTest, TestDeleteWithPrefix) {
  InitPersistedStateDB();
  base::RunLoop run_loop[4];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();

  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  MockDeleteCallbackPersistedStateDB(content_db(), true);
  run_loop[2].Run();

  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
}

TEST_F(ProfileProtoDBTest, TestDeleteOneEntry) {
  InitPersistedStateDB();
  base::RunLoop run_loop[6];
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[1].Run();
  persisted_state_db()->DeleteOneEntry(
      kMockKeyPrefixA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  MockDeleteCallbackPersistedStateDB(content_db(), false);
  run_loop[2].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kExpectedA));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
  persisted_state_db()->DeleteOneEntry(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  MockDeleteCallbackPersistedStateDB(content_db(), true);
  run_loop[4].Run();
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyPrefixA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[5].QuitClosure(),
                     kEmptyExpected));
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[5].Run();
}

TEST_F(ProfileProtoDBTest, TestDeferredOperations) {
  InitPersistedStateDBWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[4];

  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kExpectedA));
  EXPECT_EQ(2u, deferred_operations().size());

  content_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_EQ(false, FailedToInit());

  MockInsertCallbackPersistedStateDB(content_db(), true);
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[0].Run();
  run_loop[1].Run();
  EXPECT_EQ(0u, deferred_operations().size());

  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  EXPECT_EQ(0u, deferred_operations().size());
  MockDeleteCallbackPersistedStateDB(content_db(), true);

  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     kEmptyExpected));
  EXPECT_EQ(0u, deferred_operations().size());
  MockLoadCallbackPersistedStateDB(content_db(), true);
  run_loop[3].Run();
}

TEST_F(ProfileProtoDBTest, TestInitializationFailure) {
  InitPersistedStateDBWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[6];

  // Do some operations before database status is known
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     kEmptyExpected));
  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), false));
  EXPECT_EQ(3u, deferred_operations().size());

  // Error initializing database
  content_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);
  EXPECT_EQ(true, FailedToInit());
  for (int i = 0; i < 3; i++) {
    run_loop[i].Run();
  }

  // Check deferred_operations is flushed
  EXPECT_EQ(0u, deferred_operations().size());

  // More operations should just return false/null as the database
  // failed to initialize
  persisted_state_db()->InsertContent(
      kMockKeyA, kMockValueA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(), false));
  persisted_state_db()->LoadContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::GetEvaluationPersistedStateDB,
                     base::Unretained(this), run_loop[4].QuitClosure(),
                     kEmptyExpected));
  persisted_state_db()->DeleteContentWithPrefix(
      kMockKeyA,
      base::BindOnce(&ProfileProtoDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));

  // Operations should have returned immediately as database was initialization
  // resulted in an error
  EXPECT_EQ(0u, deferred_operations().size());
  for (int i = 3; i < 6; i++) {
    run_loop[i].Run();
  }
}
