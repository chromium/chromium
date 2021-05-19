// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/share_history.h"
#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    auto backing_db = std::make_unique<FakeDB>(&backing_entries_);
    backing_db_ = backing_db.get();

    db_ = std::make_unique<ShareHistory>(
        &profile_, base::WrapUnique(backing_db.release()));

    backing_init_callback_.Reset(base::BindOnce(
        [](ShareHistoryTest* test) {
          test->backing_db_->InitStatusCallback(
              leveldb_proto::Enums::InitStatus::kOK);
        },
        base::Unretained(this)));
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, backing_init_callback_.callback());
  }

  // By default, ShareHistoryTest runs start with a posted task that simulates
  // database initialization finishing successfully. Tests that need to fake
  // an init failure, or otherwise have tighter control over when init appears
  // to complete, can use this method to cancel the default init.
  void CancelDefaultInit() { backing_init_callback_.Cancel(); }

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
      // test
      // happens to run across UTC midnight, the day can change mid-test,
      // causing
      // surprising results.
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  std::unique_ptr<ShareHistory> db_;
  leveldb_proto::test::FakeDB<mojom::ShareHistory>::EntryMap backing_entries_;
  leveldb_proto::test::FakeDB<mojom::ShareHistory>* backing_db_ = nullptr;
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

  auto result = GetFlatShareHistory();
  EXPECT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].component_name, "foo");
  EXPECT_EQ(result[0].count, 2);
  EXPECT_EQ(result[1].component_name, "bar");
  EXPECT_EQ(result[1].count, 1);
}

// TODO(ellyjones): Writes to the backing leveldb are not yet implemented.
TEST_F(ShareHistoryTest, DISABLED_AddsWrittenToBackingDb) {
  db()->AddShareEntry("foo");
  db()->AddShareEntry("bar");
  db()->AddShareEntry("foo");
}

// TODO(ellyjones): reads from the backing leveldb is not yet implemented.
TEST_F(ShareHistoryTest, DISABLED_BackingDbIsLoaded) {
  // backing_entries_[key] = MakeAProto();
  auto result = GetFlatShareHistory();

  EXPECT_EQ(result.size(), 2U);
  // ...
}

}  // namespace sharing
