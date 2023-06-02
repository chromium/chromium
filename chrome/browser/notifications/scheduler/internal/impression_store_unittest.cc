// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/impression_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/proto/client_state.pb.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;
using Entries = notifications::ImpressionStore::Entries;
using DbEntries = std::vector<notifications::ClientState>;
using DbEntriesPtr = std::unique_ptr<std::vector<notifications::ClientState>>;
using TestClientStates = std::map<std::string, notifications::ClientState>;

namespace notifications {
namespace {

const char kClientStateKey[] = "guid_client_state_key1";
const ClientState kDefaultClientState;

// Test fixture to verify impression store.
class ImpressionStoreTest : public testing::Test {
 public:
  ImpressionStoreTest() : load_result_(false), db_(nullptr) {}
  ImpressionStoreTest(const ImpressionStoreTest&) = delete;
  ImpressionStoreTest& operator=(const ImpressionStoreTest&) = delete;
  ~ImpressionStoreTest() override = default;

  void SetUp() override {}

 protected:
  // Initialize the store with test data.
  void Init(const TestClientStates& test_data, InitStatus status) {
    CreateTestProto(test_data);

    auto db =
        std::make_unique<FakeDB<proto::ClientState, ClientState>>(&db_entries_);
    db_ = db.get();
    store_ = std::make_unique<ImpressionStore>(std::move(db));
    store_->InitAndLoad(base::BindOnce(&ImpressionStoreTest::OnEntriesLoaded,
                                       base::Unretained(this)));
    db_->InitStatusCallback(status);
  }

  bool load_result() const { return load_result_; }
  const Entries& loaded_entries() const { return loaded_entries_; }
  FakeDB<proto::ClientState, ClientState>* db() { return db_; }

  CollectionStore<ClientState>* store() { return store_.get(); }

  void OnEntriesLoaded(bool success, Entries entries) {
    loaded_entries_ = std::move(entries);
    load_result_ = success;
  }

  // Verifies the entries in the db is |expected|.
  void VerifyDataInDb(DbEntriesPtr expected) {
    db_->LoadEntries(base::BindOnce(
        [](DbEntriesPtr expected, bool success, DbEntriesPtr entries) {
          EXPECT_TRUE(success);
          DCHECK(entries);
          DCHECK(expected);
          EXPECT_EQ(entries->size(), expected->size());
          for (size_t i = 0, size = entries->size(); i < size; ++i) {
            EXPECT_EQ((*entries)[i], (*expected)[i]);
          }
        },
        std::move(expected)));
    db_->LoadCallback(true);
  }

 private:
  void CreateTestProto(const TestClientStates& client_states) {
    for (const auto& pair : client_states) {
      auto client_state(pair.second);
      auto key = pair.first;
      notifications::proto::ClientState proto;
      ClientStateToProto(&client_state, &proto);
      db_entries_.emplace(key, proto);
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::map<std::string, proto::ClientState> db_entries_;
  bool load_result_;
  Entries loaded_entries_;

  raw_ptr<FakeDB<proto::ClientState, ClientState>, DanglingUntriaged> db_;
  std::unique_ptr<CollectionStore<ClientState>> store_;
};

// Initializes an empty database should success.
TEST_F(ImpressionStoreTest, InitSuccessEmptyDb) {
  Init(TestClientStates(), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_entries().empty());
}

// Initialize non-empty database should success.
TEST_F(ImpressionStoreTest, InitSuccessWithData) {
  auto test_data = TestClientStates();
  test_data.emplace(kClientStateKey, kDefaultClientState);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(loaded_entries().size(), 1u);
  EXPECT_EQ(*loaded_entries().back(), kDefaultClientState);
}

// Failure when loading the data will result in error.
TEST_F(ImpressionStoreTest, InitSuccessLoadFailed) {
  Init(TestClientStates(), InitStatus::kOK);
  db()->LoadCallback(false);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(loaded_entries().empty());
}

// Failed database initialization will result in error.
TEST_F(ImpressionStoreTest, InitFailed) {
  Init(TestClientStates(), InitStatus::kCorrupt);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(loaded_entries().empty());
}

// Verifies adding data.
TEST_F(ImpressionStoreTest, Add) {
  Init(TestClientStates(), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_entries().empty());

  // Add data to the store.
  store()->Add(kClientStateKey, kDefaultClientState,
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // Verify the new data is in the database.
  auto expected = std::make_unique<DbEntries>();
  expected->emplace_back(kDefaultClientState);
  VerifyDataInDb(std::move(expected));
}

// Verifies failure when adding data will result in error.
TEST_F(ImpressionStoreTest, AddFailed) {
  Init(TestClientStates(), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_entries().empty());

  store()->Add(kClientStateKey, kDefaultClientState,
               base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->UpdateCallback(false);
}

// Verifies updating data.
TEST_F(ImpressionStoreTest, Update) {
  auto test_data = TestClientStates();
  test_data.emplace(kClientStateKey, kDefaultClientState);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);

  ClientState new_client_state;
  new_client_state.current_max_daily_show = 100;

  // Update the database.
  store()->Update(kClientStateKey, new_client_state,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // Verify the updated data is in the database.
  auto expected = std::make_unique<DbEntries>();
  expected->emplace_back(new_client_state);
  VerifyDataInDb(std::move(expected));
}

// Verifies deleting data.
TEST_F(ImpressionStoreTest, Delete) {
  auto test_data = TestClientStates();
  test_data.emplace(kClientStateKey, kDefaultClientState);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);

  // Delete the entry.
  store()->Delete(kClientStateKey,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Verify there is no data in the database.
  VerifyDataInDb(std::make_unique<DbEntries>());
}

}  // namespace
}  // namespace notifications
