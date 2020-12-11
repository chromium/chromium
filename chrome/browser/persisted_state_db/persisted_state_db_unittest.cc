// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/persisted_state_db.h"

#include <map>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {
const char kMockKey[] = "key";
const char kMockKeyPrefix[] = "k";
const std::vector<uint8_t> kMockValue = {0xfa, 0x5b, 0x4c, 0x12};
const std::vector<PersistedStateDB::KeyAndValue> kExpected = {
    {kMockKey, kMockValue}};
const std::vector<PersistedStateDB::KeyAndValue> kEmptyExpected = {};
}  // namespace

class PersistedStateDBTest : public testing::Test {
 public:
  PersistedStateDBTest() = default;

  // Initialize the test database
  void InitDatabase() {
    InitDatabaseWithoutCallback();
    MockInitCallback(content_db_, leveldb_proto::Enums::InitStatus::kOK);
  }

  void InitDatabaseWithoutCallback() {
    auto storage_db = std::make_unique<leveldb_proto::test::FakeDB<
        persisted_state_db::PersistedStateContentProto>>(&content_db_storage_);
    content_db_ = storage_db.get();
    persisted_state_db_ = base::WrapUnique(new PersistedStateDB(
        std::move(storage_db),
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE})));
  }

  // Wait for all tasks to be cleared off the queue
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void MockInitCallback(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      leveldb_proto::Enums::InitStatus status) {
    storage_db->InitStatusCallback(status);
    RunUntilIdle();
  }

  void MockInsertCallback(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool result) {
    storage_db->UpdateCallback(result);
    RunUntilIdle();
  }

  void MockLoadCallback(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->LoadCallback(res);
    RunUntilIdle();
  }

  void MockDeleteCallback(
      leveldb_proto::test::FakeDB<
          persisted_state_db::PersistedStateContentProto>* storage_db,
      bool res) {
    storage_db->UpdateCallback(res);
    RunUntilIdle();
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluation(base::OnceClosure closure,
                     std::vector<PersistedStateDB::KeyAndValue> expected,
                     bool result,
                     std::vector<PersistedStateDB::KeyAndValue> found) {
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second, expected[i].second);
    }
    std::move(closure).Run();
  }

  PersistedStateDB* persisted_state_db() { return persisted_state_db_.get(); }
  leveldb_proto::test::FakeDB<persisted_state_db::PersistedStateContentProto>*
  content_db() {
    return content_db_;
  }
  std::vector<base::OnceClosure>& deferred_operations() {
    return persisted_state_db()->deferred_operations_;
  }
  bool InitStatusUnknown() { return persisted_state_db()->InitStatusUnknown(); }
  bool FailedToInit() { return persisted_state_db()->FailedToInit(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::map<std::string, persisted_state_db::PersistedStateContentProto>
      content_db_storage_;
  leveldb_proto::test::FakeDB<persisted_state_db::PersistedStateContentProto>*
      content_db_;
  std::unique_ptr<PersistedStateDB> persisted_state_db_;

  DISALLOW_COPY_AND_ASSIGN(PersistedStateDBTest);
};

TEST_F(PersistedStateDBTest, TestInit) {
  InitDatabase();
  EXPECT_EQ(false, FailedToInit());
}

TEST_F(PersistedStateDBTest, TestKeyInsertionSucceeded) {
  InitDatabase();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[1].QuitClosure(), kExpected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(PersistedStateDBTest, TestKeyInsertionFailed) {
  InitDatabase();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  MockInsertCallback(content_db(), false);
  run_loop[0].Run();
  std::vector<PersistedStateDB::KeyAndValue> expected;
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(PersistedStateDBTest, TestKeyInsertionPrefix) {
  InitDatabase();
  base::RunLoop run_loop[2];
  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  std::vector<PersistedStateDB::KeyAndValue> expected;
  expected.emplace_back(kMockKey, kMockValue);
  persisted_state_db()->LoadContent(
      kMockKeyPrefix, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                                     base::Unretained(this),
                                     run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();
}

TEST_F(PersistedStateDBTest, TestDelete) {
  InitDatabase();
  base::RunLoop run_loop[4];
  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  MockInsertCallback(content_db(), true);
  run_loop[0].Run();
  std::vector<PersistedStateDB::KeyAndValue> expected;
  expected.emplace_back(kMockKey, kMockValue);
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[1].QuitClosure(), expected));
  MockLoadCallback(content_db(), true);
  run_loop[1].Run();

  persisted_state_db()->DeleteContent(
      kMockKey,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  MockDeleteCallback(content_db(), true);
  run_loop[2].Run();

  std::vector<PersistedStateDB::KeyAndValue> expected_after_delete;
  persisted_state_db()->LoadContent(
      kMockKey,
      base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(),
                     expected_after_delete));
  MockLoadCallback(content_db(), true);
  run_loop[3].Run();
}

TEST_F(PersistedStateDBTest, TestDeferredOperations) {
  InitDatabaseWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[4];

  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[1].QuitClosure(), kExpected));
  EXPECT_EQ(2u, deferred_operations().size());

  content_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  EXPECT_EQ(false, FailedToInit());

  MockInsertCallback(content_db(), true);
  MockLoadCallback(content_db(), true);
  run_loop[0].Run();
  run_loop[1].Run();
  EXPECT_EQ(0u, deferred_operations().size());

  persisted_state_db()->DeleteContent(
      kMockKey,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  EXPECT_EQ(0u, deferred_operations().size());
  MockDeleteCallback(content_db(), true);

  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[3].QuitClosure(), kEmptyExpected));
  EXPECT_EQ(0u, deferred_operations().size());
  MockLoadCallback(content_db(), true);
  run_loop[3].Run();
}

TEST_F(PersistedStateDBTest, TestInitializationFailure) {
  InitDatabaseWithoutCallback();
  RunUntilIdle();
  EXPECT_EQ(true, InitStatusUnknown());
  base::RunLoop run_loop[6];

  // Do some operations before database status is known
  persisted_state_db()->InsertContent(
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[1].QuitClosure(), kEmptyExpected));
  persisted_state_db()->DeleteContent(
      kMockKey,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
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
      kMockKey, kMockValue,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(), false));
  persisted_state_db()->LoadContent(
      kMockKey, base::BindOnce(&PersistedStateDBTest::GetEvaluation,
                               base::Unretained(this),
                               run_loop[4].QuitClosure(), kEmptyExpected));
  persisted_state_db()->DeleteContent(
      kMockKey,
      base::BindOnce(&PersistedStateDBTest::OperationEvaluation,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));

  // Operations should have returned immediately as database was initialization
  // resulted in an error
  EXPECT_EQ(0u, deferred_operations().size());
  for (int i = 3; i < 6; i++) {
    run_loop[i].Run();
  }
}
