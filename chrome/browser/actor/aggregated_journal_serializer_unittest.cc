// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/actor/aggregated_journal_serializer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/tracing/test_trace_processor_impl.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/aggregated_journal_file_serializer.h"
#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/aggregated_journal.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

class AggregatedJournalSerializerTest : public testing::Test {
 public:
  AggregatedJournalSerializerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~AggregatedJournalSerializerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
  }

  TestingProfile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AggregatedJournalSerializerTest, SerializerInMemory) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  AggregatedJournalInMemorySerializer serializer(journal,
                                                 /*max_bytes=*/1024 * 1024);
  serializer.Init();
  auto begin_entry = journal.CreatePendingAsyncEntry(
      GURL("http://example.com"), TaskId(), MakeBrowserTrackUUID(TaskId()),
      "Begin", JournalDetailsBuilder().Add("details", "Entry").Build());
  journal.Log(GURL(), TaskId(0), "Test", /*details=*/{});
  journal.Log(GURL(), TaskId(0), "Test2", /*details=*/{});
  journal.Log(GURL(), TaskId(0), "Test3", /*details=*/{});
  journal.Log(GURL(), TaskId(0), "Test4", /*details=*/{});
  begin_entry.reset();

  std::vector<uint8_t> result = serializer.Snapshot();
  std::vector<char> char_buffer(result.begin(), result.end());
  ASSERT_GT(result.size(), 0u);
  base::test::TestTraceProcessorImpl ttp;
  absl::Status status = ttp.ParseTrace(char_buffer);
  ASSERT_TRUE(status.ok()) << status.message();
}

TEST_F(AggregatedJournalSerializerTest, SerializerInMemoryTooSmallBuffer) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  AggregatedJournalInMemorySerializer serializer(journal, /*max_bytes=*/8);
  serializer.Init();
  journal.Log(GURL(), TaskId(0), "Test",
              JournalDetailsBuilder().Add("details", "Nothing").Build());

  // Nothing will get logged because of the small buffer.
  std::vector<uint8_t> result = serializer.Snapshot();
  ASSERT_EQ(result.size(), 0u);
}

TEST_F(AggregatedJournalSerializerTest, SerializerInMemorySmallBuffer) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  AggregatedJournalInMemorySerializer serializer(journal, /*max_bytes=*/100);
  serializer.Init();
  for (size_t i = 0; i < 10; ++i) {
    journal.Log(GURL(), TaskId(0), "Test",
                JournalDetailsBuilder().Add("details", "Nothing").Build());
  }

  // We should something but at most 100 bytes.
  std::vector<uint8_t> result = serializer.Snapshot();
  ASSERT_LT(result.size(), 100u);
  ASSERT_GT(result.size(), 0u);
}

TEST_F(AggregatedJournalSerializerTest, SerializerInFile) {
  AggregatedJournal& journal = ActorKeyedService::Get(profile())->GetJournal();
  base::ScopedTempFile temp_file;
  ASSERT_TRUE(temp_file.Create());
  AggregatedJournalFileSerializer serializer(journal);

  base::test::TestFuture<bool> init_future;
  serializer.Init(temp_file.path(), init_future.GetCallback());
  EXPECT_TRUE(init_future.Get<bool>());

  auto begin_entry = journal.CreatePendingAsyncEntry(
      GURL("http://example.com"), TaskId(), journal.AllocateDynamicTrackUUID(),
      "Begin", JournalDetailsBuilder().Add("details", "Entry").Build());
  journal.Log(GURL(), TaskId(0), "Test",
              JournalDetailsBuilder().Add("details", "Nothing").Build());
  journal.Log(GURL(), TaskId(0), "Test2", /*details=*/{});
  journal.Log(GURL(), TaskId(0), "Test3", /*details=*/{});
  journal.Log(GURL(), TaskId(0), "Test4", /*details=*/{});
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
