// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

class AggregatedJournalTest : public testing::Test {
 public:
  AggregatedJournalTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~AggregatedJournalTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
  }

  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
};

class MockJournalObserver : public AggregatedJournal::Observer {
 public:
  explicit MockJournalObserver(AggregatedJournal& journal)
      : journal_(journal.GetSafeRef()) {
    journal.AddObserver(this);
  }
  ~MockJournalObserver() override { journal_->RemoveObserver(this); }

  MOCK_METHOD(void,
              WillAddJournalEntry,
              (const AggregatedJournal::Entry& entry),
              (override));

 private:
  base::SafeRef<AggregatedJournal> journal_;
};

TEST_F(AggregatedJournalTest, AddObserver) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  MockJournalObserver observer(journal);
  EXPECT_CALL(observer, WillAddJournalEntry(testing::_)).Times(1);

  journal.Log(GURL(), TaskId(0), "Test", "Nothing");
}

}  // namespace

}  // namespace actor
