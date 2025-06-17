// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/test/test_future.h"
#include "base/test/test_trace_processor_impl.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
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

TEST_F(AggregatedJournalTest, SerializerInMemory) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  AggregatedJournalInMemorySerializer serializer(journal);
  serializer.Init();
  auto begin_entry = journal.CreatePendingAsyncEntry(
      GURL("http://example.com"), TaskId(), "Begin", "Entry");
  journal.Log(GURL(), TaskId(0), "Test", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test2", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test3", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test4", "Nothing");
  begin_entry.reset();

  std::vector<uint8_t> result = serializer.Snapshot(1024 * 1024);
  std::vector<char> char_buffer(result.begin(), result.end());
  ASSERT_GT(result.size(), 0u);
  base::test::TestTraceProcessorImpl ttp;
  absl::Status status = ttp.ParseTrace(char_buffer);
  ASSERT_TRUE(status.ok()) << status.message();
}

TEST_F(AggregatedJournalTest, SerializerInFile) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  AggregatedJournalFileSerializer serializer(journal);

  base::test::TestFuture<bool> init_future;
  serializer.Init(temp_file.path(), init_future.GetCallback());
  EXPECT_TRUE(init_future.Get<bool>());

  auto begin_entry = journal.CreatePendingAsyncEntry(
      GURL("http://example.com"), TaskId(), "Begin", "Entry");
  journal.Log(GURL(), TaskId(0), "Test", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test2", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test3", "Nothing");
  journal.Log(GURL(), TaskId(0), "Test4", "Nothing");
  begin_entry.reset();

  base::test::TestFuture<void> shutdown_future;
  serializer.Shutdown(shutdown_future.GetCallback());
  EXPECT_TRUE(shutdown_future.Wait());

  std::optional<std::vector<uint8_t>> result =
      base::ReadFileToBytes(temp_file.path());
  ASSERT_TRUE(result.has_value());
  std::vector<char> char_buffer(result->begin(), result->end());
  base::test::TestTraceProcessorImpl ttp;
  absl::Status status = ttp.ParseTrace(char_buffer);
  ASSERT_TRUE(status.ok()) << status.message();
}

}  // namespace

}  // namespace actor
