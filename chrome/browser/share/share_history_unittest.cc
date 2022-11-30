// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_history.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kTarget0Name = "baz";
const char* kTarget1Name = "quxx";

int DaysSinceUnixEpoch() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

sharing::mojom::ShareHistory BuildTestProto() {
  sharing::mojom::ShareHistory proto;

  {
    auto* today = proto.mutable_day_histories()->Add();
    today->set_day(DaysSinceUnixEpoch());

    auto* baz = today->mutable_target_histories()->Add();
    baz->mutable_target()->set_component_name(kTarget0Name);
    baz->set_count(4);

    auto* quxx = today->mutable_target_histories()->Add();
    quxx->mutable_target()->set_component_name(kTarget1Name);
    quxx->set_count(2);
  }

  {
    auto* yesterday = proto.mutable_day_histories()->Add();
    yesterday->set_day(DaysSinceUnixEpoch() - 1);

    auto* baz = yesterday->mutable_target_histories()->Add();
    baz->mutable_target()->set_component_name(kTarget0Name);
    baz->set_count(1);
  }

  {
    // An old entry that will be expired when the history is loaded from the
    // backing DB.
    auto* long_ago = proto.mutable_day_histories()->Add();
    long_ago->set_day(DaysSinceUnixEpoch() - 365);

    auto* foo = long_ago->mutable_target_histories()->Add();
    foo->mutable_target()->set_component_name(kTarget0Name);
    foo->set_count(2);
  }

  return proto;
}

}  // namespace

namespace sharing {

// Fixture for tests that test the behavior of ShareHistory. These tests use a
// fake LevelDB instance so that they don't need to touch disk and have tight
// control over when callbacks are delivered, etc. These tests are deliberately
// as close as possible to the real async behavior of the production code, so
// database init happens asynchronously and so on.
class ShareHistoryTest : public testing::Test {
 public:
  using FakeDB = leveldb_proto::test::FakeDB<mojom::ShareHistory>;

  ShareHistoryTest() {
    // These tests often want to place events/history "in the past", but the
    // mock time source starts at the epoch, which breaks a bunch of those
    // computations; give ourselves some margin to work with here.
    environment_.FastForwardBy(base::Days(7));
    Init();
  }

  void Init(bool do_default_init = true) {
    auto backing_db = std::make_unique<FakeDB>(&backing_entries_);
    backing_db_ = backing_db.get();

    db_ = std::make_unique<ShareHistory>(
        &profile_, base::WrapUnique(backing_db.release()));

    if (do_default_init) {
      backing_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
      backing_db_->GetCallback(true);
    }
  }

  // This method is a synchronous wrapper for GetFlatShareHistory() for test
  // ergonomics. Note that if the test has cancelled default init and not posted
  // its own init, this can deadlock!
  std::vector<ShareHistory::Target> GetFlatShareHistory(int window = -1) {
    base::RunLoop loop;
    std::vector<ShareHistory::Target> out_result;
    db()->GetFlatShareHistory(
        base::BindLambdaForTesting(
            [&](std::vector<ShareHistory::Target> result) {
              out_result.swap(result);
              loop.Quit();
            }),
        window);
    loop.Run();
    return out_result;
  }

  std::unique_ptr<mojom::ShareHistory> GetBackingDbProto() {
    base::RunLoop loop;
    std::unique_ptr<mojom::ShareHistory> result;
    backing_db()->GetEntry(
        "share_history",
        base::BindLambdaForTesting(
            [&](bool ok, std::unique_ptr<mojom::ShareHistory> res) {
              ASSERT_TRUE(ok);
              result = std::move(res);
              loop.Quit();
            }));
    backing_db()->GetCallback(true);
    loop.Run();
    return result;
  }

  TestingProfile* profile() { return &profile_; }
  ShareHistory* db() { return db_.get(); }

  leveldb_proto::test::FakeDB<mojom::ShareHistory>* backing_db() {
    return backing_db_;
  }

  leveldb_proto::test::FakeDB<mojom::ShareHistory>::EntryMap*
  backing_entries() {
    return &backing_entries_;
  }

 private:
  content::BrowserTaskEnvironment environment_{
      // This set of tests must use a mock time source. If they don't, and a
      // test happens to run across UTC midnight, the day can change mid-test,
      // causing surprising results.
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<ShareHistory> db_;
  leveldb_proto::test::FakeDB<mojom::ShareHistory>::EntryMap backing_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<mojom::ShareHistory>> backing_db_ =
      nullptr;
  base::CancelableOnceClosure backing_init_callback_;
};

TEST_F(ShareHistoryTest, CreateAndInitializeEmpty) {
  auto result = GetFlatShareHistory();
  EXPECT_EQ(result.size(), 0U);
}

TEST_F(ShareHistoryTest, AddInMemory) {
  db()->AddShareEntry("foo");
  db()->AddShareEntry("bar");
  db()->AddShareEntry("foo");

  backing_db()->UpdateCallback(true);

  auto result = GetFlatShareHistory();
  EXPECT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].component_name, "foo");
  EXPECT_EQ(result[0].count, 2);
  EXPECT_EQ(result[1].component_name, "bar");
  EXPECT_EQ(result[1].count, 1);
}

TEST_F(ShareHistoryTest, AddsWrittenToBackingDb) {
  db()->AddShareEntry("foo");
  db()->AddShareEntry("bar");
  db()->AddShareEntry("foo");

  backing_db()->UpdateCallback(true);

  db()->AddShareEntry("foo");
  backing_db()->UpdateCallback(true);

  auto proto = GetBackingDbProto();
  ASSERT_EQ(proto->day_histories().size(), 1);
  auto today = proto->day_histories()[0];
  EXPECT_EQ(today.day(), DaysSinceUnixEpoch());
  ASSERT_EQ(today.target_histories().size(), 2);
  EXPECT_EQ(today.target_histories()[0].target().component_name(), "foo");
  EXPECT_EQ(today.target_histories()[0].count(), 3);
  EXPECT_EQ(today.target_histories()[1].target().component_name(), "bar");
  EXPECT_EQ(today.target_histories()[1].count(), 1);
}

TEST_F(ShareHistoryTest, BackingDbIsLoaded) {
  // After rewriting backing_entries(), to simulate having stored data on disk,
  // reinitialize the database to cause it to reread the backing DB.
  (*backing_entries())["share_history"] = BuildTestProto();
  Init();

  auto result = GetFlatShareHistory();
  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].component_name, kTarget0Name);
  EXPECT_EQ(result[0].count, 5);
  EXPECT_EQ(result[1].component_name, kTarget1Name);
  EXPECT_EQ(result[1].count, 2);
}

TEST_F(ShareHistoryTest, BackingDbInitFailureStillRunsCallbacks) {
  (*backing_entries())["share_history"] = BuildTestProto();
  Init(false);
  backing_db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kError);

  auto result = GetFlatShareHistory();
  EXPECT_EQ(result.size(), 0U);
}

TEST_F(ShareHistoryTest, OffTheRecordProfileHasNoInstance) {
  Profile* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(), true);
  ShareHistory* db = ShareHistory::Get(otr_profile);
  ASSERT_FALSE(db);
}

TEST_F(ShareHistoryTest, ClearYesterdayOnly) {
  // After rewriting backing_entries(), to simulate having stored data on disk,
  // reinitialize the database to cause it to reread the backing DB.
  (*backing_entries())["share_history"] = BuildTestProto();
  Init();

  {
    auto result = GetFlatShareHistory();
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].component_name, kTarget0Name);
    EXPECT_EQ(result[0].count, 5);
    EXPECT_EQ(result[1].component_name, kTarget1Name);
    EXPECT_EQ(result[1].count, 2);
  }

  auto start_offset = base::Days(DaysSinceUnixEpoch() - 1);
  auto end_offset = start_offset + base::Seconds(3600);
  db()->Clear(base::Time::UnixEpoch() + start_offset,
              base::Time::UnixEpoch() + end_offset);

  {
    auto result = GetFlatShareHistory();
    ASSERT_EQ(result.size(), 2U);
    EXPECT_EQ(result[0].component_name, kTarget0Name);
    EXPECT_EQ(result[0].count, 4);
    EXPECT_EQ(result[1].component_name, kTarget1Name);
    EXPECT_EQ(result[1].count, 2);
  }
}

TEST_F(ShareHistoryTest, OldEntriesExpired) {
  (*backing_entries())["share_history"] = BuildTestProto();
  Init();

  auto result = GetFlatShareHistory();
  EXPECT_EQ(result[0].component_name, kTarget0Name);

  // There are 4 entries today, 1 day yesterday, and 2 entries a year ago; the
  // latter should be expired on load.
  EXPECT_EQ(result[0].count, 5);
}

}  // namespace sharing
