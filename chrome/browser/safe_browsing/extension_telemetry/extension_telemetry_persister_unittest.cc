// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_persister.h"

#include <sstream>

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ExtensionTelemetryPersisterTest : public ::testing::Test {
 public:
  ExtensionTelemetryPersisterTest();
  void SetUp() override { ASSERT_NE(persister_, nullptr); }

  std::unique_ptr<safe_browsing::ExtensionTelemetryPersister> persister_;

  void CallbackHelper(std::string read, bool success) {
    read_string_ = read;
    success_ = success;
  }

  base::test::TaskEnvironment task_environment_;
  bool success_;
  std::string read_string_;
  base::WeakPtrFactory<ExtensionTelemetryPersisterTest> weak_factory_{this};
};

ExtensionTelemetryPersisterTest::ExtensionTelemetryPersisterTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  persister_ = std::make_unique<ExtensionTelemetryPersister>();
  persister_->PersisterInit();
  task_environment_.RunUntilIdle();
}

TEST_F(ExtensionTelemetryPersisterTest, WriteReadCheck) {
  std::string written_string = "Test String 1";
  persister_->WriteReport(written_string);
  task_environment_.RunUntilIdle();
  auto callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&ExtensionTelemetryPersisterTest::CallbackHelper,
                     weak_factory_.GetWeakPtr()));
  // Read report and check its contents.
  persister_->ReadReport(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(success_, true);
  EXPECT_EQ(written_string, read_string_);
  persister_->ClearPersistedFiles();
  task_environment_.RunUntilIdle();
}

TEST_F(ExtensionTelemetryPersisterTest, WritePastFullCacheCheck) {
  read_string_ = "No File was read";
  std::string written_string = "Test String 1";
  for (int i = 0; i < 12; i++) {
    persister_->WriteReport(written_string);
    task_environment_.RunUntilIdle();
  }
  persister_->ClearPersistedFiles();
  task_environment_.RunUntilIdle();
  // After a clear no file should be there to read from.
  auto callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&ExtensionTelemetryPersisterTest::CallbackHelper,
                     weak_factory_.GetWeakPtr()));
  persister_->ReadReport(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_NE(written_string, read_string_);
  persister_->ClearPersistedFiles();
  task_environment_.RunUntilIdle();
}

TEST_F(ExtensionTelemetryPersisterTest, ReadFullCache) {
  // Fully load persisted cache.
  std::string written_string = "Test String 1";
  for (int i = 0; i < 10; i++) {
    persister_->WriteReport(written_string);
    task_environment_.RunUntilIdle();
  }
  written_string = "Test String 2";
  // Overwrite first 5 files.
  for (int i = 0; i < 5; i++) {
    persister_->WriteReport(written_string);
    task_environment_.RunUntilIdle();
  }
  //  Read should start at highest numbered file.
  for (int i = 0; i < 5; i++) {
    auto callback = base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&ExtensionTelemetryPersisterTest::CallbackHelper,
                       weak_factory_.GetWeakPtr()));
    persister_->ReadReport(std::move(callback));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(success_, true);
    EXPECT_EQ("Test String 1", read_string_);
  }
  // Files 0-4 should be different.
  for (int i = 0; i < 5; i++) {
    auto callback = base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&ExtensionTelemetryPersisterTest::CallbackHelper,
                       weak_factory_.GetWeakPtr()));
    persister_->ReadReport(std::move(callback));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(success_, true);
    EXPECT_EQ("Test String 2", read_string_);
  }
  auto callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&ExtensionTelemetryPersisterTest::CallbackHelper,
                     weak_factory_.GetWeakPtr()));
  persister_->ReadReport(std::move(callback));
  task_environment_.RunUntilIdle();
  // Last read should not happen as all files have been read.
  EXPECT_EQ(success_, false);
  persister_->ClearPersistedFiles();
  task_environment_.RunUntilIdle();
}
}  // namespace safe_browsing
