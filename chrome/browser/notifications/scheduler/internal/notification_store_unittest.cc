// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_store.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;
using Entries = notifications::NotificationStore::Entries;
using TestNotificationEntries =
    std::map<std::string, notifications::NotificationEntry>;
using DbEntries = std::vector<notifications::NotificationEntry>;
using DbEntriesPtr =
    std::unique_ptr<std::vector<notifications::NotificationEntry>>;

namespace notifications {
namespace {

const char kGuid[] = "1234";

class NotificationStoreTest : public testing::Test {
 public:
  NotificationStoreTest() : load_result_(false) {}
  NotificationStoreTest(const NotificationStoreTest&) = delete;
  NotificationStoreTest& operator=(const NotificationStoreTest&) = delete;
  ~NotificationStoreTest() override = default;

  void SetUp() override {}

 protected:
  void Init(const TestNotificationEntries& test_data, InitStatus status) {
    CreateTestProtos(test_data);
    auto db =
        std::make_unique<FakeDB<proto::NotificationEntry, NotificationEntry>>(
            &db_protos_);
    db_ = db.get();
    store_ = std::make_unique<NotificationStore>(std::move(db));
    store_->InitAndLoad(base::BindOnce(&NotificationStoreTest::OnEntriesLoaded,
                                       base::Unretained(this)));
    db_->InitStatusCallback(status);
  }

  bool load_result() const { return load_result_; }
  const Entries& loaded_entries() const { return loaded_entries_; }
  FakeDB<proto::NotificationEntry, NotificationEntry>* db() { return db_; }
  CollectionStore<NotificationEntry>* store() { return store_.get(); }

  void VerifyDataInDb(DbEntriesPtr expected) {
    db_->LoadEntries(base::BindOnce(
        [](DbEntriesPtr expected, bool success, DbEntriesPtr entries) {
          EXPECT_TRUE(success);
          DCHECK(entries);
          DCHECK(expected);
          EXPECT_EQ(entries->size(), expected->size());
          for (size_t i = 0, size = entries->size(); i < size; ++i) {
            auto& entry = (*entries)[i];
            auto& expected_entry = (*expected)[i];
            EXPECT_EQ(entry, expected_entry)
                << " \n Output: " << test::DebugString(&entry)
                << " \n Expected: " << test::DebugString(&expected_entry);
          }
        },
        std::move(expected)));
    db_->LoadCallback(true);
  }

 private:
  // Push data into |db_|.
  void CreateTestProtos(const TestNotificationEntries& test_data) {
    for (const auto& pair : test_data) {
      const auto& key = pair.first;
      auto entry = pair.second;
      proto::NotificationEntry proto;
      NotificationEntryToProto(&entry, &proto);
      db_protos_.emplace(key, proto);
    }
  }

  void OnEntriesLoaded(bool success, Entries entries) {
    load_result_ = success;
    loaded_entries_ = std::move(entries);
  }

  base::test::TaskEnvironment task_environment_;

  // Database test objects.
  raw_ptr<FakeDB<proto::NotificationEntry, NotificationEntry>,
          DanglingUntriaged>
      db_;
  std::map<std::string, proto::NotificationEntry> db_protos_;

  std::unique_ptr<CollectionStore<NotificationEntry>> store_;
  Entries loaded_entries_;
  bool load_result_;
};

// Verifies initialization with empty database.
TEST_F(NotificationStoreTest, Init) {
  Init(TestNotificationEntries(), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_entries().empty());
}

// Initialize non-empty database should success.
TEST_F(NotificationStoreTest, InitSuccessWithData) {
  auto test_data = TestNotificationEntries();
  NotificationEntry entry(SchedulerClientType::kTest2, kGuid);
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &entry.create_time);
  DCHECK(success);
  test_data.emplace(kGuid, entry);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(loaded_entries().size(), 1u);
  EXPECT_EQ(*loaded_entries().back(), entry);
}

// Failed database initialization will result in error.
TEST_F(NotificationStoreTest, InitFailed) {
  Init(TestNotificationEntries(), InitStatus::kCorrupt);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(loaded_entries().empty());
}

TEST_F(NotificationStoreTest, AddAndUpdate) {
  Init(TestNotificationEntries(), InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(loaded_entries().empty());

  NotificationEntry entry(SchedulerClientType::kTest2, kGuid);
  bool success =
      base::Time::FromString("04/25/20 01:00:00 AM", &entry.create_time);
  DCHECK(success);

  // Add data to the store and verify the database.
  store()->Add(kGuid, entry,
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  auto expected = std::make_unique<DbEntries>();
  expected->emplace_back(entry);
  VerifyDataInDb(std::move(expected));

  // Update and verified the new data.
  entry.notification_data.title = u"test_title";
  expected = std::make_unique<DbEntries>();
  expected->emplace_back(entry);
  store()->Update(kGuid, entry,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  VerifyDataInDb(std::move(expected));
}

TEST_F(NotificationStoreTest, Delete) {
  auto test_data = TestNotificationEntries();
  NotificationEntry entry(SchedulerClientType::kTest2, kGuid);
  test_data.emplace(kGuid, entry);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(loaded_entries().size(), 1u);

  // Delete the entry and verify data is deleted.
  store()->Delete(kGuid, base::DoNothing());
  db()->UpdateCallback(true);
  VerifyDataInDb(std::make_unique<DbEntries>());
}

}  // namespace
}  // namespace notifications
