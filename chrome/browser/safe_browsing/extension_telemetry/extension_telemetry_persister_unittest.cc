// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"

#include <sstream>

#include "base/functional/concurrent_closures.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using WriteReportTrigger = ExtensionTelemetryPersister::WriteReportTrigger;

}  // namespace

class ExtensionTelemetryPersisterTest : public ::testing::Test {
 public:
  ExtensionTelemetryPersisterTest();
  void SetUp() override { ASSERT_EQ(persister_.is_null(), false); }

  void TearDown() override {
    persister_.SynchronouslyResetForTest();
    testing::Test::TearDown();
  }

  base::SequenceBound<safe_browsing::ExtensionTelemetryPersister> persister_;
  int kMaxNumFilesPersisted = 10;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::HistogramTester histogram_tester_;
  base::WeakPtrFactory<ExtensionTelemetryPersisterTest> weak_factory_{this};
};

ExtensionTelemetryPersisterTest::ExtensionTelemetryPersisterTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  persister_ = base::SequenceBound<ExtensionTelemetryPersister>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      kMaxNumFilesPersisted, profile_.GetPath());
  persister_.AsyncCall(&ExtensionTelemetryPersister::PersisterInit);
}

TEST_F(ExtensionTelemetryPersisterTest, WriteReadCheck) {
  std::string written_string = "Test String 1";
  persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  base::test::TestFuture<std::string> future;
  persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
      .Then(future.GetCallback());
  EXPECT_EQ(written_string, future.Get());
}

TEST_F(ExtensionTelemetryPersisterTest, WritePastFullCacheCheck) {
  std::string written_string = "Test String 1";
  for (int i = 0; i < 12; i++) {
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  }
  persister_.AsyncCall(&ExtensionTelemetryPersister::ClearPersistedFiles);
  // After a clear no file should be there to read from.
  base::test::TestFuture<std::string> future;
  persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
      .Then(future.GetCallback());
  EXPECT_NE(written_string, future.Get());
}

TEST_F(ExtensionTelemetryPersisterTest, ReadFullCache) {
  // Fully load persisted cache.
  std::string written_string = "Test String 1";
  for (int i = 0; i < 10; i++) {
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  }
  written_string = "Test String 2";
  // Overwrite first 5 files.
  for (int i = 0; i < 5; i++) {
    persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
        .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  }
  //  Read should start at highest numbered file.
  for (int i = 0; i < 5; i++) {
    base::test::TestFuture<std::string> future;
    persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
        .Then(future.GetCallback());
    EXPECT_EQ("Test String 1", future.Get());
  }
  // Files 0-4 should be different.
  for (int i = 0; i < 5; i++) {
    base::test::TestFuture<std::string> future;
    persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
        .Then(future.GetCallback());
    EXPECT_EQ("Test String 2", future.Get());
  }
  base::test::TestFuture<std::string> future;
  persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
      .Then(future.GetCallback());
  // Last read should not happen as all files have been read.
  EXPECT_EQ("", future.Get());
}

TEST_F(ExtensionTelemetryPersisterTest, MultiProfile) {
  TestingProfile profile_2;
  base::SequenceBound<safe_browsing::ExtensionTelemetryPersister> persister_2 =
      base::SequenceBound<ExtensionTelemetryPersister>(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
          kMaxNumFilesPersisted, profile_2.GetPath());
  persister_2.AsyncCall(&ExtensionTelemetryPersister::PersisterInit);
  // Perform a simple read write test on two separate profiles.
  std::string written_string = "Test String 1";
  std::string written_string_2 = "Test String 2";
  persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval);
  persister_2.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string_2, WriteReportTrigger::kAtWriteInterval);
  persister_2.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string_2, WriteReportTrigger::kAtWriteInterval);
  // Read through profile one persisted files
  for (int i = 0; i < 2; i++) {
    base::test::TestFuture<std::string> future;
    persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
        .Then(future.GetCallback());
    EXPECT_EQ(written_string, future.Get());
  }
  // Last file read should fail since two files were written per profile.
  base::test::TestFuture<std::string> future;
  persister_.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
      .Then(future.GetCallback());
  EXPECT_EQ("", future.Get());
  // Repeat process for profile 2.
  for (int i = 0; i < 2; i++) {
    base::test::TestFuture<std::string> future_2;
    persister_2.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
        .Then(future_2.GetCallback());
    EXPECT_EQ(written_string_2, future_2.Get());
  }
  base::test::TestFuture<std::string> future_2;
  persister_2.AsyncCall(&ExtensionTelemetryPersister::ReadReport)
      .Then(future_2.GetCallback());
  EXPECT_EQ("", future_2.Get());
}

TEST_F(ExtensionTelemetryPersisterTest, VerifyWriteResultHistograms) {
  std::string written_string = "Test String 1";

  base::ConcurrentClosures concurrent;
  persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string, WriteReportTrigger::kAtWriteInterval)
      .Then(concurrent.CreateClosure());
  persister_.AsyncCall(&ExtensionTelemetryPersister::WriteReport)
      .WithArgs(written_string, WriteReportTrigger::kAtShutdown)
      .Then(concurrent.CreateClosure());

  base::RunLoop run_loop;
  std::move(concurrent).Done(run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionPersister.WriteResult", true, 2);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionPersister.WriteResult.AtWriteInterval", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionPersister.WriteResult.AtShutdown", true, 1);
}

}  // namespace safe_browsing
